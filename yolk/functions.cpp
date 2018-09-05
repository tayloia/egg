#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

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
  virtual egg::ovum::Variant resume(egg::yolk::EggProgramContext& context) = 0;
  template<typename T, typename... ARGS>
  T& push(egg::ovum::IAllocator& allocator, ARGS&&... args) {
    // Use perfect forwarding to the constructor (automatically plumbed into the parent)
    return *allocator.create<T>(0, *this->parent, std::forward<ARGS>(args)...);
  }
  EggProgramStackless* pop();
};

namespace {
  class FunctionSignatureParameter : public egg::ovum::IFunctionSignatureParameter {
  private:
    egg::ovum::String name; // may be empty
    egg::ovum::Type type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    FunctionSignatureParameter(const egg::ovum::String& name, const egg::ovum::Type& type, size_t position, Flags flags)
      : name(name), type(type), position(position), flags(flags) {
    }
    virtual egg::ovum::String getName() const override {
      return this->name;
    }
    virtual egg::ovum::Type getType() const override {
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
    egg::ovum::Type rettype;
  public:
    explicit GeneratorFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::Type& returnType)
      : FunctionType(allocator, egg::ovum::String(), egg::ovum::Type::makeUnion(allocator, *returnType, *egg::ovum::Type::Void)),
        rettype(returnType) {
      // No name or parameters in the signature
      assert(!egg::ovum::Bits::hasAnySet(returnType->getBasalTypes(), egg::ovum::BasalBits::Void));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // Format a string along the lines of '<rettype>...'
      return std::make_pair(this->rettype.toString(0).toUTF8() + "...", 0);
    }
    virtual egg::ovum::Type iterable() const {
      // We are indeed iterable
      return this->rettype;
    }
  };

  class StacklessRoot : public egg::yolk::EggProgramStackless {
    EGG_NO_COPY(StacklessRoot);
  public:
    explicit StacklessRoot(FunctionCoroutineStackless& parent)
      : EggProgramStackless(parent) {
    }
    virtual egg::ovum::Variant resume(egg::yolk::EggProgramContext&) {
      // If the root element resumed, we've completed all the statements in the function definition block
      // Simulate 'return;' to say we've finished
      return egg::ovum::Variant::ReturnVoid;
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
    virtual egg::ovum::Variant resume(egg::yolk::EggProgramContext& context) {
      while (this->progress < this->statements->size()) {
        auto& statement = (*this->statements)[this->progress++];
        assert(statement != nullptr);
        context.statement(*statement);
        auto retval = statement->coexecute(context, *this);
        if (retval.hasFlowControl()) {
          return retval;
        }
      }
      // Fallen off the end of the block
      auto* resumed = this->pop();
      assert(resumed != nullptr);
      return resumed->resume(context);
    }
  };

  class StacklessDo : public egg::yolk::EggProgramStackless {
    EGG_NO_COPY(StacklessDo);
  protected:
    std::shared_ptr<egg::yolk::IEggProgramNode> cond;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
    bool test;
  public:
    StacklessDo(FunctionCoroutineStackless& parent, const std::shared_ptr<egg::yolk::IEggProgramNode>& cond, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : EggProgramStackless(parent),
        cond(cond),
        block(block),
        test(false) {
      assert(block != nullptr);
    }
    virtual egg::ovum::Variant resume(egg::yolk::EggProgramContext& context) {
      for (;;) {
        if (!this->test) {
          this->test = true;
          auto retval = this->block->coexecute(context, *this);
          if (retval.hasFlowControl()) {
            // Probably an exception in the block
            return retval;
          }
        }
        if (this->test) {
          this->test = false;
          auto retval = context.condition(*this->cond);
          if (!retval.isBool()) {
            // Probably an exception in the condition evaluation
            return retval;
          }
          if (!retval.getBool()) {
            // Condition failed, leave the loop
            break;
          }
        }
      }
      return this->pop()->resume(context);
    }
  };

