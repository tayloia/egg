#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

#include <cmath>

namespace {
  class EggProgramExpression {
  private:
    egg::yolk::EggProgramContext* context;
    egg::lang::LocationRuntime before;
  public:
    EggProgramExpression(egg::yolk::EggProgramContext& context, const egg::yolk::IEggProgramNode& node)
      : context(&context) {
      // TODO use runtime location, not source location
      egg::lang::LocationRuntime after(node.location(), egg::lang::String::fromUTF8("TODO()"));
      this->before = this->context->swapLocation(after);
    }
    ~EggProgramExpression() {
      (void)this->context->swapLocation(this->before);
    }
  };

  class EggProgramParametersEmpty : public egg::lang::IParameters {
  public:
    virtual size_t getPositionalCount() const override {
      return 0;
    }
    virtual egg::lang::Value getPositional(size_t) const override {
      return egg::lang::Value::Void;
    }
    virtual size_t getNamedCount() const override {
      return 0;
    }
    virtual egg::lang::String getName(size_t) const override {
      return egg::lang::String::Empty;
    }
    virtual egg::lang::Value getNamed(const egg::lang::String&) const override {
      return egg::lang::Value::Void;
    }
  };
  const EggProgramParametersEmpty parametersEmpty;

  class EggProgramParameters : public egg::lang::IParameters {
  private:
    std::vector<egg::lang::Value> positional;
    std::map<egg::lang::String, egg::lang::Value> named;
  public:
    explicit EggProgramParameters(size_t count) {
      this->positional.reserve(count);
    }
    void addPositional(const egg::lang::Value& value) {
      this->positional.push_back(value);
    }
    void addNamed(const egg::lang::String& name, const egg::lang::Value& value) {
      this->named.emplace(name, value);
    }
    virtual size_t getPositionalCount() const override {
      return this->positional.size();
    }
    virtual egg::lang::Value getPositional(size_t index) const override {
      return this->positional.at(index);
    }
    virtual size_t getNamedCount() const override {
      return this->named.size();
    }
    virtual egg::lang::String getName(size_t index) const override {
      auto iter = this->named.begin();
      std::advance(iter, index);
      return iter->first;
    }
    virtual egg::lang::Value getNamed(const egg::lang::String& name) const override {
      return this->named.at(name);
    }
  };

  class EggProgramFunction : public egg::gc::HardReferenceCounted<egg::lang::IObject> {
    EGG_NO_COPY(EggProgramFunction);
  private:
    egg::yolk::EggProgramContext& program;
    egg::lang::ITypeRef type;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    EggProgramFunction(egg::yolk::EggProgramContext& program, const egg::lang::IType& type, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : HardReferenceCounted(0), program(program), type(&type), block(block) {
      assert(block != nullptr);
    }
    virtual bool dispose() override {
      return false;
    }
    virtual egg::lang::Value toString() const override {
      return egg::lang::Value(egg::lang::String::concat("<", this->type->toString(), ">"));
    }
    virtual egg::lang::Value getRuntimeType() const override {
      return egg::lang::Value(*this->type);
    }
    virtual egg::lang::Value call(egg::lang::IExecution&, const egg::lang::IParameters& parameters) override {
      return this->program.executeFunctionCall(*this->type, parameters, *this->block);
    }
    virtual egg::lang::Value getProperty(egg::lang::IExecution& execution, const egg::lang::String& property) override {
      return execution.raiseFormat(this->type->toString(), " does not support properties such as '.", property, "'");
    }
    virtual egg::lang::Value setProperty(egg::lang::IExecution& execution, const egg::lang::String& property, const egg::lang::Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support properties such as '.", property, "'");
    }
    virtual egg::lang::Value getIndex(egg::lang::IExecution& execution, const egg::lang::Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support indexing with '[]'");
    }
    virtual egg::lang::Value setIndex(egg::lang::IExecution& execution, const egg::lang::Value&, const egg::lang::Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support indexing with '[]'");
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution& execution) override {
      return execution.raiseFormat(this->type->toString(), " does not support iteration");
    }
  };

