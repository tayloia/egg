#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

#include <cmath>

namespace {
  class EggProgramAssigneeIdentifier : public egg::yolk::IEggProgramAssignee {
    EGG_NO_COPY(EggProgramAssigneeIdentifier);
  private:
    egg::yolk::EggProgramContext& context; // WIBBLE out of scope?
    egg::lang::String name;
  public:
    EggProgramAssigneeIdentifier(egg::yolk::EggProgramContext& context, const egg::lang::String& name)
      : context(context), name(name) {
    }
    virtual egg::lang::Value get() const override {
      return this->context.get(this->name, false);
    }
    virtual egg::lang::Value set(const egg::lang::Value& value) override {
      return this->context.set(this->name, value);
    }
  };

  class EggProgramAssigneeInstance : public egg::yolk::IEggProgramAssignee {
    EGG_NO_COPY(EggProgramAssigneeInstance);
  protected:
    egg::yolk::EggProgramContext& context; // WIBBLE out of scope?
    std::shared_ptr<egg::yolk::IEggProgramNode> expression;
    mutable egg::lang::Value instance;
    EggProgramAssigneeInstance(egg::yolk::EggProgramContext& context, const std::shared_ptr<egg::yolk::IEggProgramNode>& expression)
      : context(context), expression(expression), instance() {
    }
    bool evaluateInstance() const {
      if (this->instance.is(egg::lang::Discriminator::Void)) {
        // Need to evaluate the expression
        this->instance = this->expression->execute(this->context).direct();
      }
      return !this->instance.has(egg::lang::Discriminator::FlowControl);
    }
  };

  class EggProgramAssigneeBrackets : public EggProgramAssigneeInstance {
    EGG_NO_COPY(EggProgramAssigneeBrackets);
  private:
    std::shared_ptr<egg::yolk::IEggProgramNode> indexExpression;
    mutable egg::lang::Value index;
  public:
    EggProgramAssigneeBrackets(egg::yolk::EggProgramContext& context, const std::shared_ptr<egg::yolk::IEggProgramNode>& instanceExpression, const std::shared_ptr<egg::yolk::IEggProgramNode>& indexExpression)
      : EggProgramAssigneeInstance(context, instanceExpression), indexExpression(indexExpression), index() {
    }
    virtual egg::lang::Value get() const override {
      // Get the initial value of the indexed entry (probably part of a +=-type construct)
      if (this->evaluateInstance()) {
        if (this->evaluateIndex()) {
          return this->instance.getRuntimeType()->bracketsGet(this->context, this->instance, this->index);
        }
        assert(this->index.has(egg::lang::Discriminator::FlowControl));
        return this->index;
      }
      assert(this->instance.has(egg::lang::Discriminator::FlowControl));
      return this->instance;
    }
    virtual egg::lang::Value set(const egg::lang::Value& value) override {
      // Set the value of the indexed entry
      if (this->evaluateInstance()) {
        if (this->evaluateIndex()) {
          return this->instance.getRuntimeType()->bracketsSet(this->context, this->instance, this->index, value);
        }
        assert(this->index.has(egg::lang::Discriminator::FlowControl));
        return this->index;
      }
      assert(this->instance.has(egg::lang::Discriminator::FlowControl));
      return this->instance;
    }
  private:
    bool evaluateIndex() const {
      if (this->index.is(egg::lang::Discriminator::Void)) {
        // Need to evaluate the index expression
        this->index = this->indexExpression->execute(this->context).direct();
      }
      return !this->index.has(egg::lang::Discriminator::FlowControl);
    }
  };

  class EggProgramAssigneeDot : public EggProgramAssigneeInstance {
    EGG_NO_COPY(EggProgramAssigneeDot);
  private:
    egg::lang::String property;
  public:
    EggProgramAssigneeDot(egg::yolk::EggProgramContext& context, const std::shared_ptr<egg::yolk::IEggProgramNode>& expression, const egg::lang::String& property)
      : EggProgramAssigneeInstance(context, expression), property(property) {
    }
    virtual egg::lang::Value get() const override {
      // Get the initial value of the property (probably part of a +=-type construct)
      if (this->evaluateInstance()) {
        return this->instance.getRuntimeType()->dotGet(this->context, this->instance, property);
      }
      assert(this->instance.has(egg::lang::Discriminator::FlowControl));
      return this->instance;
    }
    virtual egg::lang::Value set(const egg::lang::Value& value) override {
      // Set the value of the property
      if (this->evaluateInstance()) {
        return this->instance.getRuntimeType()->dotSet(this->context, this->instance, property, value);
      }
      assert(this->instance.has(egg::lang::Discriminator::FlowControl));
      return this->instance;
    }
  };

