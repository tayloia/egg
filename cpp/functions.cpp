#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

namespace {
  class FunctionCoroutineStackless;
}

class egg::yolk::EggProgramStackless {
protected:
  FunctionCoroutineStackless* parent; // back-pointer
  EggProgramStackless* next; // next in stack, may be null
  explicit EggProgramStackless(FunctionCoroutineStackless& parent); 
public:
  virtual ~EggProgramStackless() {}
  virtual egg::lang::Value resume(egg::yolk::EggProgramContext& context) = 0;
  template<typename T, typename... ARGS>
  T& push(ARGS&&... args) {
    // Use perfect forwarding to the constructor (automatically plumbed into the parent)
    return *new T(*this->parent, std::forward<ARGS>(args)...);
  }
  EggProgramStackless* pop();
};

namespace {
  class FunctionSignatureParameter : public egg::lang::IFunctionSignatureParameter {
  private:
    egg::lang::String name; // may be empty
    egg::lang::ITypeRef type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    FunctionSignatureParameter(const egg::lang::String& name, const egg::lang::ITypeRef& type, size_t position, Flags flags)
      : name(name), type(type), position(position), flags(flags) {
    }
    virtual egg::lang::String getName() const override {
      return this->name;
    }
    virtual egg::lang::ITypeRef getType() const override {
      return this->type;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
  };

  class GeneratorFunctionType : public egg::yolk::FunctionType {
    EGG_NO_COPY(GeneratorFunctionType);
  private:
    egg::lang::ITypeRef rettype;
  public:
    explicit GeneratorFunctionType(const egg::lang::ITypeRef& returnType)
      : FunctionType(egg::lang::String::Empty, returnType->unionWith(*egg::lang::Type::Void)),
        rettype(returnType) {
      // No name or parameters in the signature
      assert(!egg::lang::Bits::hasAnySet(returnType->getSimpleTypes(), egg::lang::Discriminator::Void));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // Format a string along the lines of '<rettype>...'
      egg::lang::StringBuilder sb;
      sb.add(this->rettype->toString(0), "...");
      return std::make_pair(sb.toUTF8(), 0);
    }
    virtual bool iterable(egg::lang::ITypeRef& type) const {
      // We are indeed iterable
      type = this->rettype;
      return true;
    }
  };

  class StacklessRoot : public egg::yolk::EggProgramStackless {
    EGG_NO_COPY(StacklessRoot);
  public:
    explicit StacklessRoot(FunctionCoroutineStackless& parent)
      : EggProgramStackless(parent) {
    }
    virtual egg::lang::Value resume(egg::yolk::EggProgramContext&) {
      // If the root element resumed, we've completed all the statements in the function definition block
      // Simulate 'return;' to say we've finished
      return egg::lang::Value::ReturnVoid;
    }
  };

  class StacklessBlock : public egg::yolk::EggProgramStackless {
    EGG_NO_COPY(StacklessBlock);
  protected:
    const std::vector<std::shared_ptr<egg::yolk::IEggProgramNode>>* statements;
    size_t progress;
  public:
    StacklessBlock(FunctionCoroutineStackless& parent, const std::vector<std::shared_ptr<egg::yolk::IEggProgramNode>>& statements)
      : EggProgramStackless(parent),
        statements(&statements),
        progress(0) {
    }
    virtual egg::lang::Value resume(egg::yolk::EggProgramContext& context) {
      while (this->progress < this->statements->size()) {
        auto& statement = (*this->statements)[this->progress++];
        assert(statement != nullptr);
        context.statement(*statement);
        auto retval = statement->coexecute(context, *this);
        if (!retval.is(egg::lang::Discriminator::Void)) {
          return retval;
        }
      }
      // Fallen off the end of the block
      auto* resumed = this->pop();
      assert(resumed != nullptr);
      return resumed->resume(context);
    }
  };

