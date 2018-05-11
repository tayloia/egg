#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

namespace {
  using namespace egg::yolk;

  bool abandoned(EggProgramNodeFlags flags) {
    return egg::lang::Bits::hasAnySet(flags, EggProgramNodeFlags::Abandon);
  }

  class EggProgramPreparation : public egg::lang::IPreparation {
    EGG_NO_COPY(EggProgramPreparation);
  private:
    EggProgramContext* context;
    egg::lang::LocationSource location;
  public:
    EggProgramPreparation(EggProgramContext& context, const egg::lang::LocationSource& location)
      : context(&context), location(location) {
    }
    virtual void raise(egg::lang::LogSeverity severity, const egg::lang::String& message) override {
      this->context->compiler(severity, this->location, message);
    }
  };
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Prepare all the statements one after another
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      // We've checked for duplicate symbols already
      this->symtable->addSymbol(name, *type);
    }
    auto retval = statement->prepare(*this);
    if (abandoned(retval)) {
      return retval;
    }
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareModule(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  if (this->findDuplicateSymbols(statements)) {
    return EggProgramNodeFlags::Abandon;
  }
  return this->prepareStatements(statements);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareBlock(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  EggProgramSymbolTable nested(this->symtable);
  EggProgramContext context(*this, nested);
  if (this->findDuplicateSymbols(statements)) {
    return EggProgramNodeFlags::Abandon;
  }
  return this->prepareStatements(statements);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareDeclare(const egg::lang::String& name, const egg::lang::IType& type, IEggProgramNode* rvalue) {
  // TODO type check
  (void)name; // WIBBLE
  (void)type; // WIBBLE
  if (rvalue != nullptr) {
    if (abandoned(rvalue->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareAssign(EggProgramAssign op, IEggProgramNode& lvalue, IEggProgramNode& rvalue) {
  // TODO type check
  (void)op; // WIBBLE
  if (abandoned(lvalue.prepare(*this)) || abandoned(rvalue.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareMutate(EggProgramMutate op, IEggProgramNode& lvalue) {
  // TODO type check
  (void)op; // WIBBLE
  if (abandoned(lvalue.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCatch(const egg::lang::String& name, IEggProgramNode& type, IEggProgramNode& block) {
  // TODO
  if (abandoned(type.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  EggProgramSymbolTable nested(this->symtable);
  nested.addSymbol(name, *type.getType());
  EggProgramContext context(*this, nested);
  return block.prepare(context);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareDo(IEggProgramNode& cond, IEggProgramNode& block) {
  // TODO
  if (abandoned(cond.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return block.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareIf(IEggProgramNode& cond, IEggProgramNode& trueBlock, IEggProgramNode* falseBlock) {
  // TODO
  if (abandoned(cond.prepare(*this)) || abandoned(trueBlock.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  if (falseBlock != nullptr) {
    return falseBlock->prepare(*this);
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareFor(IEggProgramNode* pre, IEggProgramNode* cond, IEggProgramNode* post, IEggProgramNode& block) {
  // TODO
  if ((pre != nullptr) && abandoned(pre->prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  if ((cond != nullptr) && abandoned(cond->prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  if ((post != nullptr) && abandoned(post->prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return block.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareForeach(IEggProgramNode& lvalue, IEggProgramNode& rvalue, IEggProgramNode& block) {
  // TODO
  if (abandoned(rvalue.prepare(*this)) || abandoned(lvalue.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto type = rvalue.getType();
  auto* iterable = type->iterable();
  if (iterable == nullptr) {
    return this->compilerError(rvalue.location(), "Expression after the ':' in 'for' statement is not iterable: '", type->toString(), "'");
  }
  return block.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareFunctionDefinition(const egg::lang::String& name, const egg::lang::IType& type, const std::shared_ptr<IEggProgramNode>& block) {
  auto callable = type.callable();
  assert(callable != nullptr);
  assert(callable->getFunctionName() == name);
  EggProgramSymbolTable nested(this->symtable);
  auto n = callable->getParameterCount();
  for (size_t i = 0; i < n; ++i) {
    auto& parameter = callable->getParameter(i);
    nested.addSymbol(parameter.getName(), parameter.getType());
  }
  EggProgramContext context(*this, nested);
  return block->prepare(context);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareReturn(IEggProgramNode* value) {
  // TODO
  if (value != nullptr) {
    return value->prepare(*this);
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCase(const std::vector<std::shared_ptr<IEggProgramNode>>& values, IEggProgramNode& block) {
  // TODO
  for (auto& value : values) {
    if (abandoned(value->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
  }
  return block.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareSwitch(IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  // TODO
  (void)defaultIndex; // WIBBLE
  if (abandoned(value.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  for (auto& i : cases) {
    if (abandoned(i->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareThrow(IEggProgramNode* exception) {
  // TODO
  if (exception != nullptr) {
    return exception->prepare(*this);
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareTry(IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, IEggProgramNode* final) {
  // TODO
  if (abandoned(block.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  for (auto& i : catches) {
    if (abandoned(i->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
  }
  if (final != nullptr) {
    return final->prepare(*this);
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareUsing(IEggProgramNode& value, IEggProgramNode& block) {
  // TODO
  if (abandoned(value.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return block.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareWhile(IEggProgramNode& cond, IEggProgramNode& block) {
  // TODO
  if (abandoned(cond.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return block.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareYield(IEggProgramNode& value) {
  // TODO
  return value.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareArray(const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  // TODO const?
  for (auto& value : values) {
    if (abandoned(value->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareObject(const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  // TODO const?
  for (auto& value : values) {
    if (abandoned(value->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCall(IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  // TODO
  if (abandoned(callee.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  for (auto& parameter : parameters) {
    if (abandoned(parameter->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCast(egg::lang::Discriminator tag, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  // TODO
  (void)tag; // WIBBLE
  for (auto& parameter : parameters) {
    if (abandoned(parameter->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareIdentifier(const egg::lang::LocationSource& where, const egg::lang::String& name, egg::lang::ITypeRef& type) {
  // We need to work out our type
  assert(type.get() == egg::lang::Type::Void.get());
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return this->compilerError(where, "Unknown identifier: '", name, "'");
  }
  type = symbol->type;
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareLiteral(const egg::lang::Value& value) {
  // TODO
  (void)value; // WIBBLE
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareUnary(EggProgramUnary op, IEggProgramNode& value) {
  // TODO
  (void)op; // WIBBLE
  return value.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareBinary(EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs) {
  // TODO
  (void)op; // WIBBLE
  if (abandoned(lhs.prepare(*this)) || abandoned(rhs.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareTernary(IEggProgramNode& cond, IEggProgramNode& whenTrue, IEggProgramNode& whenFalse) {
  // TODO
  if (abandoned(cond.prepare(*this)) || abandoned(whenTrue.prepare(*this)) || abandoned(whenFalse.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return EggProgramNodeFlags::None;
}

egg::lang::LogSeverity egg::yolk::EggProgram::prepare(IEggEnginePreparationContext& preparation) {
  EggProgramSymbolTable symtable(nullptr);
  symtable.addBuiltins();
  egg::lang::LogSeverity severity = egg::lang::LogSeverity::None;
  EggProgramContext context(preparation, symtable, severity);
  if (abandoned(this->root->prepare(context))) {
    return egg::lang::LogSeverity::Error;
  }
  return severity;
}