  class EggProgramAssigneeDeref : public EggProgramAssigneeInstance {
    EGG_NO_COPY(EggProgramAssigneeDeref);
  public:
    EggProgramAssigneeDeref(egg::yolk::EggProgramContext& context, const std::shared_ptr<egg::yolk::IEggProgramNode>& expression)
      : EggProgramAssigneeInstance(context, expression) {
    }
    virtual egg::lang::Value get() const override {
      // Get the initial value of the dereferenced value (probably part of a +=-type construct)
      if (this->evaluateInstance()) {
        assert(this->instance.has(egg::lang::Discriminator::Pointer));
        return this->instance.getPointee();
      }
      assert(this->instance.has(egg::lang::Discriminator::FlowControl));
      return this->instance;
    }
    virtual egg::lang::Value set(const egg::lang::Value& value) override {
      // Set the value of the dereferenced value
      if (this->evaluateInstance()) {
        assert(this->instance.has(egg::lang::Discriminator::Pointer));
        egg::lang::Value& pointee = this->instance.getPointee();
        pointee = value;
        return egg::lang::Value::Void;
      }
      assert(this->instance.has(egg::lang::Discriminator::FlowControl));
      return this->instance;
    }
  };
  egg::lang::Value plusInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs + rhs);
  }
  egg::lang::Value minusInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs - rhs);
  }
  egg::lang::Value multiplyInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs * rhs);
  }
  egg::lang::Value divideInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs / rhs);
  }
  egg::lang::Value remainderInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs % rhs);
  }
  egg::lang::Value lessInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs < rhs);
  }
  egg::lang::Value lessEqualInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs <= rhs);
  }
  egg::lang::Value greaterEqualInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs >= rhs);
  }
  egg::lang::Value greaterInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs > rhs);
  }
  egg::lang::Value bitwiseAndInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs & rhs);
  }
  egg::lang::Value bitwiseOrInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs | rhs);
  }
  egg::lang::Value bitwiseXorInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs ^ rhs);
  }
  egg::lang::Value shiftLeftInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs << rhs);
  }
  egg::lang::Value shiftRightInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(lhs >> rhs);
  }
  egg::lang::Value shiftRightUnsignedInt(int64_t lhs, int64_t rhs) {
    return egg::lang::Value(int64_t(uint64_t(lhs) >> rhs));
  }
  egg::lang::Value plusFloat(double lhs, double rhs) {
    return egg::lang::Value(lhs + rhs);
  }
  egg::lang::Value minusFloat(double lhs, double rhs) {
    return egg::lang::Value(lhs - rhs);
  }
  egg::lang::Value multiplyFloat(double lhs, double rhs) {
    return egg::lang::Value(lhs * rhs);
  }
  egg::lang::Value divideFloat(double lhs, double rhs) {
    return egg::lang::Value(lhs / rhs);
  }
  egg::lang::Value remainderFloat(double lhs, double rhs) {
    return egg::lang::Value(std::remainder(lhs, rhs));
  }
  egg::lang::Value lessFloat(double lhs, double rhs) {
    return egg::lang::Value(lhs < rhs);
  }
  egg::lang::Value lessEqualFloat(double lhs, double rhs) {
    return egg::lang::Value(lhs <= rhs);
  }
  egg::lang::Value greaterEqualFloat(double lhs, double rhs) {
    return egg::lang::Value(lhs >= rhs);
  }
  egg::lang::Value greaterFloat(double lhs, double rhs) {
    return egg::lang::Value(lhs > rhs);
  }
}

void egg::yolk::EggProgramSymbol::setInferredType(const egg::lang::IType& inferred) {
  // We only allow inferred type updates
  assert(this->type->getSimpleTypes() == egg::lang::Discriminator::Inferred);
  this->type.set(&inferred);
}

