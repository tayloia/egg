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

// WIBBLE
#pragma warning(disable: 4100)

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareDeclare(const egg::lang::String& name, const egg::lang::IType& type, IEggProgramNode* rvalue) {
  // TODO type check
  if (rvalue != nullptr) {
    rvalue->prepare(*this); // WIBBLE
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareAssign(EggProgramAssign op, IEggProgramNode& lvalue, IEggProgramNode& rvalue) {
  // TODO type check
  lvalue.prepare(*this); // WIBBLE
  rvalue.prepare(*this); // WIBBLE
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareMutate(EggProgramMutate op, IEggProgramNode& lvalue) {
  // TODO type check
  lvalue.prepare(*this); // WIBBLE
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCatch(const egg::lang::String& name, IEggProgramNode& type, IEggProgramNode& block) {
  // TODO
  type.prepare(*this); // WIBBLE
  EggProgramSymbolTable nested(this->symtable);
  nested.addSymbol(name, *type.getType());
  EggProgramContext context(*this, nested);
  block.prepare(context); // WIBBLE
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareDo(IEggProgramNode& cond, IEggProgramNode& block) {
  // TODO
  cond.prepare(*this); // WIBBLE
  block.prepare(*this); // WIBBLE
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareIf(IEggProgramNode& cond, IEggProgramNode& trueBlock, IEggProgramNode* falseBlock) {
  // TODO
  cond.prepare(*this); // WIBBLE
  trueBlock.prepare(*this); // WIBBLE
  if (falseBlock != nullptr) {
    falseBlock->prepare(*this); // WIBBLE
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareFor(IEggProgramNode* pre, IEggProgramNode* cond, IEggProgramNode* post, IEggProgramNode& block) {
  // TODO
  if (pre != nullptr) {
    pre->prepare(*this); // WIBBLE
  }
  if (cond != nullptr) {
    cond->prepare(*this); // WIBBLE
  }
  if (post != nullptr) {
    post->prepare(*this); // WIBBLE
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareForeach(IEggProgramNode& lvalue, IEggProgramNode& rvalue, IEggProgramNode& block) {
  // TODO
  lvalue.prepare(*this); // WIBBLE
  rvalue.prepare(*this); // WIBBLE
  block.prepare(*this); // WIBBLE
  return EggProgramNodeFlags::None;
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
    value->prepare(*this);
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCase(const std::vector<std::shared_ptr<IEggProgramNode>>& values, IEggProgramNode& block) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareSwitch(IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareThrow(IEggProgramNode* exception) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareTry(IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, IEggProgramNode* final) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareUsing(IEggProgramNode& value, IEggProgramNode& block) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareWhile(IEggProgramNode& cond, IEggProgramNode& block) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareYield(IEggProgramNode& value) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareArray(const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareObject(const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCall(IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  // TODO
  callee.prepare(*this);
  for (auto& parameter : parameters) {
    parameter->prepare(*this);
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCast(egg::lang::Discriminator tag, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareIdentifier(const egg::lang::LocationSource& where, const egg::lang::String& name) {
  // TODO uninitialised?
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return this->compilerError(where, "Unknown identifier: '", name, "'");
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareLiteral(const egg::lang::Value& value) {
  // TODO
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareUnary(EggProgramUnary op, IEggProgramNode& value) {
  // TODO
  value.prepare(*this); // WIBBLE
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareBinary(EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs) {
  // TODO
  lhs.prepare(*this); // WIBBLE
  rhs.prepare(*this); // WIBBLE
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareTernary(IEggProgramNode& cond, IEggProgramNode& whenTrue, IEggProgramNode& whenFalse) {
  // TODO
  cond.prepare(*this); // WIBBLE
  whenTrue.prepare(*this); // WIBBLE
  whenFalse.prepare(*this); // WIBBLE
  return EggProgramNodeFlags::None;
}

egg::lang::LogSeverity egg::yolk::EggProgram::prepare(IEggEnginePreparationContext& preparation) {
  EggProgramSymbolTable symtable(nullptr);
  symtable.addBuiltins();
  egg::lang::LogSeverity severity = egg::lang::LogSeverity::None;
  EggProgramContext context(preparation, symtable, severity);
  if (abandoned(this->root->prepare(context))) {
    context.log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Compilation abandoned due to previous errors");
    return egg::lang::LogSeverity::Error;
  }
  return severity;
}