  class StacklessFor : public egg::yolk::EggProgramStackless {
    EGG_NO_COPY(StacklessFor);
  protected:
    std::shared_ptr<egg::yolk::IEggProgramNode> pre;
    std::shared_ptr<egg::yolk::IEggProgramNode> cond;
    std::shared_ptr<egg::yolk::IEggProgramNode> post;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
    bool started;
  public:
    StacklessFor(FunctionCoroutineStackless& parent, const std::shared_ptr<egg::yolk::IEggProgramNode>& pre, const std::shared_ptr<egg::yolk::IEggProgramNode>& cond, const std::shared_ptr<egg::yolk::IEggProgramNode>& post, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : EggProgramStackless(parent),
        pre(pre),
        cond(cond),
        post(post),
        block(block),
        started(false) {
      assert(block != nullptr);
    }
    virtual egg::ovum::Variant resume(egg::yolk::EggProgramContext& context) {
      // The pre/cond/post nodes are all simple; they cannot contain yields, so can be running stackfully
      egg::ovum::Variant retval;
      if (!this->started) {
        this->started = true;
        // Run 'pre'
        if (this->pre != nullptr) {
          retval = this->pre->execute(context);
          if (retval.hasFlowControl()) {
            // Probably an exception in the pre-loop statement
            return retval;
          }
        }
      }
      for (;;) {
        // Run 'cond'
        if (this->cond != nullptr) {
          retval = context.condition(*this->cond);
          if (!retval.isBool()) {
            // Probably an exception in the condition evaluation
            return retval;
          }
          if (!retval.getBool()) {
            // Condition failed, leave the loop
            break;
          }
        }
        // Run 'block'
        retval = this->block->coexecute(context, *this);
        if (retval.hasFlowControl()) {
          if (retval.is(egg::ovum::VariantBits::Break)) {
            // Explicit 'break'
            break;
          }
          if (!retval.is(egg::ovum::VariantBits::Continue)) {
            // Not explicit 'continue'
            return retval;
          }
        }
        // Run 'post'
        if (this->post != nullptr) {
          retval = this->post->execute(context);
          if (retval.hasFlowControl()) {
            // Probably an exception in the post-loop statement
            return retval;
          }
        }
      }
      return this->pop()->resume(context);
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
      assert(block != nullptr);
    }
    virtual egg::ovum::Variant resume(egg::yolk::EggProgramContext& context) {
      for (;;) {
        auto retval = context.condition(*this->cond);
        if (!retval.isBool()) {
          // Probably an exception in the condition
          return retval;
        }
        if (!retval.getBool()) {
          // Condition failed, leave the loop
          break;
        }
        retval = this->block->coexecute(context, *this);
        if (retval.hasFlowControl()) {
          if (retval.is(egg::ovum::VariantBits::Break)) {
            // Explicit 'break;
            break;
          }
          if (!retval.is(egg::ovum::VariantBits::Continue)) {
            // Not explicit 'continue;
            return retval;
          }
        }
      }
      return this->pop()->resume(context);
    }
  };