egg::lang::Value egg::yolk::EggProgramSymbol::assign(egg::lang::IExecution& execution, const egg::lang::Value& rhs) {
  // Ask the type to assign the value so that type promotion can occur
  switch (this->kind) {
  case Builtin:
    return execution.raiseFormat("Cannot re-assign built-in value: '", this->name, "'");
  case Readonly:
    return execution.raiseFormat("Cannot modify read-only variable: '", this->name, "'");
  case ReadWrite:
  default:
    break;
  }
  auto promoted = this->type->promoteAssignment(execution, rhs.direct());
  if (promoted.has(egg::lang::Discriminator::FlowControl)) {
    // The assignment failed
    return promoted;
  }
  if (promoted.is(egg::lang::Discriminator::Void)) {
    return execution.raiseFormat("Cannot assign 'void' to '", this->name, "'");
  }
  auto* slot = &this->value;
  if (slot->has(egg::lang::Discriminator::Indirect)) {
    // We're already indirect, so store the value in our child
    slot = &slot->direct();
    *slot = promoted;
  } else {
    *slot = promoted;
  }
  return egg::lang::Value::Void;
}

void egg::yolk::EggProgramSymbolTable::addBuiltins() {
  // TODO add built-in symbol to symbol table here
  this->addBuiltin("string", egg::lang::Value::builtinString());
  this->addBuiltin("type", egg::lang::Value::builtinType());
  this->addBuiltin("assert", egg::lang::Value::builtinAssert());
  this->addBuiltin("print", egg::lang::Value::builtinPrint());
}

void egg::yolk::EggProgramSymbolTable::addBuiltin(const std::string& name, const egg::lang::Value& value) {
  this->addSymbol(EggProgramSymbol::Builtin, egg::lang::String::fromUTF8(name), *value.getRuntimeType(), value);
}

std::shared_ptr<egg::yolk::EggProgramSymbol> egg::yolk::EggProgramSymbolTable::addSymbol(EggProgramSymbol::Kind kind, const egg::lang::String& name, const egg::lang::IType& type, const egg::lang::Value& value) {
  auto result = this->map.emplace(name, std::make_shared<EggProgramSymbol>(kind, name, type, value));
  assert(result.second);
  return result.first->second;
}

std::shared_ptr<egg::yolk::EggProgramSymbol> egg::yolk::EggProgramSymbolTable::findSymbol(const egg::lang::String& name, bool includeParents) const {
  auto found = this->map.find(name);
  if (found != this->map.end()) {
    return found->second;
  }
  if (includeParents && (this->parent != nullptr)) {
    return this->parent->findSymbol(name);
  }
  return nullptr;
}

#define EGG_PROGRAM_OPERATOR_STRING(name, text) text,