  class StacklessWhile : public egg::yolk::EggProgramStackless {
    EGG_NO_COPY(StacklessWhile);
  protected:
    std::shared_ptr<egg::yolk::IEggProgramNode> cond;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    StacklessWhile(FunctionCoroutineStackless& parent, const std::shared_ptr<egg::yolk::IEggProgramNode>& cond, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : EggProgramStackless(parent),
        cond(cond),
        block(block) {
    }
    virtual egg::lang::Value resume(egg::yolk::EggProgramContext& context) {
      auto retval = context.condition(*this->cond);
      if (retval.is(egg::lang::Discriminator::Bool)) {
        if (!retval.getBool()) {
          // Condition failed, leave the loop
          return this->pop()->resume(context);
        }
        return this->block->coexecute(context, *this);
      }
      return retval;
    }
  };

  class FunctionCoroutineStackless : public egg::yolk::FunctionCoroutine {
    EGG_NO_COPY(FunctionCoroutineStackless);
    friend class egg::yolk::EggProgramStackless;
  private:
    // We cannot use a std::stack for this as the destruction order of elements is undefined by the standard
    egg::yolk::EggProgramStackless* stack;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    explicit FunctionCoroutineStackless(const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : stack(nullptr),
        block(block) {
      assert(block != nullptr);
    }
    virtual ~FunctionCoroutineStackless() {
      while (this->stack != nullptr) {
        (void)this->stack->pop();
      }
    }
    virtual egg::lang::Value resume(egg::yolk::EggProgramContext& context) override {
      if (this->stack == nullptr) {
        // This is the first time through; push a root context
        auto* root = new StacklessRoot(*this);
        assert(this->stack == root);
        return this->block->coexecute(context, *root);
      }
      return this->stack->resume(context);
    }
  };
}

class egg::yolk::FunctionSignature : public egg::lang::IFunctionSignature {
  EGG_NO_COPY(FunctionSignature);
private:
  egg::lang::String name;
  egg::lang::ITypeRef returnType;
  std::vector<FunctionSignatureParameter> parameters;
public:
  FunctionSignature(const egg::lang::String& name, const egg::lang::ITypeRef& returnType)
    : name(name), returnType(returnType) {
  }
  void addSignatureParameter(const egg::lang::String& parameterName, const egg::lang::ITypeRef& parameterType, size_t position, FunctionSignatureParameter::Flags flags) {
    this->parameters.emplace_back(parameterName, parameterType, position, flags);
  }
  virtual egg::lang::String getFunctionName() const override {
    return this->name;
  }
  virtual egg::lang::ITypeRef getReturnType() const override {
    return this->returnType;
  }
  virtual size_t getParameterCount() const override {
    return this->parameters.size();
  }
  virtual const egg::lang::IFunctionSignatureParameter& getParameter(size_t index) const override {
    assert(index < this->parameters.size());
    return this->parameters[index];
  }
};

void egg::lang::IFunctionSignature::buildStringDefault(StringBuilder& sb, IFunctionSignature::Parts parts) const {
  // TODO better formatting of named/variadic etc.
  if (Bits::hasAnySet(parts, IFunctionSignature::Parts::ReturnType)) {
    // Use precedence zero to get any necessary parentheses
    sb.add(this->getReturnType()->toString(0));
  }
  if (Bits::hasAnySet(parts, IFunctionSignature::Parts::FunctionName)) {
    auto name = this->getFunctionName();
    if (!name.empty()) {
      sb.add(' ', name);
    }
  }
  if (Bits::hasAnySet(parts, IFunctionSignature::Parts::ParameterList)) {
    sb.add('(');
    auto n = this->getParameterCount();
    for (size_t i = 0; i < n; ++i) {
      if (i > 0) {
        sb.add(", ");
      }
      auto& parameter = this->getParameter(i);
      assert(parameter.getPosition() != SIZE_MAX);
      if (parameter.isVariadic()) {
        sb.add("...");
      } else {
        sb.add(parameter.getType()->toString());
        if (Bits::hasAnySet(parts, IFunctionSignature::Parts::ParameterNames)) {
          auto pname = parameter.getName();
          if (!pname.empty()) {
            sb.add(' ', pname);
          }
        }
        if (!parameter.isRequired()) {
          sb.add(" = null");
        }
      }
    }
    sb.add(')');
  }
}

egg::yolk::FunctionType::FunctionType(const egg::lang::String& name, const egg::lang::ITypeRef& returnType)
  : HardReferenceCounted(0),
    signature(std::make_unique<FunctionSignature>(name, returnType)) {
}

egg::yolk::FunctionType::~FunctionType() {
  // Must be in this source file due to incomplete types in the header
}

std::pair<std::string, int> egg::yolk::FunctionType::toStringPrecedence() const {
  // Do not include names in the signature
  egg::lang::StringBuilder sb;
  this->signature->buildStringDefault(sb, egg::lang::IFunctionSignature::Parts::NoNames);
  return std::make_pair(sb.toUTF8(), 0);
}

egg::yolk::FunctionType::AssignmentSuccess egg::yolk::FunctionType::canBeAssignedFrom(const IType& rtype) const {
  // We can assign if the signatures are the same or equal
  auto* rsig = rtype.callable();
  if (rsig == nullptr) {
    return egg::yolk::FunctionType::AssignmentSuccess::Never;
  }
  auto* lsig = this->signature.get();
  if (lsig == rsig) {
    return egg::yolk::FunctionType::AssignmentSuccess::Always;
  }
  // TODO fuzzy matching of signatures
  if (lsig->getParameterCount() != rsig->getParameterCount()) {
    return egg::yolk::FunctionType::AssignmentSuccess::Never;
  }
  return lsig->getReturnType()->canBeAssignedFrom(*rsig->getReturnType()); // TODO
}

const egg::lang::IFunctionSignature* egg::yolk::FunctionType::callable() const {
  return this->signature.get();
}

void egg::yolk::FunctionType::addParameter(const egg::lang::String& name, const egg::lang::ITypeRef& type, egg::lang::IFunctionSignatureParameter::Flags flags) {
  this->signature->addSignatureParameter(name, type, this->signature->getParameterCount(), flags);
}

egg::yolk::FunctionType* egg::yolk::FunctionType::createFunctionType(const egg::lang::String& name, const egg::lang::ITypeRef& returnType) {
  return new FunctionType(name, returnType);
}

egg::yolk::FunctionType* egg::yolk::FunctionType::createGeneratorType(const egg::lang::String& name, const egg::lang::ITypeRef& returnType) {
  // Convert the return type (e.g. 'int') into a generator function 'int..' aka '(void|int)()'
  return new FunctionType(name, egg::lang::ITypeRef::make<GeneratorFunctionType>(returnType));
}

egg::yolk::FunctionCoroutine* egg::yolk::FunctionCoroutine::create(const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  // Create a stackless block executor for generator coroutines
  return new FunctionCoroutineStackless(block);
}

egg::yolk::EggProgramStackless::EggProgramStackless(FunctionCoroutineStackless& parent)
  : parent(&parent),
   next(parent.stack) {
  // Plumb ourselves into the synthetic stack
  parent.stack = this;
}

egg::yolk::EggProgramStackless* egg::yolk::EggProgramStackless::pop() {
  // Remove the top element of the synthetic stack (should be us!)
  auto* top = this->parent->stack;
  assert(top != nullptr);
  assert(top == this);
  auto* result = top->next;
  this->parent->stack = result;
  delete top;
  return result;
}

egg::lang::Value egg::yolk::EggProgramContext::coexecuteBlock(EggProgramStackless& stackless, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Create a new context to execute the statements in order
  return stackless.push<StacklessBlock>(statements).resume(*this);
}

egg::lang::Value egg::yolk::EggProgramContext::coexecuteWhile(EggProgramStackless& stackless, const std::shared_ptr<egg::yolk::IEggProgramNode>& cond, const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  return stackless.push<StacklessWhile>(cond, block).resume(*this);
}

egg::lang::Value egg::yolk::EggProgramContext::coexecuteYield(EggProgramStackless&, const std::shared_ptr<IEggProgramNode>& value) {
  auto result = value->execute(*this).direct();
  if (!result.has(egg::lang::Discriminator::FlowControl)) {
    // Need to convert the result to a return flow control
    result.addFlowControl(egg::lang::Discriminator::Yield);
  }
  return result;
}