  class FunctionCoroutineStackless : public egg::ovum::HardReferenceCounted<egg::yolk::FunctionCoroutine> {
    EGG_NO_COPY(FunctionCoroutineStackless);
    friend class egg::yolk::EggProgramStackless;
  private:
    // We cannot use a std::stack for this as the destruction order of elements is undefined by the standard
    egg::yolk::EggProgramStackless* stack;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    FunctionCoroutineStackless(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : HardReferenceCounted(allocator, 0),
        stack(nullptr),
        block(block) {
      assert(block != nullptr);
    }
    virtual ~FunctionCoroutineStackless() {
      while (this->stack != nullptr) {
        (void)this->stack->pop();
      }
    }
    virtual egg::ovum::Variant resume(egg::yolk::EggProgramContext& context) override {
      if (this->stack == nullptr) {
        // This is the first time through; push a root context
        auto* root = allocator.create<StacklessRoot>(0, *this);
        assert(this->stack == root);
        return this->block->coexecute(context, *root);
      }
      return this->stack->resume(context);
    }
  };
}

class egg::yolk::FunctionSignature : public egg::ovum::IFunctionSignature {
  EGG_NO_COPY(FunctionSignature);
private:
  egg::ovum::String name;
  egg::ovum::Type returnType;
  std::vector<FunctionSignatureParameter> parameters;
public:
  FunctionSignature(const egg::ovum::String& name, const egg::ovum::Type& returnType)
    : name(name), returnType(returnType) {
  }
  void addSignatureParameter(const egg::ovum::String& parameterName, const egg::ovum::Type& parameterType, size_t position, FunctionSignatureParameter::Flags flags) {
    this->parameters.emplace_back(parameterName, parameterType, position, flags);
  }
  virtual egg::ovum::String getFunctionName() const override {
    return this->name;
  }
  virtual egg::ovum::Type getReturnType() const override {
    return this->returnType;
  }
  virtual size_t getParameterCount() const override {
    return this->parameters.size();
  }
  virtual const egg::ovum::IFunctionSignatureParameter& getParameter(size_t index) const override {
    assert(index < this->parameters.size());
    return this->parameters[index];
  }
};

egg::yolk::FunctionType::FunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType)
  : HardReferenceCounted(allocator, 0),
    signature(std::make_unique<FunctionSignature>(name, returnType)) {
}

egg::yolk::FunctionType::~FunctionType() {
  // Must be in this source file due to incomplete types in the header
}

std::pair<std::string, int> egg::yolk::FunctionType::toStringPrecedence() const {
  // Do not include names in the signature
  auto sig = egg::ovum::Function::signatureToString(*this->signature, egg::ovum::Function::Parts::NoNames);
  return std::make_pair(sig.toUTF8(), 0);
}

egg::ovum::Node egg::yolk::FunctionType::compile(egg::ovum::IAllocator& memallocator, const egg::ovum::NodeLocation& location) const {
  return egg::ovum::NodeFactory::createFunctionType(memallocator, location, *this->signature);
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

const egg::ovum::IFunctionSignature* egg::yolk::FunctionType::callable() const {
  return this->signature.get();
}

void egg::yolk::FunctionType::addParameter(const egg::ovum::String& name, const egg::ovum::Type& type, egg::ovum::IFunctionSignatureParameter::Flags flags) {
  this->signature->addSignatureParameter(name, type, this->signature->getParameterCount(), flags);
}

egg::yolk::FunctionType* egg::yolk::FunctionType::createFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType) {
  return allocator.create<FunctionType>(0, allocator, name, returnType);
}

egg::yolk::FunctionType* egg::yolk::FunctionType::createGeneratorType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType) {
  // Convert the return type (e.g. 'int') into a generator function 'int..' aka '(void|int)()'
  return allocator.create<FunctionType>(0, allocator, name, allocator.make<GeneratorFunctionType, egg::ovum::Type>(returnType));
}

egg::ovum::HardPtr<egg::yolk::FunctionCoroutine> egg::yolk::FunctionCoroutine::create(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  // Create a stackless block executor for generator coroutines
  return allocator.make<FunctionCoroutineStackless>(block);
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
  top->parent->allocator.destroy(top);
  return result;
}

egg::ovum::Variant egg::yolk::EggProgramContext::coexecuteBlock(EggProgramStackless& stackless, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Create a new context to execute the statements in order
  return stackless.push<StacklessBlock>(this->allocator, statements).resume(*this);
}

egg::ovum::Variant egg::yolk::EggProgramContext::coexecuteDo(EggProgramStackless& stackless, const std::shared_ptr<egg::yolk::IEggProgramNode>& cond, const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  // Run in a new context
  return stackless.push<StacklessDo>(this->allocator, cond, block).resume(*this);
}

egg::ovum::Variant egg::yolk::EggProgramContext::coexecuteFor(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& pre, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& post, const std::shared_ptr<IEggProgramNode>& block) {
  // Run in a new context
  return stackless.push<StacklessFor>(this->allocator, pre, cond, post, block).resume(*this);
}

egg::ovum::Variant egg::yolk::EggProgramContext::coexecuteForeach(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& lvalue, const std::shared_ptr<IEggProgramNode>& rvalue, const std::shared_ptr<IEggProgramNode>& block) {
  // Run in a new context
  EGG_UNUSED(stackless);
  EGG_UNUSED(lvalue);
  EGG_UNUSED(rvalue);
  EGG_UNUSED(block);
  return this->raiseFormat("StacklessForeach not implemented");
  // TODO: return stackless.push<StacklessForeach>(lvalue, rvalue, block).resume(*this);
}

egg::ovum::Variant egg::yolk::EggProgramContext::coexecuteWhile(EggProgramStackless& stackless, const std::shared_ptr<egg::yolk::IEggProgramNode>& cond, const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  // Run in a new context
  return stackless.push<StacklessWhile>(this->allocator, cond, block).resume(*this);
}

egg::ovum::Variant egg::yolk::EggProgramContext::coexecuteYield(EggProgramStackless&, const std::shared_ptr<IEggProgramNode>& value) {
  auto result = value->execute(*this).direct();
  if (!result.hasFlowControl()) {
    // Need to convert the result to a return flow control
    result.addFlowControl(egg::ovum::VariantBits::Yield);
  }
  return result;
}