  class EggProgramAssigneeIdentifier : public egg::yolk::IEggProgramAssignee {
    EGG_NO_COPY(EggProgramAssigneeIdentifier);
  private:
    egg::yolk::EggProgramContext& context;
    egg::lang::String name;
  public:
    EggProgramAssigneeIdentifier(egg::yolk::EggProgramContext& context, const egg::lang::String& name)
      : context(context), name(name) {
    }
    virtual egg::lang::Value get() const override {
      return this->context.get(this->name);
    }
    virtual egg::lang::Value set(const egg::lang::Value& value) override {
      return this->context.set(this->name, value);
    }
  };

  class EggProgramAssigneeInstance : public egg::yolk::IEggProgramAssignee {
    EGG_NO_COPY(EggProgramAssigneeInstance);
  protected:
    egg::yolk::EggProgramContext& context;
    std::shared_ptr<egg::yolk::IEggProgramNode> expression;
    mutable egg::lang::Value instance;
    EggProgramAssigneeInstance(egg::yolk::EggProgramContext& context, const std::shared_ptr<egg::yolk::IEggProgramNode>& expression)
      : context(context), expression(expression), instance() {
    }
    bool evaluateInstance() const {
      if (this->instance.is(egg::lang::Discriminator::Void)) {
        // Need to evaluate the expression
        this->instance = this->expression->execute(this->context);
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
          return this->instance.getRuntimeType().bracketsGet(this->context, this->instance, this->index);
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
          return this->instance.getRuntimeType().bracketsSet(this->context, this->instance, this->index, value);
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
        this->index = this->indexExpression->execute(this->context);
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
        return this->instance.getRuntimeType().dotGet(this->context, this->instance, property);
      }
      assert(this->instance.has(egg::lang::Discriminator::FlowControl));
      return this->instance;
    }
    virtual egg::lang::Value set(const egg::lang::Value& value) override {
      // Set the value of the property
      if (this->evaluateInstance()) {
        return this->instance.getRuntimeType().dotSet(this->context, this->instance, property, value);
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

egg::lang::Value egg::yolk::EggProgramSymbolTable::Symbol::assign(egg::lang::IExecution& execution, const egg::lang::Value& rhs) {
  // Ask the type to assign the value so that type promotion can occur
  egg::lang::String problem;
  auto promoted = this->type->promoteAssignment(execution, rhs);
  if (promoted.has(egg::lang::Discriminator::FlowControl)) {
    // The assignment failed
    return promoted;
  }
  this->value = promoted;
  return egg::lang::Value::Void;
}

void egg::yolk::EggProgramSymbolTable::addBuiltins() {
  // TODO add built-in symbol to symbol table here
  this->addBuiltin("assert", egg::lang::Value::builtinAssert());
  this->addBuiltin("print", egg::lang::Value::builtinPrint());
}

void egg::yolk::EggProgramSymbolTable::addBuiltin(const std::string& name, const egg::lang::Value& value) {
  this->addSymbol(egg::lang::String::fromUTF8(name), value.getRuntimeType())->value = value;
}

std::shared_ptr<egg::yolk::EggProgramSymbolTable::Symbol> egg::yolk::EggProgramSymbolTable::addSymbol(const egg::lang::String& name, const egg::lang::IType& type) {
  auto result = this->map.emplace(name, std::make_shared<Symbol>(name, type));
  assert(result.second);
  return result.first->second;
}

std::shared_ptr<egg::yolk::EggProgramSymbolTable::Symbol> egg::yolk::EggProgramSymbolTable::findSymbol(const egg::lang::String& name, bool includeParents) const {
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

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeDot(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const std::shared_ptr<IEggProgramNode>& property) {
  EggProgramExpression expression(*this, self);
  auto name = property->execute(*this);
  if (name.is(egg::lang::Discriminator::String)) {
    return std::make_unique<EggProgramAssigneeDot>(*this, instance, name.getString());
  }
  return nullptr; // TODO error message propagation
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

egg::lang::Value egg::yolk::EggProgramContext::get(const egg::lang::String& name) {
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return this->raiseFormat("Unknown identifier: '", name, "'");
  }
  if (symbol->value.is(egg::lang::Discriminator::Void)) {
    return this->raiseFormat("Uninitialized identifier: '", name.toUTF8(), "'");
  }
  return symbol->value;
}

egg::lang::Value egg::yolk::EggProgramContext::set(const egg::lang::String& name, const egg::lang::Value& rvalue) {
  if (rvalue.has(egg::lang::Discriminator::FlowControl)) {
    return rvalue;
  }
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  return symbol->assign(*this, rvalue);
}

egg::lang::Value egg::yolk::EggProgramContext::assign(EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue) {
  auto dst = lvalue.assignee(*this);
  if (dst == nullptr) {
    return this->raiseFormat("Left-hand side of assignment operator '", EggProgram::assignToString(op), "' is not a valid target");
  }
  egg::lang::Value rhs;
  if (op == EggProgramAssign::Equal) {
    // Simple assignment without interrogation beforehand
    rhs = rvalue.execute(*this);
  } else {
    // We need to interrogate the value of the lhs so we can modify it
    auto lhs = dst->get();
    if (lhs.has(egg::lang::Discriminator::FlowControl)) {
      return lhs;
    }
    switch (op) {
    case EggProgramAssign::Remainder:
      rhs = arithmeticIntFloat(lhs, rvalue, "remainder assignment '%='", remainderInt, remainderFloat);
      break;
    case EggProgramAssign::BitwiseAnd:
      rhs = arithmeticInt(lhs, rvalue, "bitwise-and assignment '&='", bitwiseAndInt);
      break;
    case EggProgramAssign::Multiply:
      rhs = arithmeticIntFloat(lhs, rvalue, "multiplication assignment '*='", multiplyInt, multiplyFloat);
      break;
    case EggProgramAssign::Plus:
      rhs = arithmeticIntFloat(lhs, rvalue, "addition assignment '+='", plusInt, plusFloat);
      break;
    case EggProgramAssign::Minus:
      rhs = arithmeticIntFloat(lhs, rvalue, "subtraction assignment '-='", minusInt, minusFloat);
      break;
    case EggProgramAssign::Divide:
      rhs = arithmeticIntFloat(lhs, rvalue, "division assignment '/='", divideInt, divideFloat);
      break;
    case EggProgramAssign::ShiftLeft:
      rhs = arithmeticInt(lhs, rvalue, "shift-left assignment '<<='", shiftLeftInt);
      break;
    case EggProgramAssign::ShiftRight:
      rhs = arithmeticInt(lhs, rvalue, "shift-right assignment '>>='", shiftRightInt);
      break;
    case EggProgramAssign::ShiftRightUnsigned:
      rhs = arithmeticInt(lhs, rvalue, "shift-right-unsigned assignment '>>>='", shiftRightUnsignedInt);
      break;
    case EggProgramAssign::BitwiseXor:
      rhs = arithmeticInt(lhs, rvalue, "bitwise-xor assignment '^='", bitwiseXorInt);
      break;
    case EggProgramAssign::BitwiseOr:
      rhs = arithmeticInt(lhs, rvalue, "bitwise-or assignment '|='", bitwiseOrInt);
      break;
    case EggProgramAssign::Equal:
    default:
      return this->raiseFormat("Internal runtime error: Unknown assignment operator: '", EggProgram::assignToString(op), "'");
    }
  }
  if (rhs.has(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return dst->set(rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::mutate(EggProgramMutate op, const IEggProgramNode& lvalue) {
  auto dst = lvalue.assignee(*this);
  if (dst == nullptr) {
    return this->raiseFormat("Operand of mutation operator '", EggProgram::mutateToString(op), "' is not a valid target");
  }
  auto lhs = dst->get();
  if (lhs.has(egg::lang::Discriminator::FlowControl)) {
    return lhs;
  }
  egg::lang::Value rhs;
  switch (op) {
  case EggProgramMutate::Increment:
    if (!lhs.is(egg::lang::Discriminator::Int)) {
      return this->unexpected("Expected operand of increment operator '++' to be 'int'", lhs);
    }
    rhs = plusInt(lhs.getInt(), 1);
    break;
  case EggProgramMutate::Decrement:
    if (!lhs.is(egg::lang::Discriminator::Int)) {
      return this->unexpected("Expected operand of decrement operator '--' to be 'int'", lhs);
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
  auto retval = expression.execute(*this);
  if (retval.has(egg::lang::Discriminator::Bool | egg::lang::Discriminator::FlowControl)) {
    return retval;
  }
  return this->raiseFormat("Expected condition to evaluate to a 'bool', but got '", retval.getTagString(), "' instead");
}

egg::lang::Value egg::yolk::EggProgramContext::unary(EggProgramUnary op, const IEggProgramNode& value) {
  egg::lang::Value result;
  switch (op) {
  case EggProgramUnary::LogicalNot:
    if (this->operand(result, value, egg::lang::Discriminator::Bool, "Expected operand of logical-not operator '!' to be 'bool'")) {
      return egg::lang::Value(!result.getBool());
    }
    return result;
  case EggProgramUnary::Negate:
    if (this->operand(result, value, egg::lang::Discriminator::Arithmetic, "Expected operand of negation operator '-' to be 'int' or 'float'")) {
      return result.is(egg::lang::Discriminator::Int) ? egg::lang::Value(-result.getInt()) : egg::lang::Value(-result.getFloat());
    }
    return result;
  case EggProgramUnary::BitwiseNot:
    if (this->operand(result, value, egg::lang::Discriminator::Int, "Expected operand of bitwise-not operator '~' to be 'int'")) {
      return egg::lang::Value(~result.getInt());
    }
    return result;
  case EggProgramUnary::Ref:
  case EggProgramUnary::Deref:
  case EggProgramUnary::Ellipsis:
    return this->raiseFormat("TODO unary() not fully implemented"); // TODO
  default:
    return this->raiseFormat("Internal runtime error: Unknown unary operator: '", EggProgram::unaryToString(op), "'");
  }
}

egg::lang::Value egg::yolk::EggProgramContext::binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  // OPTIMIZE
  egg::lang::Value left = lhs.execute(*this);
  if (left.has(egg::lang::Discriminator::FlowControl)) {
    return left;
  }
  switch (op) {
  case EggProgramBinary::Unequal:
    if (left.has(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null)) {
      egg::lang::Value right;
      if (!this->operand(right, rhs, egg::lang::Discriminator::Any | egg::lang::Discriminator::Null, "Expected right operand of inequality '!=' to be a value")) {
        return right;
      }
      return egg::lang::Value(left != right);
    }
    return this->unexpected("Expected left operand of inequality '!=' to be a value", left);
  case EggProgramBinary::Remainder:
    return this->arithmeticIntFloat(left, rhs, "remainder '%'", remainderInt, remainderFloat);
  case EggProgramBinary::BitwiseAnd:
    return this->arithmeticInt(left, rhs, "bitwise-and '&'", bitwiseAndInt);
  case EggProgramBinary::LogicalAnd:
    if (left.is(egg::lang::Discriminator::Bool)) {
      if (!left.getBool()) {
        return left;
      }
      egg::lang::Value right;
      (void)this->operand(right, rhs, egg::lang::Discriminator::Bool, "Expected right operand of logical-and '&&' to be 'bool'");
      return right;
    }
    return this->unexpected("Expected left operand of logical-and '&&' to be 'bool'", left);
  case EggProgramBinary::Multiply:
    return this->arithmeticIntFloat(left, rhs, "multiplication '*'", multiplyInt, multiplyFloat);
  case EggProgramBinary::Plus:
    return this->arithmeticIntFloat(left, rhs, "addition '+'", plusInt, plusFloat);
  case EggProgramBinary::Minus:
    return this->arithmeticIntFloat(left, rhs, "subtraction '-'", minusInt, minusFloat);
  case EggProgramBinary::Lambda:
    return this->raiseFormat("TODO binary(Lambda) not fully implemented"); // TODO
  case EggProgramBinary::Dot:
    return this->operatorDot(left, rhs);
  case EggProgramBinary::Divide:
    return this->arithmeticIntFloat(left, rhs, "division '/'", divideInt, divideFloat);
  case EggProgramBinary::Less:
    return this->arithmeticIntFloat(left, rhs, "comparison '<'", lessInt, lessFloat);
  case EggProgramBinary::ShiftLeft:
    return this->arithmeticInt(left, rhs, "shift-left '<<'", shiftLeftInt);
  case EggProgramBinary::LessEqual:
    return this->arithmeticIntFloat(left, rhs, "comparison '<='", lessEqualInt, lessEqualFloat);
  case EggProgramBinary::Equal:
    if (left.has(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null)) {
      egg::lang::Value right;
      if (!this->operand(right, rhs, egg::lang::Discriminator::Any | egg::lang::Discriminator::Null, "Expected right operand of equality '==' to be a value")) {
        return right;
      }
      return egg::lang::Value(left == right);
    }
    return this->unexpected("Expected left operand of equality '==' to be a value", left);
  case EggProgramBinary::Greater:
    return this->arithmeticIntFloat(left, rhs, "comparison '>'", greaterInt, greaterFloat);
  case EggProgramBinary::GreaterEqual:
    return this->arithmeticIntFloat(left, rhs, "comparison '>='", greaterEqualInt, greaterEqualFloat);
  case EggProgramBinary::ShiftRight:
    return this->arithmeticInt(left, rhs, "shift-right '>>'", shiftRightInt);
  case EggProgramBinary::ShiftRightUnsigned:
    return this->arithmeticInt(left, rhs, "shift-right-unsigned '>>>'", shiftRightUnsignedInt);
  case EggProgramBinary::NullCoalescing:
    return left.is(egg::lang::Discriminator::Null) ? rhs.execute(*this) : left;
  case EggProgramBinary::Brackets:
    return this->operatorBrackets(left, rhs);
  case EggProgramBinary::BitwiseXor:
    return this->arithmeticInt(left, rhs, "bitwise-xor '^'", bitwiseXorInt);
  case EggProgramBinary::BitwiseOr:
    return this->arithmeticInt(left, rhs, "bitwise-or '|'", bitwiseOrInt);
  case EggProgramBinary::LogicalOr:
    if (left.is(egg::lang::Discriminator::Bool)) {
      if (left.getBool()) {
        return left;
      }
      egg::lang::Value right;
      (void)this->operand(right, rhs, egg::lang::Discriminator::Bool, "Expected right operand of logical-or '||' to be 'bool'");
      return right;
    }
    return this->unexpected("Expected left operand of logical-or '||' to be 'bool'", left);
  default:
    return this->raiseFormat("Internal runtime error: Unknown binary operator: '", EggProgram::binaryToString(op), "'");
  }
}

bool egg::yolk::EggProgramContext::operand(egg::lang::Value& dst, const IEggProgramNode& src, egg::lang::Discriminator expected, const char* expectation) {
  dst = src.execute(*this);
  if (dst.has(egg::lang::Discriminator::FlowControl)) {
    return false;
  }
  if (dst.has(expected)) {
    return true;
  }
  dst = this->unexpected(expectation, dst);
  return false;
}

egg::lang::Value egg::yolk::EggProgramContext::operatorDot(const egg::lang::Value& lhs, const IEggProgramNode& rhs) {
  auto property = rhs.execute(*this);
  if (!property.is(egg::lang::Discriminator::String)) {
    if (!property.has(egg::lang::Discriminator::FlowControl)) {
      return this->unexpected("Expected right-hand side of '.' operator to be a property name", property);
    }
    return property;
  }
  return lhs.getRuntimeType().dotGet(*this, lhs, property.getString());
}

egg::lang::Value egg::yolk::EggProgramContext::operatorBrackets(const egg::lang::Value& lhs, const IEggProgramNode& rhs) {
  // Override our location with the index value
  this->location.column++; // TODO a better way of doing this?
  auto index = rhs.execute(*this);
  return lhs.getRuntimeType().bracketsGet(*this, lhs, index);
}

egg::lang::Value egg::yolk::EggProgramContext::arithmeticIntFloat(const egg::lang::Value& lhs, const IEggProgramNode& rvalue, const char* operation, ArithmeticInt ints, ArithmeticFloat floats) {
  if (!lhs.has(egg::lang::Discriminator::Arithmetic)) {
    return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int' or 'float'", lhs);
  }
  egg::lang::Value rhs = rvalue.execute(*this);
  if (rhs.is(egg::lang::Discriminator::Int)) {
    if (lhs.is(egg::lang::Discriminator::Int)) {
      return ints(lhs.getInt(), rhs.getInt());
    }
    return floats(lhs.getFloat(), static_cast<double>(rhs.getInt()));
  }
  if (rhs.is(egg::lang::Discriminator::Float)) {
    if (lhs.is(egg::lang::Discriminator::Int)) {
      return floats(static_cast<double>(lhs.getInt()), rhs.getFloat());
    }
    return floats(lhs.getFloat(), rhs.getFloat());
  }
  if (rhs.has(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return this->unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int' or 'float'", rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::arithmeticInt(const egg::lang::Value& lhs, const IEggProgramNode& rvalue, const char* operation, ArithmeticInt ints) {
  if (!lhs.is(egg::lang::Discriminator::Int)) {
    return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int'", lhs);
  }
  egg::lang::Value rhs = rvalue.execute(*this);
  if (rhs.is(egg::lang::Discriminator::Int)) {
    return ints(lhs.getInt(), rhs.getInt());
  }
  if (rhs.has(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return this->unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int'", rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::call(const egg::lang::Value& callee, const egg::lang::IParameters& parameters) {
  if (!callee.is(egg::lang::Discriminator::Object)) {
    return this->unexpected("Expected function-like expression to be 'object'", callee);
  }
  auto& object = callee.getObject();
  return object.call(*this, parameters);
}

egg::lang::Value egg::yolk::EggProgramContext::cast(egg::lang::Discriminator tag, const egg::lang::IParameters& parameters) {
  auto* native = egg::lang::Type::getNative(tag);
  if (native == nullptr) {
    return this->raiseFormat("Cast to non-trivial type is not supported: '", egg::lang::Value::getTagString(tag), "'");
  }
  return native->cast(*this, parameters);
}

egg::lang::Value egg::yolk::EggProgramContext::unexpected(const std::string& expectation, const egg::lang::Value& value) {
  return this->raiseFormat(expectation, ", but got '", value.getTagString(), "' instead");
}

egg::lang::Value egg::yolk::EggProgramContext::assertion(const egg::lang::Value& predicate) {
  if (!predicate.is(egg::lang::Discriminator::Bool)) {
    return this->unexpected("Expected predicate to be a 'bool'", predicate);
  }
  if (!predicate.getBool()) {
    return this->raiseFormat("Predicate is false");
  }
  return egg::lang::Value::Void;
}

void egg::yolk::EggProgramContext::print(const std::string& utf8) {
  return this->log(egg::lang::LogSource::User, egg::lang::LogSeverity::Information, utf8);
}