std::string egg::yolk::EggProgram::unaryToString(egg::yolk::EggProgramUnary op) {
  static const char* const table[] = {
    EGG_PROGRAM_UNARY_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
    nullptr // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::binaryToString(egg::yolk::EggProgramBinary op) {
  static const char* const table[] = {
    EGG_PROGRAM_BINARY_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
    nullptr // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::assignToString(egg::yolk::EggProgramAssign op) {
  static const char* const table[] = {
    EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
    nullptr // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::mutateToString(egg::yolk::EggProgramMutate op) {
  static const char* const table[] = {
    EGG_PROGRAM_MUTATE_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
    nullptr // Stops GCC 7.3 complaining with error: array subscript is above array bounds [-Werror=array-bounds]
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

void egg::yolk::EggProgramContext::log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) {
  if (severity > *this->maximumSeverity) {
    *this->maximumSeverity = severity;
  }
  this->logger->log(source, severity, message);
}

bool egg::yolk::EggProgramContext::findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Check for duplicate symbols
  bool error = false;
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  std::map<egg::lang::String, egg::lang::LocationSource> seen;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      auto here = statement->location();
      auto already = seen.emplace(std::make_pair(name, here));
      if (!already.second) {
        // Already seen at this level
        this->compiler(egg::lang::LogSeverity::Error, here, "Duplicate symbol declared at module level: '", name, "'");
        this->compiler(egg::lang::LogSeverity::Information, already.first->second, "Previous declaration was here");
        error = true;
      } else if (this->symtable->findSymbol(name) != nullptr) {
        // Seen at an enclosing level
        this->compilerWarning(statement->location(), "Symbol name hides previously declared symbol in enclosing level: '", name, "'");
      }
    }
  }
  return error;
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeIdentifier(const IEggProgramNode& self, const egg::lang::String& name) {
  EggProgramExpression expression(*this, self);
  return std::make_unique<EggProgramAssigneeIdentifier>(*this, name);
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeBrackets(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const std::shared_ptr<IEggProgramNode>& index) {
  EggProgramExpression expression(*this, self);
  return std::make_unique<EggProgramAssigneeBrackets>(*this, instance, index);
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeDot(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const egg::lang::String& property) {
  EggProgramExpression expression(*this, self);
  return std::make_unique<EggProgramAssigneeDot>(*this, instance, property);
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeDeref(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance) {
  EggProgramExpression expression(*this, self);
  return std::make_unique<EggProgramAssigneeDeref>(*this, instance);
}

void egg::yolk::EggProgramContext::statement(const IEggProgramNode& node) {
  // TODO use runtime location, not source location
  egg::lang::LocationSource& source = this->location;
  source = node.location();
}

egg::lang::LocationRuntime egg::yolk::EggProgramContext::swapLocation(const egg::lang::LocationRuntime& loc) {
  egg::lang::LocationRuntime before = this->location;
  this->location = loc;
  return before;
}

egg::lang::Value egg::yolk::EggProgramContext::get(const egg::lang::String& name, bool byref) {
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return this->raiseFormat("Unknown identifier: '", name, "'");
  }
  auto& value = symbol->getValue();
  if (value.direct().is(egg::lang::Discriminator::Void)) {
    return this->raiseFormat("Uninitialized identifier: '", name.toUTF8(), "'");
  }
  if (byref) {
    (void)value.indirect();
  }
  return value;
}

egg::lang::Value egg::yolk::EggProgramContext::set(const egg::lang::String& name, const egg::lang::Value& rvalue) {
  if (rvalue.has(egg::lang::Discriminator::FlowControl)) {
    return rvalue;
  }
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  return symbol->assign(*this, rvalue);
}

egg::lang::Value egg::yolk::EggProgramContext::guard(const egg::lang::String& name, const egg::lang::Value& rvalue) {
  if (rvalue.has(egg::lang::Discriminator::FlowControl)) {
    return rvalue;
  }
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  auto retval = symbol->assign(*this, rvalue);
  if (retval.is(egg::lang::Discriminator::Void)) {
    // The assignment succeeded
    return egg::lang::Value::True;
  }
  return egg::lang::Value::False;
}

egg::lang::Value egg::yolk::EggProgramContext::assign(EggProgramAssign op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  auto dst = lhs.assignee(*this);
  if (dst == nullptr) {
    return this->raiseFormat("Left-hand side of assignment '", EggProgram::assignToString(op), "' operator is not a valid target");
  }
  egg::lang::Value right;
  if (op == EggProgramAssign::Equal) {
    // Simple assignment without interrogation beforehand
    right = rhs.execute(*this).direct();
  } else {
    // We need to interrogate the value of the lhs so we can modify it
    auto left = dst->get().direct();
    if (left.has(egg::lang::Discriminator::FlowControl)) {
      return left;
    }
    switch (op) {
    case EggProgramAssign::Remainder:
      right = this->arithmeticIntFloat(left, right, rhs, "remainder assignment '%='", remainderInt, remainderFloat);
      break;
    case EggProgramAssign::BitwiseAnd:
      right = this->arithmeticInt(left, right, rhs, "bitwise-and assignment '&='", bitwiseAndInt);
      break;
    case EggProgramAssign::Multiply:
      right = this->arithmeticIntFloat(left, right, rhs, "multiplication assignment '*='", multiplyInt, multiplyFloat);
      break;
    case EggProgramAssign::Plus:
      right = this->arithmeticIntFloat(left, right, rhs, "addition assignment '+='", plusInt, plusFloat);
      break;
    case EggProgramAssign::Minus:
      right = this->arithmeticIntFloat(left, right, rhs, "subtraction assignment '-='", minusInt, minusFloat);
      break;
    case EggProgramAssign::Divide:
      right = this->arithmeticIntFloat(left, right, rhs, "division assignment '/='", divideInt, divideFloat);
      break;
    case EggProgramAssign::ShiftLeft:
      right = this->arithmeticInt(left, right, rhs, "shift-left assignment '<<='", shiftLeftInt);
      break;
    case EggProgramAssign::ShiftRight:
      right = this->arithmeticInt(left, right, rhs, "shift-right assignment '>>='", shiftRightInt);
      break;
    case EggProgramAssign::ShiftRightUnsigned:
      right = this->arithmeticInt(left, right, rhs, "shift-right-unsigned assignment '>>>='", shiftRightUnsignedInt);
      break;
    case EggProgramAssign::BitwiseXor:
      right = this->arithmeticInt(left, right, rhs, "bitwise-xor assignment '^='", bitwiseXorInt);
      break;
    case EggProgramAssign::BitwiseOr:
      right = this->arithmeticInt(left, right, rhs, "bitwise-or assignment '|='", bitwiseOrInt);
      break;
    case EggProgramAssign::Equal:
    default:
      return this->raiseFormat("Internal runtime error: Unknown assignment operator: '", EggProgram::assignToString(op), "'");
    }
  }
  if (right.has(egg::lang::Discriminator::FlowControl)) {
    return right;
  }
  return dst->set(right);
}

egg::lang::Value egg::yolk::EggProgramContext::mutate(EggProgramMutate op, const IEggProgramNode& lvalue) {
  auto dst = lvalue.assignee(*this);
  if (dst == nullptr) {
    return this->raiseFormat("Operand of mutation '", EggProgram::mutateToString(op), "' operator is not a valid target");
  }
  auto lhs = dst->get().direct();
  if (lhs.has(egg::lang::Discriminator::FlowControl)) {
    return lhs;
  }
  egg::lang::Value rhs;
  switch (op) {
  case EggProgramMutate::Increment:
    if (!lhs.is(egg::lang::Discriminator::Int)) {
      return this->unexpected("Expected operand of increment '++' operator to be 'int'", lhs);
    }
    rhs = plusInt(lhs.getInt(), 1);
    break;
  case EggProgramMutate::Decrement:
    if (!lhs.is(egg::lang::Discriminator::Int)) {
      return this->unexpected("Expected operand of decrement '--' operator to be 'int'", lhs);
    }
    rhs = minusInt(lhs.getInt(), 1);
    break;
  default:
    return this->raiseFormat("Internal runtime error: Unknown mutation operator: '", EggProgram::mutateToString(op), "'");
  }
  if (rhs.has(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return dst->set(rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::condition(const IEggProgramNode& expression) {
  auto retval = expression.execute(*this).direct();
  if (retval.has(egg::lang::Discriminator::Bool | egg::lang::Discriminator::FlowControl)) {
    return retval;
  }
  return this->raiseFormat("Expected condition to evaluate to a 'bool', but got '", retval.getTagString(), "' instead");
}

egg::lang::Value egg::yolk::EggProgramContext::unary(EggProgramUnary op, const IEggProgramNode& expr, egg::lang::Value& value) {
  switch (op) {
  case EggProgramUnary::LogicalNot:
    if (this->operand(value, expr, egg::lang::Discriminator::Bool, "Expected operand of logical-not '!' operator to be 'bool'")) {
      return egg::lang::Value(!value.getBool());
    }
    return value;
  case EggProgramUnary::Negate:
    if (this->operand(value, expr, egg::lang::Discriminator::Arithmetic, "Expected operand of negation '-' operator to be 'int' or 'float'")) {
      return value.is(egg::lang::Discriminator::Int) ? egg::lang::Value(-value.getInt()) : egg::lang::Value(-value.getFloat());
    }
    return value;
  case EggProgramUnary::BitwiseNot:
    if (this->operand(value, expr, egg::lang::Discriminator::Int, "Expected operand of bitwise-not '~' operator to be 'int'")) {
      return egg::lang::Value(~value.getInt());
    }
    return value;
  case EggProgramUnary::Ref:
    value = expr.execute(*this); // not .direct()
    if (!value.has(egg::lang::Discriminator::FlowControl)) {
      return egg::lang::Value(value.indirect()); // address
    }
    return value;
  case EggProgramUnary::Deref:
    value = expr.execute(*this).direct();
    if (value.has(egg::lang::Discriminator::FlowControl)) {
      return value;
    }
    if (!value.has(egg::lang::Discriminator::Pointer)) {
      return this->unexpected("Expected operand of dereference '*' operator to be a pointer", value);
    }
    return value.getPointee();
  case EggProgramUnary::Ellipsis:
    return this->raiseFormat("TODO unary(...) not fully implemented"); // TODO
  default:
    return this->raiseFormat("Internal runtime error: Unknown unary operator: '", EggProgram::unaryToString(op), "'");
  }
}

egg::lang::Value egg::yolk::EggProgramContext::binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs, egg::lang::Value& left, egg::lang::Value& right) {
  // OPTIMIZE
  left = lhs.execute(*this).direct();
  if (left.has(egg::lang::Discriminator::FlowControl)) {
    return left;
  }
  switch (op) {
  case EggProgramBinary::Unequal:
    if (left.has(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null)) {
      if (!this->operand(right, rhs, egg::lang::Discriminator::Any | egg::lang::Discriminator::Null, "Expected right operand of inequality '!=' to be a value")) {
        return right;
      }
      return egg::lang::Value(left != right);
    }
    return this->unexpected("Expected left operand of inequality '!=' to be a value", left);
  case EggProgramBinary::Remainder:
    return this->arithmeticIntFloat(left, right, rhs, "remainder '%'", remainderInt, remainderFloat);
  case EggProgramBinary::BitwiseAnd:
    return this->arithmeticInt(left, right, rhs, "bitwise-and '&'", bitwiseAndInt);
  case EggProgramBinary::LogicalAnd:
    if (left.is(egg::lang::Discriminator::Bool)) {
      if (!left.getBool()) {
        return left;
      }
      (void)this->operand(right, rhs, egg::lang::Discriminator::Bool, "Expected right operand of logical-and '&&' to be 'bool'");
      return right;
    }
    return this->unexpected("Expected left operand of logical-and '&&' to be 'bool'", left);
  case EggProgramBinary::Multiply:
    return this->arithmeticIntFloat(left, right, rhs, "multiplication '*'", multiplyInt, multiplyFloat);
  case EggProgramBinary::Plus:
    return this->arithmeticIntFloat(left, right, rhs, "addition '+'", plusInt, plusFloat);
  case EggProgramBinary::Minus:
    return this->arithmeticIntFloat(left, right, rhs, "subtraction '-'", minusInt, minusFloat);
  case EggProgramBinary::Lambda:
    return this->raiseFormat("TODO binary(Lambda) not fully implemented"); // TODO
  case EggProgramBinary::Divide:
    return this->arithmeticIntFloat(left, right, rhs, "division '/'", divideInt, divideFloat);
  case EggProgramBinary::Less:
    return this->arithmeticIntFloat(left, right, rhs, "comparison '<'", lessInt, lessFloat);
  case EggProgramBinary::ShiftLeft:
    return this->arithmeticInt(left, right, rhs, "shift-left '<<'", shiftLeftInt);
  case EggProgramBinary::LessEqual:
    return this->arithmeticIntFloat(left, right, rhs, "comparison '<='", lessEqualInt, lessEqualFloat);
  case EggProgramBinary::Equal:
    if (left.has(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null)) {
      if (!this->operand(right, rhs, egg::lang::Discriminator::Any | egg::lang::Discriminator::Null, "Expected right operand of equality '==' to be a value")) {
        return right;
      }
      return egg::lang::Value(left == right);
    }
    return this->unexpected("Expected left operand of equality '==' to be a value", left);
  case EggProgramBinary::Greater:
    return this->arithmeticIntFloat(left, right, rhs, "comparison '>'", greaterInt, greaterFloat);
  case EggProgramBinary::GreaterEqual:
    return this->arithmeticIntFloat(left, right, rhs, "comparison '>='", greaterEqualInt, greaterEqualFloat);
  case EggProgramBinary::ShiftRight:
    return this->arithmeticInt(left, right, rhs, "shift-right '>>'", shiftRightInt);
  case EggProgramBinary::ShiftRightUnsigned:
    return this->arithmeticInt(left, right, rhs, "shift-right-unsigned '>>>'", shiftRightUnsignedInt);
  case EggProgramBinary::NullCoalescing:
    return left.is(egg::lang::Discriminator::Null) ? rhs.execute(*this).direct() : left;
  case EggProgramBinary::BitwiseXor:
    return this->arithmeticInt(left, right, rhs, "bitwise-xor '^'", bitwiseXorInt);
  case EggProgramBinary::BitwiseOr:
    return this->arithmeticInt(left, right, rhs, "bitwise-or '|'", bitwiseOrInt);
  case EggProgramBinary::LogicalOr:
    if (left.is(egg::lang::Discriminator::Bool)) {
      if (left.getBool()) {
        return left;
      }
      (void)this->operand(right, rhs, egg::lang::Discriminator::Bool, "Expected right operand of logical-or '||' to be 'bool'");
      return right;
    }
    return this->unexpected("Expected left operand of logical-or '||' to be 'bool'", left);
  default:
    return this->raiseFormat("Internal runtime error: Unknown binary operator: '", EggProgram::binaryToString(op), "'");
  }
}

bool egg::yolk::EggProgramContext::operand(egg::lang::Value& dst, const IEggProgramNode& src, egg::lang::Discriminator expected, const char* expectation) {
  dst = src.execute(*this).direct();
  if (dst.has(egg::lang::Discriminator::FlowControl)) {
    return false;
  }
  if (dst.has(expected)) {
    return true;
  }
  dst = this->unexpected(expectation, dst);
  return false;
}

egg::lang::Value egg::yolk::EggProgramContext::arithmeticIntFloat(const egg::lang::Value& left, egg::lang::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints, ArithmeticFloat floats) {
  assert(!left.has(egg::lang::Discriminator::Indirect));
  if (!left.has(egg::lang::Discriminator::Arithmetic)) {
    return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int' or 'float'", left);
  }
  right = rhs.execute(*this).direct();
  assert(!right.has(egg::lang::Discriminator::Indirect));
  if (right.is(egg::lang::Discriminator::Int)) {
    if (left.is(egg::lang::Discriminator::Int)) {
      return ints(left.getInt(), right.getInt());
    }
    return floats(left.getFloat(), static_cast<double>(right.getInt()));
  }
  if (right.is(egg::lang::Discriminator::Float)) {
    if (left.is(egg::lang::Discriminator::Int)) {
      return floats(static_cast<double>(left.getInt()), right.getFloat());
    }
    return floats(left.getFloat(), right.getFloat());
  }
  if (right.has(egg::lang::Discriminator::FlowControl)) {
    return right;
  }
  return this->unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int' or 'float'", right);
}

egg::lang::Value egg::yolk::EggProgramContext::arithmeticInt(const egg::lang::Value& left, egg::lang::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints) {
  assert(!left.has(egg::lang::Discriminator::Indirect));
  if (!left.is(egg::lang::Discriminator::Int)) {
    return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int'", left);
  }
  right = rhs.execute(*this).direct();
  assert(!right.has(egg::lang::Discriminator::Indirect));
  if (right.is(egg::lang::Discriminator::Int)) {
    return ints(left.getInt(), right.getInt());
  }
  if (right.has(egg::lang::Discriminator::FlowControl)) {
    return right;
  }
  return this->unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int'", right);
}

egg::lang::Value egg::yolk::EggProgramContext::call(const egg::lang::Value& callee, const egg::lang::IParameters& parameters) {
  auto& direct = callee.direct();
  if (!direct.is(egg::lang::Discriminator::Object)) {
    return this->unexpected("Expected function-like expression to be 'object'", direct);
  }
  auto& object = direct.getObject();
  return object.call(*this, parameters);
}

egg::lang::Value egg::yolk::EggProgramContext::unexpected(const std::string& expectation, const egg::lang::Value& value) {
  return this->raiseFormat(expectation, ", but got '", value.getTagString(), "' instead");
}

egg::lang::Value egg::yolk::EggProgramContext::assertion(const egg::lang::Value& predicate) {
  auto& direct = predicate.direct();
  if (!direct.is(egg::lang::Discriminator::Bool)) {
    return this->unexpected("Expected assertion predicate to be 'bool'", direct);
  }
  if (!direct.getBool()) {
    return this->raiseFormat("Assertion is untrue");
  }
  return egg::lang::Value::Void;
}

void egg::yolk::EggProgramContext::print(const std::string& utf8) {
  return this->log(egg::lang::LogSource::User, egg::lang::LogSeverity::Information, utf8);
}
