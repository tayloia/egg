#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

namespace {
  using namespace egg::yolk;

  bool abandoned(EggProgramNodeFlags flags) {
    return egg::ovum::Bits::hasAnySet(flags, EggProgramNodeFlags::Abandon);
  }
  bool fallthrough(EggProgramNodeFlags flags) {
    return egg::ovum::Bits::hasAnySet(flags, EggProgramNodeFlags::Fallthrough);
  }
  EggProgramNodeFlags checkBinarySide(EggProgramContext& context, const egg::ovum::LocationSource& where, EggProgramBinary op, const char* side, egg::ovum::ValueFlags expected, IEggProgramNode& node) {
    auto prepared = node.prepare(context);
    if (!abandoned(prepared)) {
      auto type = node.getType();
      if (!type.hasPrimitiveFlag(expected)) {
        if (expected == egg::ovum::ValueFlags::Null) {
          context.compilerWarning(where, "Expected ", side, " of '", EggProgram::binaryToString(op), "' operator to be possibly 'null', but got '", type.toString(), "' instead");
        } else {
          auto readable = egg::ovum::StringBuilder::concat(expected).replace("|", "' or '");
          prepared = context.compilerError(where, "Expected ", side, " of '", EggProgram::binaryToString(op), "' operator to be '", readable, "', but got '", type.toString(), "' instead");
        }
      }
    }
    return prepared;
  }
  EggProgramNodeFlags checkBinary(EggProgramContext& context, const egg::ovum::LocationSource& where, EggProgramBinary op, egg::ovum::ValueFlags lexp, IEggProgramNode& lhs, egg::ovum::ValueFlags rexp, IEggProgramNode& rhs) {
    auto lflags = checkBinarySide(context, where, op, "left-hand side", lexp, lhs);
    if (abandoned(lflags)) {
      return lflags;
    }
    auto rflags = checkBinarySide(context, where, op, "right-hand side", rexp, rhs);
    if (abandoned(rflags)) {
      return rflags;
    }
    return egg::ovum::Bits::mask(lflags, rflags);
  }
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareScope(const IEggProgramNode* node, std::function<EggProgramNodeFlags(EggProgramContext&)> action) {
  EggProgramSymbol symbol;
  if ((node != nullptr) && node->symbol(symbol)) {
    // Perform the action with a new scope containing our symbol
    auto nested = this->allocator.makeHard<EggProgramSymbolTable>(this->symtable.get());
    nested->addSymbol(EggProgramSymbol::Kind::ReadWrite, symbol.name, symbol.type);
    auto context = this->createNestedContext(*nested, this->scopeFunction);
    return action(*context);
  }
  // Just perform the action in the current scope
  return action(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Prepare all the statements one after another
  EggProgramSymbol symbol;
  EggProgramNodeFlags retval = EggProgramNodeFlags::Fallthrough; // We fallthrough if there are no statements
  auto unreachable = false;
  for (auto& statement : statements) {
    if (!unreachable && (retval != EggProgramNodeFlags::Fallthrough)) {
      this->compilerWarning(statement->location(), "Unreachable code");
      unreachable = true;
    }
    if (statement->symbol(symbol)) {
      // We've checked for duplicate symbols already
      this->symtable->addSymbol(symbol.kind, symbol.name, symbol.type);
    }
    retval = statement->prepare(*this);
    if (abandoned(retval)) {
      return retval;
    }
    // We can only perform this after preparing the statement, otherwise the type information isn't correct (always 'void')
    auto rettype = statement->getType();
    if (rettype->getPrimitiveFlags() != egg::ovum::ValueFlags::Void) {
      this->compilerWarning(statement->location(), "Expected statement to return 'void', but got '", rettype.toString(), "' instead");
    }
  }
  return retval;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareModule(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // We don't need a nested scope here
  if (this->findDuplicateSymbols(statements)) {
    return EggProgramNodeFlags::Abandon;
  }
  return this->prepareStatements(statements);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareBlock(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // We need a nested scope here to deal with local variables
  if (this->findDuplicateSymbols(statements)) {
    return EggProgramNodeFlags::Abandon;
  }
  auto nested = this->allocator.makeHard<EggProgramSymbolTable>(this->symtable.get());
  auto context = this->createNestedContext(*nested, this->scopeFunction);
  return context->prepareStatements(statements);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareDeclare(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::Type& ltype, IEggProgramNode* rvalue) {
  if (this->scopeDeclare != nullptr) {
    // This must be a prepare call with an inferred type
    assert(rvalue == nullptr);
    return this->typeCheck(where, ltype, egg::ovum::Type(this->scopeDeclare), name, false);
  }
  if (rvalue != nullptr) {
    // Type-check the initialization
    if (abandoned(rvalue->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
    return this->typeCheck(rvalue->location(), ltype, rvalue->getType(), name, false);
  }
  if (ltype == nullptr) {
    return this->compilerError(where, "Cannot infer type of '", name, "' declared with 'var'");
  }
  return EggProgramNodeFlags::Fallthrough;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareMember(const egg::ovum::LocationSource& where, const egg::ovum::String& name, const egg::ovum::IType& type, egg::ovum::Modifiability modifiability) {
  assert(this->scopeTypedef != nullptr);
  (void)where; // WIBBLE
  (void)name; // WIBBLE
  (void)type; // WIBBLE
  (void)modifiability; // WIBBLE
  return EggProgramNodeFlags::Fallthrough;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareStatic(const egg::ovum::LocationSource&, IEggProgramNode& child) {
  return child.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareIterable(const egg::ovum::LocationSource& where, egg::ovum::Type& itype) {
  if (itype == nullptr) {
    return this->compilerError(where, "Cannot infer iterable type");
  }
  return EggProgramNodeFlags::Fallthrough;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareGuard(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::Type& ltype, IEggProgramNode& rvalue) {
  if (abandoned(rvalue.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return this->typeCheck(where, ltype, rvalue.getType(), name, true);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareAssign(const egg::ovum::LocationSource& where, EggProgramAssign op, IEggProgramNode& lvalue, IEggProgramNode& rvalue) {
  if (abandoned(lvalue.prepare(*this)) || abandoned(rvalue.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto ltype = lvalue.getType();
  auto rtype = rvalue.getType();
  switch (op) {
  case EggProgramAssign::Equal:
    // Simple assignment
    switch (ltype.queryAssignable(*rtype)) {
    case egg::ovum::Type::Assignability::Never:
      return this->compilerError(where, ltype.describeValue(), " cannot be assigned a value of type '", rtype.toString(), "'");
    case egg::ovum::Type::Assignability::Sometimes:
    case egg::ovum::Type::Assignability::Always:
      break;
    }
    break;
  case EggProgramAssign::LogicalAnd:
  case EggProgramAssign::LogicalOr:
    // Boolean operation
    if (!ltype.hasPrimitiveFlag(egg::ovum::ValueFlags::Bool)) {
      return this->compilerError(where, "Expected left-hand side of '", EggProgram::assignToString(op), "' assignment operator to be 'bool', but got '", ltype.toString(), "' instead");
    }
    if (!rtype.hasPrimitiveFlag(egg::ovum::ValueFlags::Bool)) {
      return this->compilerError(where, "Expected right-hand side of '", EggProgram::assignToString(op), "' assignment operator to be 'bool', but got '", ltype.toString(), "' instead");
    }
    break;
  case EggProgramAssign::BitwiseAnd:
  case EggProgramAssign::BitwiseOr:
  case EggProgramAssign::BitwiseXor:
    // Boolean/Integer operation
    if (!ltype.hasPrimitiveFlag(egg::ovum::ValueFlags::Bool | egg::ovum::ValueFlags::Int)) {
      return this->compilerError(where, "Expected left-hand side of '", EggProgram::assignToString(op), "' assignment operator to be 'bool' or 'int', but got '", ltype.toString(), "' instead");
    }
    if (rtype->getPrimitiveFlags() != ltype->getPrimitiveFlags()) {
      return this->compilerError(where, "Expected right-hand target of '", EggProgram::assignToString(op), "' assignment operator to be '", ltype.toString(), "', but got '", rtype.toString(), "' instead");
    }
    break;
  case EggProgramAssign::ShiftLeft:
  case EggProgramAssign::ShiftRight:
  case EggProgramAssign::ShiftRightUnsigned:
    // Integer-only operation
    if (!ltype.hasPrimitiveFlag(egg::ovum::ValueFlags::Int)) {
      return this->compilerError(where, "Expected left-hand target of integer '", EggProgram::assignToString(op), "' assignment operator to be 'int', but got '", ltype.toString(), "' instead");
    }
    if (!rtype.hasPrimitiveFlag(egg::ovum::ValueFlags::Int)) {
      return this->compilerError(where, "Expected right-hand side of integer '", EggProgram::assignToString(op), "' assignment operator to be 'int', but got '", rtype.toString(), "' instead");
    }
    break;
  case EggProgramAssign::Remainder:
  case EggProgramAssign::Multiply:
  case EggProgramAssign::Plus:
  case EggProgramAssign::Minus:
  case EggProgramAssign::Divide:
    // Arithmetic operation
    switch (EggProgram::arithmeticTypes(rtype)) {
    case EggProgram::ArithmeticTypes::Float:
      // Float-only operation
      if (!ltype.hasPrimitiveFlag(egg::ovum::ValueFlags::Float)) {
        return this->compilerError(where, "Expected left-hand target of floating-point '", EggProgram::assignToString(op), "' assignment operator to be 'float', but got '", ltype.toString(), "' instead");
      }
      break;
    case EggProgram::ArithmeticTypes::Both:
    case EggProgram::ArithmeticTypes::Int:
      // Float-or-int operation
      if (EggProgram::arithmeticTypes(ltype) == EggProgram::ArithmeticTypes::None) {
        return this->compilerError(where, "Expected left-hand target of '", EggProgram::assignToString(op), "' assignment operator to be 'int' or 'float', but got '", ltype.toString(), "' instead");
      }
      break;
    case EggProgram::ArithmeticTypes::None:
      return this->compilerError(where, "Expected right-hand side of '", EggProgram::assignToString(op), "' assignment operator to be 'int' or 'float', but got '", rtype.toString(), "' instead");
    }
    break;
  case EggProgramAssign::NullCoalescing:
    switch (ltype.queryAssignable(*rtype)) {
    case egg::ovum::Type::Assignability::Never:
      return this->compilerError(where, ltype.describeValue(), " cannot be assigned a value of type '", rtype.toString(), "'");
    case egg::ovum::Type::Assignability::Sometimes:
    case egg::ovum::Type::Assignability::Always:
      break;
    }
    if (!ltype.hasPrimitiveFlag(egg::ovum::ValueFlags::Null)) {
      // This is just a warning
      this->compilerWarning(where, "Expected left-hand target of null-coalescing '??=' assignment operator to be possibly 'null', but got '", ltype.toString(), "' instead");
    }
    break;
  }
  return EggProgramNodeFlags::Fallthrough;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareMutate(const egg::ovum::LocationSource& where, EggProgramMutate op, IEggProgramNode& lvalue) {
  if (abandoned(lvalue.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto ltype = lvalue.getType();
  switch (op) {
  case EggProgramMutate::Increment:
  case EggProgramMutate::Decrement:
    // Integer-only operation
    if (!ltype.hasPrimitiveFlag(egg::ovum::ValueFlags::Int)) {
      return this->compilerError(where, "Expected target of integer '", EggProgram::mutateToString(op), "' operator to be 'int', but got '", ltype.toString(), "' instead");
    }
    break;
  }
  return EggProgramNodeFlags::Fallthrough;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCatch(const egg::ovum::String& name, IEggProgramNode& type, IEggProgramNode& block) {
  // TODO
  if (abandoned(type.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto nested = this->allocator.makeHard<EggProgramSymbolTable>(this->symtable.get());
  nested->addSymbol(EggProgramSymbol::Kind::ReadWrite, name, type.getType());
  auto context = this->createNestedContext(*nested, this->scopeFunction);
  return block.prepare(*context);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareDo(IEggProgramNode& cond, IEggProgramNode& block) {
  // TODO
  if (abandoned(cond.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return block.prepare(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareIf(IEggProgramNode& cond, IEggProgramNode& trueBlock, IEggProgramNode* falseBlock) {
  return this->prepareScope(&cond, [&](EggProgramContext& scope) {
    // We prepare the 'else' block in the original scope (with no guarded identifiers)
    auto pcond = cond.prepare(scope);
    if (abandoned(pcond)) {
      return EggProgramNodeFlags::Abandon;
    }
    if (egg::ovum::Bits::hasAnySet(pcond, EggProgramNodeFlags::Constant)) {
      this->compilerWarning(cond.location(), "Condition in 'if' statement is constant");
    }
    auto ptrue = trueBlock.prepare(scope);
    if (abandoned(ptrue)) {
      return ptrue;
    }
    if (falseBlock == nullptr) {
      return EggProgramNodeFlags::Fallthrough;
    }
    auto pfalse = falseBlock->prepare(*this);
    if (abandoned(pfalse)) {
      return EggProgramNodeFlags::Abandon;
    }
    if (fallthrough(ptrue)) {
      return ptrue;
    }
    return pfalse;
  });
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareFor(IEggProgramNode* pre, IEggProgramNode* cond, IEggProgramNode* post, IEggProgramNode& block) {
  // TODO
  return this->prepareScope(pre, [&](EggProgramContext& scope) {
    if ((pre != nullptr) && abandoned(pre->prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    if ((cond != nullptr) && abandoned(cond->prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    if ((post != nullptr) && abandoned(post->prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    return block.prepare(scope);
  });
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareForeach(IEggProgramNode& lvalue, IEggProgramNode& rvalue, IEggProgramNode& block) {
  return this->prepareScope(&lvalue, [&](EggProgramContext& scope) {
    if (abandoned(rvalue.prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    auto type = rvalue.getType();
    auto iterable = this->factory.queryIterable(type);
    if (iterable == nullptr) {
      return scope.compilerError(rvalue.location(), type.describeValue(), " is not iterable");
    }
    if (abandoned(scope.prepareWithType(lvalue, iterable->getType()))) {
      return EggProgramNodeFlags::Abandon;
    }
    return block.prepare(scope);
  });
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareFunctionDefinition(const egg::ovum::String& name, const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& block) {
  // TODO type check
  auto callable = this->factory.queryCallable(type);
  assert(callable != nullptr);
  assert(callable->getName() == name);
  auto nested = this->allocator.makeHard<EggProgramSymbolTable>(this->symtable.get());
  auto n = callable->getParameterCount();
  for (size_t i = 0; i < n; ++i) {
    auto& parameter = callable->getParameter(i);
    nested->addSymbol(EggProgramSymbol::Kind::ReadWrite, parameter.getName(), parameter.getType());
  }
  auto rettype = callable->getReturnType();
  // This structure will be overwritten later if this is actually a generator definition
  ScopeFunction function{ rettype.get(), false };
  auto context = this->createNestedContext(*nested, &function);
  assert(context->scopeFunction == &function);
  auto flags = block->prepare(*context);
  if (abandoned(flags)) {
    return flags;
  }
  if (fallthrough(flags)) {
    // Falling through to the end of a non-generator function is the same as an implicit 'return' with no parameters
    if (!egg::ovum::Bits::hasAnySet(function.rettype->getPrimitiveFlags(), egg::ovum::ValueFlags::Void)) {
      egg::ovum::String suffix;
      if (!name.empty()) {
        suffix = egg::ovum::StringBuilder::concat(": '", name, "'");
      }
      egg::ovum::Type expected{ function.rettype };
      return context->compilerError(block->location(), "Missing 'return' statement with a value of type '", expected.toString(), "' at the end of the function definition", suffix);
    }
  }
  return EggProgramNodeFlags::Fallthrough; // We fallthrough AFTER the function definition
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareGeneratorDefinition(const egg::ovum::Type& rettype, const std::shared_ptr<IEggProgramNode>& block) {
  // We're in a 'generator' node that's the parent of a 'block' node within a 'function definition' node
  assert(this->scopeFunction != nullptr);
  assert(this->scopeFunction->rettype != nullptr);
  assert(this->scopeFunction->generator == false);
  // Adjust the scope function for generators
  this->scopeFunction->rettype = rettype.get();
  this->scopeFunction->generator = true;
  auto flags = block->prepare(*this);
  if (abandoned(flags)) {
    return flags;
  }
  // The implementation of the final generator definition is effectively a single return statement; we don't fallthrough
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareReturn(const egg::ovum::LocationSource& where, IEggProgramNode* value) {
  if (this->scopeFunction == nullptr) {
    return this->compilerError(where, "Unexpected 'return' statement");
  }
  if (this->scopeFunction->generator) {
    if (value == nullptr) {
      // No return value
      return EggProgramNodeFlags::None; // No fallthrough
    }
    return this->compilerError(where, "Unexpected value in generator 'return' statement");
  }
  egg::ovum::Type rettype{ this->scopeFunction->rettype };
  assert(rettype != nullptr);
  if (value == nullptr) {
    // No return value
    switch (rettype.queryAssignable(*egg::ovum::Type::Void)) {
    case egg::ovum::Type::Assignability::Never:
      return this->compilerError(where, "Expected 'return' statement with a value of type '", rettype.toString(), "'");
    case egg::ovum::Type::Assignability::Sometimes:
    case egg::ovum::Type::Assignability::Always:
      break;
    }
    return EggProgramNodeFlags::None; // No fallthrough
  }
  if (abandoned(value->prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto rtype = value->getType();
  switch (rettype.queryAssignable(*rtype)) {
  case egg::ovum::Type::Assignability::Never:
    return this->compilerError(where, "Expected 'return' statement with a value of type '", rettype.toString(), "', but got '", rtype.toString(), "' instead");
  case egg::ovum::Type::Assignability::Sometimes:
  case egg::ovum::Type::Assignability::Always:
    break;
  }
  return EggProgramNodeFlags::None; // No fallthrough
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
  // TODO check duplicate constants
  return this->prepareScope(&value, [&](EggProgramContext& scope) {
    if (abandoned(value.prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    bool falls = (defaultIndex < 0); // No 'default:' clause
    for (auto& i : cases) {
      auto flags = i->prepare(scope);
      if (abandoned(flags)) {
        return EggProgramNodeFlags::Abandon;
      }
      falls |= fallthrough(flags);
    }
    return falls ? EggProgramNodeFlags::Fallthrough : EggProgramNodeFlags::None;
  });
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareThrow(IEggProgramNode* exception) {
  // TODO
  if (exception != nullptr) {
    return exception->prepare(*this);
  }
  return EggProgramNodeFlags::None; // No fallthrough
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareTry(IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, IEggProgramNode* final) {
  // TODO
  auto flags = block.prepare(*this);
  if (abandoned(flags)) {
    return EggProgramNodeFlags::Abandon;
  }
  auto falls = fallthrough(flags);
  for (auto& i : catches) {
    flags = i->prepare(*this);
    if (abandoned(flags)) {
      return EggProgramNodeFlags::Abandon;
    }
    falls |= fallthrough(flags);
  }
  if (final != nullptr) {
    return final->prepare(*this);
  }
  return falls ? EggProgramNodeFlags::Fallthrough : EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareTypedef(const egg::ovum::LocationSource&, const egg::ovum::String& name, const std::vector<std::shared_ptr<IEggProgramNode>>& constraints, const std::vector<std::shared_ptr<IEggProgramNode>>& definitions) {
  // Run a prepare call with a scope typedef set
  assert(this->scopeTypedef == nullptr);
  ScopeTypedef scope{ name };
  try {
    this->scopeTypedef = &scope;
    for (auto& constraint : constraints) {
      if (abandoned(constraint->prepare(*this))) {
        this->scopeTypedef = nullptr;
        return EggProgramNodeFlags::Abandon;
      }
    }
    for (auto& definition : definitions) {
      if (abandoned(definition->prepare(*this))) {
        this->scopeTypedef = nullptr;
        return EggProgramNodeFlags::Abandon;
      }
    }
    this->scopeTypedef = nullptr;
  }
  catch (...) {
    this->scopeTypedef = nullptr;
    throw;
  }
  return EggProgramNodeFlags::Fallthrough;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareWhile(IEggProgramNode& cond, IEggProgramNode& block) {
  // TODO
  return this->prepareScope(&cond, [&](EggProgramContext& scope) {
    if (abandoned(cond.prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    return block.prepare(scope);
  });
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareYield(const egg::ovum::LocationSource& where, IEggProgramNode& value) {
  if ((this->scopeFunction == nullptr) || !this->scopeFunction->generator) {
    return this->compilerError(where, "Unexpected 'yield' statement");
  }
  if (abandoned(value.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto rtype = value.getType();
  egg::ovum::Type rettype{ this->scopeFunction->rettype };
  assert(rettype != nullptr);
  switch (rettype.queryAssignable(*rtype)) {
  case egg::ovum::Type::Assignability::Never:
    return this->compilerError(where, "Expected 'yield' statement with a value of type '", rettype.toString(), "', but got '", rtype.toString(), "' instead");
  case egg::ovum::Type::Assignability::Sometimes:
  case egg::ovum::Type::Assignability::Always:
    break;
  }
  return EggProgramNodeFlags::Fallthrough;
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

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCall(IEggProgramNode& callee, std::vector<std::shared_ptr<IEggProgramNode>>& parameters, egg::ovum::Type& rettype) {
  if (abandoned(callee.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto ctype = callee.getType();
  assert(ctype != nullptr);
  auto* callable = this->factory.queryCallable(ctype);
  if (callable == nullptr) {
    return this->compilerError(callee.location(), "Expected function-like expression to be callable, but got '", ctype.toString(), "' instead");
  }
  // TODO type check parameters
  auto expected = callable->getParameterCount();
  size_t position = 0;
  bool variadic = false;
  for (auto& parameter : parameters) {
    if (position >= expected) {
      return this->compilerError(parameter->location(), "Expected ", expected, " parameters for '", ctype.toString(), "', but got ", parameters.size(), " instead");
    }
    auto& cparam = callable->getParameter(position);
    if (egg::ovum::Bits::hasAnySet(cparam.getFlags(), egg::ovum::IFunctionSignatureParameter::Flags::Variadic)) {
      variadic = true;
    }
    if (egg::ovum::Bits::hasAnySet(cparam.getFlags(), egg::ovum::IFunctionSignatureParameter::Flags::Predicate)) {
      parameter->empredicate(*this, parameter);
    }
    if (abandoned(parameter->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
    if (!variadic) {
      position++;
    }
  }
  rettype = callable->getReturnType();
  return EggProgramNodeFlags::Fallthrough;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareIdentifier(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::Type& type) {
  // We need to work out our type
  assert(type.get() == egg::ovum::Type::Void.get());
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return this->compilerError(where, "Unknown identifier: '", name, "'");
  }
  type.set(symbol->type.get());
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareBrackets(const egg::ovum::LocationSource& where, IEggProgramNode& instance, IEggProgramNode& index) {
  if (abandoned(instance.prepare(*this)) || abandoned(index.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto ltype = instance.getType();
  auto indexable = this->factory.queryIndexable(ltype);
  if (indexable == nullptr) {
    return this->compilerError(where, ltype.describeValue(), " does not support the indexing '[]' operator");
  }
  // TODO check type indexable->getIndexType()
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareDot(const egg::ovum::LocationSource& where, IEggProgramNode& instance, const egg::ovum::String& property, egg::ovum::Type& restype) {
  // Left-hand side should be string/object
  if (abandoned(instance.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto ltype = instance.getType();
  auto dotable = this->factory.queryDotable(ltype);
  if (dotable == nullptr) {
    return this->compilerError(where, ltype.describeValue(), " does not support the property '.' operator");
  }
  auto modifiability = dotable->getModifiability(property);
  if (modifiability == egg::ovum::Modifiability::NONE) {
    if (dotable->isClosed()) {
      return this->compilerError(where, ltype.describeValue(), " does not support the property '", property, "'");
    }
  }
  restype = dotable->getType(property);
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareUnary(const egg::ovum::LocationSource& where, EggProgramUnary op, IEggProgramNode& value) {
  auto prepared = value.prepare(*this);
  if (abandoned(prepared)) {
    return EggProgramNodeFlags::Abandon;
  }
  auto type = value.getType();
  switch (op) {
  case EggProgramUnary::LogicalNot:
    // Boolean-only operation
    if (!type.hasPrimitiveFlag(egg::ovum::ValueFlags::Bool)) {
      return this->compilerError(where, "Expected operand of logical-not '!' operator to be 'bool', but got '", type.toString(), "' instead");
    }
    break;
  case EggProgramUnary::BitwiseNot:
    // Integer-only operation
    if (!type.hasPrimitiveFlag(egg::ovum::ValueFlags::Int)) {
      return this->compilerError(where, "Expected operand of bitwise-not '~' operator to be 'int', but got '", type.toString(), "' instead");
    }
    break;
  case EggProgramUnary::Negate:
    // Arithmetic operation
    if (EggProgram::arithmeticTypes(type) == EggProgram::ArithmeticTypes::None) {
      return this->compilerError(where, "Expected operand of negation '-' operator to be 'int' or 'float', but got '", type.toString(), "' instead");
    }
    break;
  case EggProgramUnary::Ref:
    // Reference '&' operation tells the child node to return the address of the value ("byref")
    // TODO check the expression: do we allow, for example, '&0'?
    break;
  case EggProgramUnary::Deref:
    // Dereference '*' operation
    if (this->factory.queryPointable(type) == nullptr) {
      return this->compilerError(where, "Expected operand of dereference '*' operator to be a pointer, but got '", type.toString(), "' instead");
    }
    break;
  case EggProgramUnary::Ellipsis:
    return this->compilerError(where, "Unary '", EggProgram::unaryToString(op), "' operator not yet supported"); // TODO
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareBinary(const egg::ovum::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs) {
  switch (op) {
  case EggProgramBinary::LogicalAnd:
  case EggProgramBinary::LogicalOr:
    // Boolean-only operation
    return checkBinary(*this, where, op, egg::ovum::ValueFlags::Bool, lhs, egg::ovum::ValueFlags::Bool, rhs);
  case EggProgramBinary::BitwiseAnd:
  case EggProgramBinary::BitwiseOr:
  case EggProgramBinary::BitwiseXor:
    // Boolean/integer operation
    return checkBinary(*this, where, op, egg::ovum::ValueFlags::Bool | egg::ovum::ValueFlags::Int, lhs, egg::ovum::ValueFlags::Bool | egg::ovum::ValueFlags::Int, rhs);
  case EggProgramBinary::ShiftLeft:
  case EggProgramBinary::ShiftRight:
  case EggProgramBinary::ShiftRightUnsigned:
    // Integer-only operation
    return checkBinary(*this, where, op, egg::ovum::ValueFlags::Int, lhs, egg::ovum::ValueFlags::Int, rhs);
  case EggProgramBinary::Plus:
  case EggProgramBinary::Minus:
  case EggProgramBinary::Multiply:
  case EggProgramBinary::Divide:
  case EggProgramBinary::Remainder:
  case EggProgramBinary::Less:
  case EggProgramBinary::LessEqual:
  case EggProgramBinary::Greater:
  case EggProgramBinary::GreaterEqual:
    // Arithmetic operation
    return checkBinary(*this, where, op, egg::ovum::ValueFlags::Arithmetic, lhs, egg::ovum::ValueFlags::Arithmetic, rhs);
  case EggProgramBinary::Equal:
  case EggProgramBinary::Unequal:
    // Equality operation
    break;
  case EggProgramBinary::NullCoalescing:
    // Warn if the left-hand-side can never be null
    return checkBinary(*this, where, op, egg::ovum::ValueFlags::Null, lhs, egg::ovum::ValueFlags::AnyQ, rhs);
  case EggProgramBinary::Lambda:
    return this->compilerError(where, "'", EggProgram::binaryToString(op), "' operators not yet supported in 'prepareBinary'");
  }
  if (abandoned(lhs.prepare(*this)) || abandoned(rhs.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareTernary(const egg::ovum::LocationSource& where, IEggProgramNode& cond, IEggProgramNode& whenTrue, IEggProgramNode& whenFalse) {
  // TODO
  if (abandoned(cond.prepare(*this)) || abandoned(whenTrue.prepare(*this)) || abandoned(whenFalse.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto type = cond.getType();
  if (!type.hasPrimitiveFlag(egg::ovum::ValueFlags::Bool)) {
    return this->compilerError(where, "Expected condition of ternary '?:' operator to be 'bool', but got '", type.toString(), "' instead");
  }
  type = whenTrue.getType();
  if (type->getPrimitiveFlags() == egg::ovum::ValueFlags::None) {
    return this->compilerError(whenTrue.location(), "Expected value for second operand of ternary '?:' operator , but got '", type.toString(), "' instead");
  }
  type = whenFalse.getType();
  if (type->getPrimitiveFlags() == egg::ovum::ValueFlags::None) {
  return this->compilerError(whenTrue.location(), "Expected value for third operand of ternary '?:' operator , but got '", type.toString(), "' instead");
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::preparePredicate(const egg::ovum::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs) {
  return this->prepareBinary(where, op, lhs, rhs);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareWithType(IEggProgramNode& node, const egg::ovum::Type& type) {
  // Run a prepare call with a scope type set
  assert(this->scopeDeclare == nullptr);
  try {
    this->scopeDeclare = type.get();
    auto result = node.prepare(*this);
    this->scopeDeclare = nullptr;
    return result;
  } catch (...) {
    this->scopeDeclare = nullptr;
    throw;
  }
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::typeCheck(const egg::ovum::LocationSource& where, egg::ovum::Type& ltype, const egg::ovum::Type& rtype, const egg::ovum::String& name, bool guard) {
  if (ltype == nullptr) {
    // We need to infer the type
    ltype = this->factory.removeVoid(rtype);
    if (guard) {
      ltype = this->factory.removeNull(ltype);
    }
    if (ltype == nullptr) {
      return this->compilerError(where, "Cannot infer type of '", name, "' based on a value of type '", rtype.toString(), "'"); // TODO useful?
    }
    auto symbol = this->symtable->findSymbol(name, false);
    assert(symbol != nullptr);
    assert(symbol->type == nullptr);
    symbol->type = ltype;
  }
  switch (ltype.queryAssignable(*rtype)) {
  case egg::ovum::Type::Assignability::Never:
    return this->compilerError(where, "Cannot initialize '", name, "' of type '", ltype.toString(), "' with a value of type '", rtype.toString(), "'");
  case egg::ovum::Type::Assignability::Sometimes:
    break;
  case egg::ovum::Type::Assignability::Always:
    if (guard) {
      this->compilerWarning(where, "Guarded assignment to '", name, "' of type '", ltype.toString(), "' will always succeed");
    }
    break;
  }
  return EggProgramNodeFlags::Fallthrough;
}

egg::ovum::ILogger::Severity egg::yolk::EggProgram::prepare(IEggEngineContext& context) {
  auto& allocator = context.getAllocator();
  auto& factory = context.getTypeFactory();
  auto symtable = allocator.makeHard<EggProgramSymbolTable>();
  symtable->addBuiltins(factory, *this->basket);
  egg::ovum::ILogger::Severity severity = egg::ovum::ILogger::Severity::None;
  auto rootContext = this->createRootContext(factory, context, *symtable, severity);
  if (abandoned(this->root->prepare(*rootContext))) {
    return egg::ovum::ILogger::Severity::Error;
  }
  return severity;
}
