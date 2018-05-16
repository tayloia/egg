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
  EggProgramNodeFlags checkBinarySide(EggProgramContext& context, const egg::lang::LocationSource& where, EggProgramBinary op, const char* side, egg::lang::Discriminator expected, IEggProgramNode& node) {
    auto prepared = node.prepare(context);
    if (!abandoned(prepared)) {
      auto& type = *node.getType();
      auto simple = type.getSimpleTypes();
      assert(simple != egg::lang::Discriminator::Inferred);
      if (!egg::lang::Bits::hasAnySet(simple, expected)) {
        std::string readable = egg::lang::Value::getTagString(expected);
        readable = String::replace(readable, "|", "' or '");
        prepared = context.compilerError(where, "Expected ", side, " of '", EggProgram::binaryToString(op), "' operator to be '", readable, "', but got '", type.toString(), "' instead");
      }
    }
    return prepared;
  }
  EggProgramNodeFlags checkBinary(EggProgramContext& context, const egg::lang::LocationSource& where, EggProgramBinary op, egg::lang::Discriminator lexp, IEggProgramNode& lhs, egg::lang::Discriminator rexp, IEggProgramNode& rhs) {
    auto result = checkBinarySide(context, where, op, "left-hand side", lexp, lhs);
    if (abandoned(result)) {
      return result;
    }
    return checkBinarySide(context, where, op, "right-hand side", rexp, rhs);
  }
  EggProgramNodeFlags checkBrackets(EggProgramContext& context, const egg::lang::LocationSource& where, IEggProgramNode& lhs, IEggProgramNode& rhs) {
    // Brackets can only be applied to strings and objects
    if (abandoned(checkBinarySide(context, where, EggProgramBinary::Brackets, "subject", egg::lang::Discriminator::String | egg::lang::Discriminator::Object, lhs))) {
      return EggProgramNodeFlags::Abandon;
    }
    auto ltype = lhs.getType();
    auto mask = egg::lang::Bits::mask(ltype->getSimpleTypes(), egg::lang::Discriminator::String | egg::lang::Discriminator::Object);
    if (mask == egg::lang::Discriminator::String) {
      // Strings only accept integer indices
      return checkBinarySide(context, where, EggProgramBinary::Brackets, " string index", egg::lang::Discriminator::Int, rhs);
    }
    if (abandoned(rhs.prepare(context))) {
      return EggProgramNodeFlags::Abandon;
    }
    if (mask == egg::lang::Discriminator::Object) {
      // Ask the object what indexing it supports
      auto indexable = ltype->indexable();
      if (indexable == nullptr) {
        return context.compilerError(where, "Instances of type '", ltype->toString(), "' do not support the indexing operator '[]'");
      }
      // TODO check type indexable->getIndexType()
    }
    return EggProgramNodeFlags::None;
  }
  EggProgramNodeFlags checkDot(EggProgramContext& context, const egg::lang::LocationSource& where, IEggProgramNode& lhs, IEggProgramNode& rhs) {
    // Left-hand side should be string/object
    auto result = checkBinary(context, where, EggProgramBinary::Dot, egg::lang::Discriminator::String | egg::lang::Discriminator::Object, lhs, egg::lang::Discriminator::String, rhs);
    if (abandoned(result)) {
      return result;
    }
    auto ltype = lhs.getType();
    auto mask = egg::lang::Bits::mask(ltype->getSimpleTypes(), egg::lang::Discriminator::String | egg::lang::Discriminator::Object);
    if (mask == egg::lang::Discriminator::String) {
      // 'result' holds the flags for the right-hand side (property name)
      if (result == EggProgramNodeFlags::Constant) {
        // Check specific property names
        auto property = rhs.execute(context);
        assert(property.is(egg::lang::Discriminator::String));
        auto name = property.getString();
        if (egg::lang::String::builtinFactory(name) == nullptr) {
          // Not a known string builtin
          return context.compilerError(where, "Unknown property for 'string': '.", name, "'");
        }
      }
    }
    if (mask == egg::lang::Discriminator::Object) {
      // Ask the object what fields it supports
      auto dotable = ltype->dotable();
      if (dotable == nullptr) {
        return context.compilerError(where, "Instances of type '", ltype->toString(), "' do not support the '.' operator for field access");
      }
    }
    return EggProgramNodeFlags::None;
  }
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareScope(const IEggProgramNode* node, std::function<EggProgramNodeFlags(EggProgramContext&)> action) {
  egg::lang::String name;
  egg::lang::ITypeRef type{ egg::lang::Type::Void };
  if ((node != nullptr) && node->symbol(name, type)) {
    // Perform the action with a new scope containing our symbol
    EggProgramSymbolTable nested(this->symtable);
    nested.addSymbol(EggProgramSymbol::ReadWrite, name, *type);
    EggProgramContext context(*this, nested);
    return action(context);
  }
  // Just perform the action in the current scope
  return action(*this);
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Prepare all the statements one after another
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      // We've checked for duplicate symbols already
      this->symtable->addSymbol(EggProgramSymbol::ReadWrite, name, *type);
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

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareDeclare(const egg::lang::LocationSource& where, const egg::lang::String& name, egg::lang::ITypeRef& ltype, const egg::lang::IType* rtype, IEggProgramNode* rvalue) {
  if (rtype != nullptr) {
    // This must be a prepareWithType call with an inferred type
    assert(rvalue == nullptr);
    return this->typeCheck(where, ltype, egg::lang::ITypeRef(rtype), name);
  }
  if (rvalue != nullptr) {
    // Type-check the initialization
    if (abandoned(rvalue->prepare(*this))) {
      return EggProgramNodeFlags::Abandon;
    }
    return this->typeCheck(rvalue->location(), ltype, rvalue->getType(), name);
  }
  if (ltype->getSimpleTypes() == egg::lang::Discriminator::Inferred) {
    return this->compilerError(where, "Cannot infer type of '", name, "' declared with 'var'");
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareAssign(const egg::lang::LocationSource& where, EggProgramAssign op, IEggProgramNode& lvalue, IEggProgramNode& rvalue) {
  if (abandoned(lvalue.prepare(*this)) || abandoned(rvalue.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto& ltype = *lvalue.getType();
  auto lsimple = ltype.getSimpleTypes();
  assert(lsimple != egg::lang::Discriminator::Inferred);
  auto& rtype = *rvalue.getType();
  auto rsimple = rtype.getSimpleTypes();
  assert(rsimple != egg::lang::Discriminator::Inferred);
  switch (op) {
  case EggProgramAssign::Equal:
    // Simple assignment
    if (!ltype.canBeAssignedFrom(rtype)) {
      return this->compilerError(where, "Cannot assign a value of type '", rtype.toString(), "' to a target of type '", ltype.toString(), "'");
    }
    break;
  case EggProgramAssign::BitwiseAnd:
  case EggProgramAssign::BitwiseOr:
  case EggProgramAssign::BitwiseXor:
  case EggProgramAssign::ShiftLeft:
  case EggProgramAssign::ShiftRight:
  case EggProgramAssign::ShiftRightUnsigned:
    // Integer-only operation
    if (!egg::lang::Bits::hasAnySet(rsimple, egg::lang::Discriminator::Int)) {
      return this->compilerError(where, "Expected right-hand side of integer '", EggProgram::assignToString(op), "' assignment operator to be 'int', but got '", rtype.toString(), "' instead");
    }
    if (!egg::lang::Bits::hasAnySet(lsimple, egg::lang::Discriminator::Int)) {
      return this->compilerError(where, "Expected left-hand target of integer '", EggProgram::assignToString(op), "' assignment operator to be 'int', but got '", ltype.toString(), "' instead");
    }
    break;
  case EggProgramAssign::Remainder:
  case EggProgramAssign::Multiply:
  case EggProgramAssign::Plus:
  case EggProgramAssign::Minus:
  case EggProgramAssign::Divide:
    // Arithmetic operation
    if (egg::lang::Bits::mask(rsimple, egg::lang::Discriminator::Arithmetic) == egg::lang::Discriminator::Float) {
      // Float-only operation
      if (!egg::lang::Bits::hasAnySet(lsimple, egg::lang::Discriminator::Float)) {
        return this->compilerError(where, "Expected left-hand target of floating-point '", EggProgram::assignToString(op), "' assignment operator to be 'float', but got '", ltype.toString(), "' instead");
      }
    } else {
      // Float-or-int operation
      if (!egg::lang::Bits::hasAnySet(rsimple, egg::lang::Discriminator::Arithmetic)) {
        return this->compilerError(where, "Expected right-hand side of '", EggProgram::assignToString(op), "' assignment operator to be 'int' or 'float', but got '", rtype.toString(), "' instead");
      }
      if (!egg::lang::Bits::hasAnySet(lsimple, egg::lang::Discriminator::Arithmetic)) {
        return this->compilerError(where, "Expected left-hand target of '", EggProgram::assignToString(op), "' assignment operator to be 'int' or 'float', but got '", ltype.toString(), "' instead");
      }
    }
    break;
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareMutate(const egg::lang::LocationSource& where, EggProgramMutate op, IEggProgramNode& lvalue) {
  if (abandoned(lvalue.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto& ltype = *lvalue.getType();
  auto lsimple = ltype.getSimpleTypes();
  assert(lsimple != egg::lang::Discriminator::Inferred);
  switch (op) {
  case EggProgramMutate::Increment:
  case EggProgramMutate::Decrement:
    // Integer-only operation
    if (!egg::lang::Bits::hasAnySet(lsimple, egg::lang::Discriminator::Int)) {
      return this->compilerError(where, "Expected target of integer '", EggProgram::mutateToString(op), "' operator to be 'int', but got '", ltype.toString(), "' instead");
    }
    break;
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareCatch(const egg::lang::String& name, IEggProgramNode& type, IEggProgramNode& block) {
  // TODO
  if (abandoned(type.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  EggProgramSymbolTable nested(this->symtable);
  nested.addSymbol(EggProgramSymbol::ReadWrite, name, *type.getType());
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
  return this->prepareScope(&cond, [&](EggProgramContext& scope) {
    if (abandoned(cond.prepare(scope)) || abandoned(trueBlock.prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    if (falseBlock != nullptr) {
      return falseBlock->prepare(scope);
    }
    return EggProgramNodeFlags::None;
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
    auto* iterable = type->iterable();
    if (iterable == nullptr) {
      return scope.compilerError(rvalue.location(), "Expression after the ':' in 'for' statement is not iterable: '", type->toString(), "'");
    }
    if (abandoned(lvalue.prepareWithType(scope, *iterable))) {
      return EggProgramNodeFlags::Abandon;
    }
    return block.prepare(scope);
  });
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareFunctionDefinition(const egg::lang::String& name, const egg::lang::IType& type, const std::shared_ptr<IEggProgramNode>& block) {
  // TODO type check
  auto callable = type.callable();
  assert(callable != nullptr);
  assert(callable->getFunctionName() == name);
  EggProgramSymbolTable nested(this->symtable);
  auto n = callable->getParameterCount();
  for (size_t i = 0; i < n; ++i) {
    auto& parameter = callable->getParameter(i);
    nested.addSymbol(EggProgramSymbol::ReadWrite, parameter.getName(), parameter.getType());
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

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareSwitch(IEggProgramNode& value, int64_t, const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  // TODO check duplicate constants
  return this->prepareScope(&value, [&](EggProgramContext& scope) {
    if (abandoned(value.prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    for (auto& i : cases) {
      if (abandoned(i->prepare(scope))) {
        return EggProgramNodeFlags::Abandon;
      }
    }
    return EggProgramNodeFlags::None;
  });
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
  return this->prepareScope(&value, [&](EggProgramContext& scope) {
    if (abandoned(value.prepare(scope))) {
      return EggProgramNodeFlags::Abandon;
    }
    return block.prepare(scope);
  });
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

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareIdentifier(const egg::lang::LocationSource& where, const egg::lang::String& name, egg::lang::ITypeRef& type) {
  // We need to work out our type
  assert(type.get() == egg::lang::Type::Void.get());
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return this->compilerError(where, "Unknown identifier: '", name, "'");
  }
  type.set(&symbol->getType());
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareUnary(const egg::lang::LocationSource& where, EggProgramUnary op, IEggProgramNode& value) {
  if (abandoned(value.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto& type = *value.getType();
  auto simple = type.getSimpleTypes();
  assert(simple != egg::lang::Discriminator::Inferred);
  switch (op) {
  case EggProgramUnary::BitwiseNot:
  case EggProgramUnary::LogicalNot:
    // Integer-only operation
    if (!egg::lang::Bits::hasAnySet(simple, egg::lang::Discriminator::Int)) {
      return this->compilerError(where, "Expected operand of unary '", EggProgram::unaryToString(op), "' operator to be 'int', but got '", type.toString(), "' instead");
    }
    break;
  case EggProgramUnary::Negate:
    // Arithmetic operation
    if (!egg::lang::Bits::hasAnySet(simple, egg::lang::Discriminator::Arithmetic)) {
      return this->compilerError(where, "Expected operand of unary '", EggProgram::unaryToString(op), "' operator to be 'int' or 'float', but got '", type.toString(), "' instead");
    }
    break;
  case EggProgramUnary::Ref:
  case EggProgramUnary::Deref:
  case EggProgramUnary::Ellipsis:
    return this->compilerError(where, "Unary '", EggProgram::unaryToString(op), "' operator not yet supported"); // TODO
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareBinary(const egg::lang::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs) {
  switch (op) {
  case EggProgramBinary::LogicalAnd:
  case EggProgramBinary::LogicalOr:
    // Boolean-only operation
    return checkBinary(*this, where, op, egg::lang::Discriminator::Bool, lhs, egg::lang::Discriminator::Bool, rhs);
  case EggProgramBinary::BitwiseAnd:
  case EggProgramBinary::BitwiseOr:
  case EggProgramBinary::BitwiseXor:
  case EggProgramBinary::ShiftLeft:
  case EggProgramBinary::ShiftRight:
  case EggProgramBinary::ShiftRightUnsigned:
    // Integer-only operation
    return checkBinary(*this, where, op, egg::lang::Discriminator::Int, lhs, egg::lang::Discriminator::Int, rhs);
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
    return checkBinary(*this, where, op, egg::lang::Discriminator::Arithmetic, lhs, egg::lang::Discriminator::Arithmetic, rhs);
  case EggProgramBinary::Equal:
  case EggProgramBinary::Unequal:
    // Equality operation
    break;
  case EggProgramBinary::Brackets:
    // Index operation
    return checkBrackets(*this, where, lhs, rhs);
  case EggProgramBinary::Dot:
    // Dot operation (fields)
    return checkDot(*this, where, lhs, rhs);
  case EggProgramBinary::Lambda:
  case EggProgramBinary::NullCoalescing:
    return this->compilerError(where, "'", EggProgram::binaryToString(op), "' operators not yet supported in 'prepareBinary'"); // TODO
  }
  if (abandoned(lhs.prepare(*this)) || abandoned(rhs.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::prepareTernary(const egg::lang::LocationSource& where, IEggProgramNode& cond, IEggProgramNode& whenTrue, IEggProgramNode& whenFalse) {
  // TODO
  if (abandoned(cond.prepare(*this)) || abandoned(whenTrue.prepare(*this)) || abandoned(whenFalse.prepare(*this))) {
    return EggProgramNodeFlags::Abandon;
  }
  auto type = cond.getType();
  if (!type->hasNativeType(egg::lang::Discriminator::Bool)) {
    return this->compilerError(where, "Expected condition of ternary '?:' operator to be 'bool', but got '", type->toString(), "' instead");
  }
  type = whenTrue.getType();
  if (!type->hasNativeType(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null)) {
    return this->compilerError(whenTrue.location(), "Expected value for second operand of ternary '?:' operator , but got '", type->toString(), "' instead");
  }
  type = whenFalse.getType();
  if (!type->hasNativeType(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null)) {
    return this->compilerError(whenTrue.location(), "Expected value for third operand of ternary '?:' operator , but got '", type->toString(), "' instead");
  }
  return EggProgramNodeFlags::None;
}

egg::yolk::EggProgramNodeFlags egg::yolk::EggProgramContext::typeCheck(const egg::lang::LocationSource& where, egg::lang::ITypeRef& ltype, const egg::lang::ITypeRef& rtype, const egg::lang::String& name) {
  assert(rtype->getSimpleTypes() != egg::lang::Discriminator::Inferred);
  if (ltype->getSimpleTypes() == egg::lang::Discriminator::Inferred) {
    // We can infer the type
    ltype = rtype;
    auto symbol = this->symtable->findSymbol(name, false);
    assert(symbol != nullptr);
    symbol->inferType(*rtype);
  }
  if (!ltype->canBeAssignedFrom(*rtype)) {
    return this->compilerError(where, "Cannot initialize '", name, "' of type '", ltype->toString(), "' with a value of type '", rtype->toString(), "'");
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
