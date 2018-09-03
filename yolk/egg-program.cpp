#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"
#include "ovum/test/ctest.h"

#include <cmath>

namespace {
  // The yolk system expects a shared_ptr to a logger, so we use this adapter for testing
  class Relogger : public egg::ovum::ILogger {
  private:
    ILogger* logger;
  public:
    explicit Relogger(ILogger& logger) : logger(&logger) {
    }
    virtual void log(Source source, Severity severity, const std::string & message) override {
      this->logger->log(source, severity, message);
    }
  };

  egg::ovum::Module compileModule(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, egg::yolk::TextStream& stream) {
    auto relogger = std::make_shared<Relogger>(logger);
    auto engine = egg::yolk::EggEngineFactory::createEngineFromTextStream(stream);
    auto preparation = egg::yolk::EggEngineFactory::createPreparationContext(allocator, relogger);
    if (engine->prepare(*preparation) == egg::ovum::ILogger::Severity::Error) {
      return nullptr;
    }
    auto compilation = egg::yolk::EggEngineFactory::createCompilationContext(allocator, relogger);
    egg::ovum::Module module;
    if (engine->compile(*compilation, module) == egg::ovum::ILogger::Severity::Error) {
      return nullptr;
    }
    return module;
  }

  class EggProgramAssigneeIdentifier : public egg::yolk::IEggProgramAssignee {
  private:
    egg::yolk::EggProgramContext* program;
    egg::ovum::String name;
  public:
    EggProgramAssigneeIdentifier(egg::yolk::EggProgramContext& program, const egg::ovum::String& name)
      : program(&program),
        name(name) {
    }
    virtual egg::ovum::Variant get() const override {
      return this->program->get(this->name, false);
    }
    virtual egg::ovum::Variant set(const egg::ovum::Variant& value) override {
      return this->program->set(this->name, value);
    }
  };

  class EggProgramAssigneeInstance : public egg::yolk::IEggProgramAssignee {
  protected:
    egg::yolk::EggProgramContext* program;
    std::shared_ptr<egg::yolk::IEggProgramNode> expression;
    mutable egg::ovum::Variant instance;
    EggProgramAssigneeInstance(egg::yolk::EggProgramContext& program, const std::shared_ptr<egg::yolk::IEggProgramNode>& expression)
      : program(&program),
        expression(expression),
        instance() {
    }
    bool evaluateInstance() const {
      if (this->instance.isVoid()) {
        // Need to evaluate the expression
        this->instance = this->expression->execute(*this->program).direct();
      }
      return !this->instance.hasFlowControl();
    }
  };

  class EggProgramAssigneeBrackets : public EggProgramAssigneeInstance {
    EGG_NO_COPY(EggProgramAssigneeBrackets);
  private:
    std::shared_ptr<egg::yolk::IEggProgramNode> indexExpression;
    mutable egg::ovum::Variant index;
  public:
    EggProgramAssigneeBrackets(egg::yolk::EggProgramContext& context, const std::shared_ptr<egg::yolk::IEggProgramNode>& instanceExpression, const std::shared_ptr<egg::yolk::IEggProgramNode>& indexExpression)
      : EggProgramAssigneeInstance(context, instanceExpression), indexExpression(indexExpression), index() {
    }
    virtual egg::ovum::Variant get() const override {
      // Get the initial value of the indexed entry (probably part of a +=-type construct)
      if (this->evaluateInstance()) {
        if (this->evaluateIndex()) {
          return this->program->bracketsGet(this->instance, this->index);
        }
        assert(this->index.hasFlowControl());
        return this->index;
      }
      assert(this->instance.hasFlowControl());
      return this->instance;
    }
    virtual egg::ovum::Variant set(const egg::ovum::Variant& value) override {
      // Set the value of the indexed entry
      if (this->evaluateInstance()) {
        if (this->evaluateIndex()) {
          return this->program->bracketsSet(this->instance, this->index, value);
        }
        assert(this->index.hasFlowControl());
        return this->index;
      }
      assert(this->instance.hasFlowControl());
      return this->instance;
    }
  private:
    bool evaluateIndex() const {
      if (this->index.isVoid()) {
        // Need to evaluate the index expression
        this->index = this->indexExpression->execute(*this->program).direct();
      }
      return !this->index.hasFlowControl();
    }
  };

  class EggProgramAssigneeDot : public EggProgramAssigneeInstance {
    EGG_NO_COPY(EggProgramAssigneeDot);
  private:
    egg::ovum::String property;
  public:
    EggProgramAssigneeDot(egg::yolk::EggProgramContext& context, const std::shared_ptr<egg::yolk::IEggProgramNode>& expression, const egg::ovum::String& property)
      : EggProgramAssigneeInstance(context, expression), property(property) {
    }
    virtual egg::ovum::Variant get() const override {
      // Get the initial value of the property (probably part of a +=-type construct)
      if (this->evaluateInstance()) {
        return this->program->dotGet(this->instance, property);
      }
      assert(this->instance.hasFlowControl());
      return this->instance;
    }
    virtual egg::ovum::Variant set(const egg::ovum::Variant& value) override {
      // Set the value of the property
      if (this->evaluateInstance()) {
        return this->program->dotSet(this->instance, property, value);
      }
      assert(this->instance.hasFlowControl());
      return this->instance;
    }
  };

  class EggProgramAssigneeDeref : public EggProgramAssigneeInstance {
    EGG_NO_COPY(EggProgramAssigneeDeref);
  public:
    EggProgramAssigneeDeref(egg::yolk::EggProgramContext& context, const std::shared_ptr<egg::yolk::IEggProgramNode>& expression)
      : EggProgramAssigneeInstance(context, expression) {
    }
    virtual egg::ovum::Variant get() const override {
      // Get the initial value of the dereferenced value (probably part of a +=-type construct)
      if (this->evaluateInstance()) {
        assert(this->instance.hasPointer());
        return this->instance.getPointee();
      }
      assert(this->instance.hasFlowControl());
      return this->instance;
    }
    virtual egg::ovum::Variant set(const egg::ovum::Variant& value) override {
      // Set the value of the dereferenced value
      if (this->evaluateInstance()) {
        assert(this->instance.hasPointer());
        egg::ovum::Variant& pointee = this->instance.getPointee();
        pointee = value;
        return egg::ovum::Variant::Void;
      }
      assert(this->instance.hasFlowControl());
      return this->instance;
    }
  };
  egg::ovum::Variant plusInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs + rhs);
  }
  egg::ovum::Variant minusInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs - rhs);
  }
  egg::ovum::Variant multiplyInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs * rhs);
  }
  egg::ovum::Variant divideInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs / rhs);
  }
  egg::ovum::Variant remainderInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs % rhs);
  }
  egg::ovum::Variant lessInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs < rhs);
  }
  egg::ovum::Variant lessEqualInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs <= rhs);
  }
  egg::ovum::Variant greaterEqualInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs >= rhs);
  }
  egg::ovum::Variant greaterInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs > rhs);
  }
  egg::ovum::Variant bitwiseAndBool(bool lhs, bool rhs) {
    return egg::ovum::Variant(bool(lhs & rhs));
  }
  egg::ovum::Variant bitwiseAndInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs & rhs);
  }
  egg::ovum::Variant bitwiseOrBool(bool lhs, bool rhs) {
    return egg::ovum::Variant(bool(lhs | rhs));
  }
  egg::ovum::Variant bitwiseOrInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs | rhs);
  }
  egg::ovum::Variant bitwiseXorBool(bool lhs, bool rhs) {
    return egg::ovum::Variant(bool(lhs ^ rhs));
  }
  egg::ovum::Variant bitwiseXorInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs ^ rhs);
  }
  egg::ovum::Variant shiftLeftInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs << rhs);
  }
  egg::ovum::Variant shiftRightInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(lhs >> rhs);
  }
  egg::ovum::Variant shiftRightUnsignedInt(int64_t lhs, int64_t rhs) {
    return egg::ovum::Variant(int64_t(uint64_t(lhs) >> rhs));
  }
  egg::ovum::Variant plusFloat(double lhs, double rhs) {
    return egg::ovum::Variant(lhs + rhs);
  }
  egg::ovum::Variant minusFloat(double lhs, double rhs) {
    return egg::ovum::Variant(lhs - rhs);
  }
  egg::ovum::Variant multiplyFloat(double lhs, double rhs) {
    return egg::ovum::Variant(lhs * rhs);
  }
  egg::ovum::Variant divideFloat(double lhs, double rhs) {
    return egg::ovum::Variant(lhs / rhs);
  }
  egg::ovum::Variant remainderFloat(double lhs, double rhs) {
    return egg::ovum::Variant(std::remainder(lhs, rhs));
  }
  egg::ovum::Variant lessFloat(double lhs, double rhs) {
    return egg::ovum::Variant(lhs < rhs);
  }
  egg::ovum::Variant lessEqualFloat(double lhs, double rhs) {
    return egg::ovum::Variant(lhs <= rhs);
  }
  egg::ovum::Variant greaterEqualFloat(double lhs, double rhs) {
    return egg::ovum::Variant(lhs >= rhs);
  }
  egg::ovum::Variant greaterFloat(double lhs, double rhs) {
    return egg::ovum::Variant(lhs > rhs);
  }
}

void egg::yolk::EggProgramSymbol::setInferredType(const egg::ovum::Type& inferred) {
  // We only allow inferred type updates
  assert(this->type == nullptr);
  this->type = inferred;
}

egg::ovum::Variant egg::yolk::EggProgramSymbol::assign(EggProgramContext& context, const egg::ovum::Variant& rhs) {
  // Ask the type to assign the value so that type promotion can occur
  switch (this->kind) {
  case Builtin:
    return context.raiseFormat("Cannot re-assign built-in value: '", this->name, "'");
  case Readonly:
    return context.raiseFormat("Cannot modify read-only variable: '", this->name, "'");
  case ReadWrite:
  default:
    break;
  }
  assert(!rhs.hasFlowControl());
  if (rhs.isVoid()) {
    return context.raiseFormat("Cannot assign 'void' to '", this->name, "'");
  }
  egg::ovum::Variant* target;
  if (this->value.hasIndirect()) {
    // We're already indirect, so store the value in our child
    target = &this->value.getPointee();
  } else {
    // Store the value directly
    target = &this->value;
  }
  auto retval = this->type->tryAssign(*target, rhs);
  if (retval.hasFlowControl()) {
    // The assignment failed
    if (retval.hasString()) {
      // Convert the error message to a full-blown exception
      return context.raise(retval.getString());
    }
    return retval;
  }
  auto* basket = context.softGetBasket();
  assert(basket != nullptr);
  this->value.soften(*basket);
  return egg::ovum::Variant::Void;
}

void egg::yolk::EggProgramSymbolTable::softVisitLinks(const Visitor& visitor) const {
  // Visit all our soft links
  this->parent.visit(visitor);
  for (auto& kv : this->map) {
    auto& symbol = *kv.second;
    symbol.getValue().softVisitLink(visitor);
  }
}

void egg::yolk::EggProgramSymbolTable::addBuiltins() {
  // TODO add built-in symbol to symbol table here
  this->addBuiltin("string", Builtins::builtinString(this->allocator));
  this->addBuiltin("type", Builtins::builtinType(this->allocator));
  this->addBuiltin("assert", Builtins::builtinAssert(this->allocator));
  this->addBuiltin("print", Builtins::builtinPrint(this->allocator));
}

void egg::yolk::EggProgramSymbolTable::addBuiltin(const std::string& name, const egg::ovum::Variant& value) {
  (void)this->addSymbol(EggProgramSymbol::Builtin, name, value.getRuntimeType(), value);
}

std::shared_ptr<egg::yolk::EggProgramSymbol> egg::yolk::EggProgramSymbolTable::addSymbol(EggProgramSymbol::Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type, const egg::ovum::Variant& value) {
  auto result = this->map.emplace(name, std::make_shared<EggProgramSymbol>(kind, name, type, value));
  assert(result.second);
  auto symbol = result.first->second;
  symbol->getValue().soften(*this->softGetBasket());
  return symbol;
}

std::shared_ptr<egg::yolk::EggProgramSymbol> egg::yolk::EggProgramSymbolTable::findSymbol(const egg::ovum::String& name, bool includeParents) const {
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

egg::ovum::HardPtr<egg::yolk::EggProgramContext> egg::yolk::EggProgram::createRootContext(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity& maximumSeverity) {
  egg::ovum::LocationRuntime location(this->root->location(), "<module>");
  return allocator.make<EggProgramContext>(location, logger, symtable, maximumSeverity);
}

void egg::yolk::EggProgramContext::softVisitLinks(const Visitor& visitor) const {
  this->symtable.visit(visitor);
}

egg::ovum::HardPtr<egg::yolk::EggProgramContext> egg::yolk::EggProgramContext::createNestedContext(EggProgramSymbolTable& parent, ScopeFunction* prepareFunction) {
  return this->allocator.make<EggProgramContext>(*this, parent, prepareFunction);
}

void egg::yolk::EggProgramContext::log(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, const std::string& message) {
  if (severity > *this->maximumSeverity) {
    *this->maximumSeverity = severity;
  }
  this->logger->log(source, severity, message);
}

bool egg::yolk::EggProgramContext::findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Check for duplicate symbols
  bool error = false;
  egg::ovum::String name;
  auto type = egg::ovum::Type::Void;
  std::map<egg::ovum::String, egg::ovum::LocationSource> seen;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      auto here = statement->location();
      auto already = seen.emplace(std::make_pair(name, here));
      if (!already.second) {
        // Already seen at this level
        this->compiler(egg::ovum::ILogger::Severity::Error, here, "Duplicate symbol declared at module level: '", name, "'");
        this->compiler(egg::ovum::ILogger::Severity::Information, already.first->second, "Previous declaration was here");
        error = true;
      } else if (this->symtable->findSymbol(name) != nullptr) {
        // Seen at an enclosing level
        this->compilerWarning(statement->location(), "Symbol name hides previously declared symbol in enclosing level: '", name, "'");
      }
    }
  }
  return error;
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeIdentifier(const IEggProgramNode& self, const egg::ovum::String& name) {
  EggProgramExpression expression(*this, self);
  return std::make_unique<EggProgramAssigneeIdentifier>(*this, name);
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeBrackets(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const std::shared_ptr<IEggProgramNode>& index) {
  EggProgramExpression expression(*this, self);
  return std::make_unique<EggProgramAssigneeBrackets>(*this, instance, index);
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeDot(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const egg::ovum::String& property) {
  EggProgramExpression expression(*this, self);
  return std::make_unique<EggProgramAssigneeDot>(*this, instance, property);
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeDeref(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance) {
  EggProgramExpression expression(*this, self);
  return std::make_unique<EggProgramAssigneeDeref>(*this, instance);
}

void egg::yolk::EggProgramContext::statement(const IEggProgramNode& node) {
  // TODO use runtime location, not source location
  egg::ovum::LocationSource& source = this->location;
  source = node.location();
}

egg::ovum::LocationRuntime egg::yolk::EggProgramContext::swapLocation(const egg::ovum::LocationRuntime& loc) {
  egg::ovum::LocationRuntime before = this->location;
  this->location = loc;
  return before;
}

egg::ovum::Variant egg::yolk::EggProgramContext::get(const egg::ovum::String& name, bool byref) {
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return this->raiseFormat("Unknown identifier: '", name, "'");
  }
  auto& value = symbol->getValue();
  if (value.direct().isVoid()) {
    return this->raiseFormat("Uninitialized identifier: '", name.toUTF8(), "'");
  }
  if (byref) {
    value.indirect(this->getAllocator(), *this->softGetBasket());
  }
  return value;
}

egg::ovum::Variant egg::yolk::EggProgramContext::set(const egg::ovum::String& name, const egg::ovum::Variant& rvalue) {
  if (rvalue.hasFlowControl()) {
    return rvalue;
  }
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  return symbol->assign(*this, rvalue);
}

egg::ovum::Variant egg::yolk::EggProgramContext::guard(const egg::ovum::String& name, const egg::ovum::Variant& rvalue) {
  if (rvalue.hasFlowControl()) {
    return rvalue;
  }
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  auto retval = symbol->assign(*this, rvalue);
  if (retval.isVoid()) {
    // The assignment succeeded
    return egg::ovum::Variant::True;
  }
  return egg::ovum::Variant::False;
}

egg::ovum::Variant egg::yolk::EggProgramContext::assign(EggProgramAssign op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  auto dst = lhs.assignee(*this);
  if (dst == nullptr) {
    return this->raiseFormat("Left-hand side of assignment '", EggProgram::assignToString(op), "' operator is not a valid target");
  }
  egg::ovum::Variant right;
  if (op == EggProgramAssign::Equal) {
    // Simple assignment without interrogation beforehand
    right = rhs.execute(*this).direct();
  } else {
    // We need to interrogate the value of the lhs so we can modify it
    auto left = dst->get().direct();
    if (left.hasFlowControl()) {
      return left;
    }
    switch (op) {
    case EggProgramAssign::Remainder:
      right = this->arithmeticIntFloat(left, right, rhs, "remainder assignment '%='", remainderInt, remainderFloat);
      break;
    case EggProgramAssign::BitwiseAnd:
      right = this->arithmeticBoolInt(left, right, rhs, "and assignment '&='", bitwiseAndBool, bitwiseAndInt);
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
      right = this->arithmeticBoolInt(left, right, rhs, "xor assignment '^='", bitwiseXorBool, bitwiseXorInt);
      break;
    case EggProgramAssign::BitwiseOr:
      right = this->arithmeticBoolInt(left, right, rhs, "or assignment '|='", bitwiseOrBool, bitwiseOrInt);
      break;
    case EggProgramAssign::LogicalAnd:
      right = this->logicalBool(left, right, rhs, "logical-and assignment '&&='", EggProgramBinary::LogicalAnd);
      break;
    case EggProgramAssign::LogicalOr:
      right = this->logicalBool(left, right, rhs, "logical-or assignment '||='", EggProgramBinary::LogicalOr);
      break;
    case EggProgramAssign::NullCoalescing:
      right = this->coalesceNull(left, right, rhs);
      break;
    case EggProgramAssign::Equal:
    default:
      return this->raiseFormat("Internal runtime error: Unknown assignment operator: '", EggProgram::assignToString(op), "'");
    }
  }
  if (right.hasFlowControl()) {
    return right;
  }
  return dst->set(right);
}

egg::ovum::Variant egg::yolk::EggProgramContext::mutate(EggProgramMutate op, const IEggProgramNode& lvalue) {
  auto dst = lvalue.assignee(*this);
  if (dst == nullptr) {
    return this->raiseFormat("Operand of mutation '", EggProgram::mutateToString(op), "' operator is not a valid target");
  }
  auto lhs = dst->get().direct();
  if (lhs.hasFlowControl()) {
    return lhs;
  }
  egg::ovum::Variant rhs;
  switch (op) {
  case EggProgramMutate::Increment:
    if (!lhs.isInt()) {
      return this->unexpected("Expected operand of increment '++' operator to be 'int'", lhs);
    }
    rhs = plusInt(lhs.getInt(), 1);
    break;
  case EggProgramMutate::Decrement:
    if (!lhs.isInt()) {
      return this->unexpected("Expected operand of decrement '--' operator to be 'int'", lhs);
    }
    rhs = minusInt(lhs.getInt(), 1);
    break;
  default:
    return this->raiseFormat("Internal runtime error: Unknown mutation operator: '", EggProgram::mutateToString(op), "'");
  }
  if (rhs.hasFlowControl()) {
    return rhs;
  }
  return dst->set(rhs);
}

egg::ovum::Variant egg::yolk::EggProgramContext::condition(const IEggProgramNode& expression) {
  auto retval = expression.execute(*this).direct();
  if (retval.hasBool() || retval.hasFlowControl()) {
    return retval;
  }
  return this->raiseFormat("Expected condition to evaluate to a 'bool', but got '", retval.getRuntimeType().toString(), "' instead");
}

egg::ovum::Variant egg::yolk::EggProgramContext::unary(EggProgramUnary op, const IEggProgramNode& expr, egg::ovum::Variant& value) {
  switch (op) {
  case EggProgramUnary::LogicalNot:
    if (this->operand(value, expr, egg::ovum::VariantBits::Bool, "Expected operand of logical-not '!' operator to be 'bool'")) {
      return egg::ovum::Variant(!value.getBool());
    }
    return value;
  case EggProgramUnary::Negate:
    if (this->operand(value, expr, egg::ovum::VariantBits::Arithmetic, "Expected operand of negation '-' operator to be 'int' or 'float'")) {
      return value.isInt() ? egg::ovum::Variant(-value.getInt()) : egg::ovum::Variant(-value.getFloat());
    }
    return value;
  case EggProgramUnary::BitwiseNot:
    if (this->operand(value, expr, egg::ovum::VariantBits::Int, "Expected operand of bitwise-not '~' operator to be 'int'")) {
      return egg::ovum::Variant(~value.getInt());
    }
    return value;
  case EggProgramUnary::Ref:
    value = expr.execute(*this); // not .direct()
    if (value.hasFlowControl()) {
      return value;
    }
    return value.address();
  case EggProgramUnary::Deref:
    value = expr.execute(*this).direct();
    if (value.hasFlowControl()) {
      return value;
    }
    if (!value.hasPointer()) {
      return this->unexpected("Expected operand of dereference '*' operator to be a pointer", value);
    }
    return value.getPointee();
  case EggProgramUnary::Ellipsis:
    return this->raiseFormat("TODO unary(...) not fully implemented"); // TODO
  default:
    return this->raiseFormat("Internal runtime error: Unknown unary operator: '", EggProgram::unaryToString(op), "'");
  }
}

egg::ovum::Variant egg::yolk::EggProgramContext::binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs, egg::ovum::Variant& left, egg::ovum::Variant& right) {
  // OPTIMIZE
  left = lhs.execute(*this).direct();
  if (left.hasFlowControl()) {
    return left;
  }
  switch (op) {
  case EggProgramBinary::Unequal:
    if (left.hasAny(egg::ovum::VariantBits::AnyQ)) {
      if (!this->operand(right, rhs, egg::ovum::VariantBits::Any | egg::ovum::VariantBits::Null, "Expected right operand of inequality '!=' to be a value")) {
        return right;
      }
      return egg::ovum::Variant(left != right);
    }
    return this->unexpected("Expected left operand of inequality '!=' to be a value", left);
  case EggProgramBinary::Remainder:
    return this->arithmeticIntFloat(left, right, rhs, "remainder '%'", remainderInt, remainderFloat);
  case EggProgramBinary::BitwiseAnd:
    return this->arithmeticBoolInt(left, right, rhs, "and '&'", bitwiseAndBool, bitwiseAndInt);
  case EggProgramBinary::LogicalAnd:
    return this->logicalBool(left, right, rhs, "logical-and '&&'", EggProgramBinary::LogicalAnd);
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
    if (left.hasAny(egg::ovum::VariantBits::AnyQ)) {
      if (!this->operand(right, rhs, egg::ovum::VariantBits::AnyQ, "Expected right operand of equality '==' to be a value")) {
        return right;
      }
      return egg::ovum::Variant(left == right);
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
    return left.isNull() ? rhs.execute(*this).direct() : left;
  case EggProgramBinary::BitwiseXor:
    return this->arithmeticBoolInt(left, right, rhs, "xor '^'", bitwiseXorBool, bitwiseXorInt);
  case EggProgramBinary::BitwiseOr:
    return this->arithmeticBoolInt(left, right, rhs, "or '|'", bitwiseOrBool, bitwiseOrInt);
  case EggProgramBinary::LogicalOr:
    return this->logicalBool(left, right, rhs, "logical-or '||'", EggProgramBinary::LogicalOr);
  default:
    return this->raiseFormat("Internal runtime error: Unknown binary operator: '", EggProgram::binaryToString(op), "'");
  }
}

bool egg::yolk::EggProgramContext::operand(egg::ovum::Variant& dst, const IEggProgramNode& src, egg::ovum::VariantBits expected, const char* expectation) {
  dst = src.execute(*this).direct();
  if (dst.hasFlowControl()) {
    return false;
  }
  if (dst.hasAny(expected)) {
    return true;
  }
  dst = this->unexpected(expectation, dst);
  return false;
}

egg::ovum::Variant egg::yolk::EggProgramContext::coalesceNull(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs) {
  assert(!left.hasIndirect());
  if (!left.isNull()) {
    // Short-circuit
    right = egg::ovum::Variant::Void;
    return left;
  }
  right = rhs.execute(*this).direct();
  return right;
}

egg::ovum::Variant egg::yolk::EggProgramContext::logicalBool(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, EggProgramBinary binary) {
  assert(!left.hasIndirect());
  if (!left.isBool()) {
    return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'bool'", left);
  }
  if (left.getBool()) {
    if (binary == EggProgramBinary::LogicalOr) {
      // 'true || rhs' short-circuits to 'true'
      right = egg::ovum::Variant::Void;
      return egg::ovum::Variant::True;
    }
  } else {
    if (binary == EggProgramBinary::LogicalAnd) {
      // 'false && rhs' short-circuits to 'false'
      right = egg::ovum::Variant::Void;
      return egg::ovum::Variant::False;
    }
  }
  // The result is always 'rhs' now
  right = rhs.execute(*this).direct();
  assert(!right.hasIndirect());
  if (right.isBool()) {
    return right;
  }
  if (right.hasFlowControl()) {
    return right;
  }
  return this->unexpected("Expected right-hand side of " + std::string(operation) + " to be 'bool'", right);
}

egg::ovum::Variant egg::yolk::EggProgramContext::arithmeticBool(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools) {
  assert(!left.hasIndirect());
  if (!left.isBool()) {
    return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'bool'", left);
  }
  right = rhs.execute(*this).direct();
  assert(!right.hasIndirect());
  if (right.isBool()) {
    return bools(left.getBool(), right.getBool());
  }
  if (right.hasFlowControl()) {
    return right;
  }
  return this->unexpected("Expected right-hand side of " + std::string(operation) + " to be 'bool'", right);
}

egg::ovum::Variant egg::yolk::EggProgramContext::arithmeticInt(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints) {
  assert(!left.hasIndirect());
  if (!left.isInt()) {
    return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int'", left);
  }
  right = rhs.execute(*this).direct();
  assert(!right.hasIndirect());
  if (right.isInt()) {
    return ints(left.getInt(), right.getInt());
  }
  if (right.hasFlowControl()) {
    return right;
  }
  return this->unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int'", right);
}

egg::ovum::Variant egg::yolk::EggProgramContext::arithmeticBoolInt(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools, ArithmeticInt ints) {
  assert(!left.hasIndirect());
  if (left.isBool()) {
    return this->arithmeticBool(left, right, rhs, operation, bools);
  }
  if (left.isInt()) {
    return this->arithmeticInt(left, right, rhs, operation, ints);
  }
  if (right.hasFlowControl()) {
    return right;
  }
  return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'bool' or 'int'", left);
}

egg::ovum::Variant egg::yolk::EggProgramContext::arithmeticIntFloat(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints, ArithmeticFloat floats) {
  assert(!left.hasIndirect());
  if (!left.hasAny(egg::ovum::VariantBits::Arithmetic)) {
    return this->unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int' or 'float'", left);
  }
  right = rhs.execute(*this).direct();
  assert(!right.hasIndirect());
  if (right.isInt()) {
    if (left.isInt()) {
      return ints(left.getInt(), right.getInt());
    }
    return floats(left.getFloat(), static_cast<double>(right.getInt()));
  }
  if (right.isFloat()) {
    if (left.isInt()) {
      return floats(static_cast<double>(left.getInt()), right.getFloat());
    }
    return floats(left.getFloat(), right.getFloat());
  }
  if (right.hasFlowControl()) {
    return right;
  }
  return this->unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int' or 'float'", right);
}

egg::ovum::Variant egg::yolk::EggProgramContext::call(const egg::ovum::Variant& callee, const egg::ovum::IParameters& parameters) {
  auto& direct = callee.direct();
  if (!direct.hasObject()) {
    return this->unexpected("Expected function-like expression to be 'object'", direct);
  }
  auto object = direct.getObject();
  return object->call(*this, parameters);
}

egg::ovum::Variant egg::yolk::EggProgramContext::dotGet(const egg::ovum::Variant& instance, const egg::ovum::String& property) {
  // Dispatch requests for strings and complex types
  auto& direct = instance.direct();
  if (direct.hasObject()) {
    return direct.getObject()->getProperty(*this, property);
  }
  if (direct.hasString()) {
    return egg::yolk::Builtins::stringBuiltin(*this, direct.getString(), property);
  }
  return this->raiseFormat("Values of type '", instance.getRuntimeType().toString(), "' do not support properties such as '.", property, "'");
}

egg::ovum::Variant egg::yolk::EggProgramContext::dotSet(const egg::ovum::Variant& instance, const egg::ovum::String& property, const egg::ovum::Variant& value) {
  // Dispatch requests for complex types
  auto& direct = instance.direct();
  if (direct.hasObject()) {
    auto object = direct.getObject();
    return object->setProperty(*this, property, value);
  }
  if (direct.hasString()) {
    return this->raiseFormat("Strings do not support modification through properties such as '.", property, "'");
  }
  return this->raiseFormat("Values of type '", instance.getRuntimeType().toString(), "' do not support modification of properties such as '.", property, "'");
}

egg::ovum::Variant egg::yolk::EggProgramContext::bracketsGet(const egg::ovum::Variant& instance, const egg::ovum::Variant& index) {
  // Dispatch requests for strings and complex types
  auto& direct = instance.direct();
  if (direct.hasObject()) {
    auto object = direct.getObject();
    return object->getIndex(*this, index);
  }
  if (direct.hasString()) {
    // string operator[](int index)
    auto str = direct.getString();
    if (!index.isInt()) {
      return this->raiseFormat("String indexing '[]' only supports indices of type 'int', not '", index.getRuntimeType().toString(), "'");
    }
    auto i = index.getInt();
    auto c = str.codePointAt(size_t(i));
    if (c < 0) {
      // Invalid index
      auto n = str.length();
      if ((i < 0) || (size_t(i) >= n)) {
        return this->raiseFormat("String index ", i, " is out of range for a string of length ", n);
      }
      return this->raiseFormat("Cannot index a malformed string");
    }
    return egg::ovum::Variant{ egg::ovum::StringFactory::fromCodePoint(this->allocator, char32_t(c)) };
  }
  return this->raiseFormat("Values of type '", instance.getRuntimeType().toString(), "' do not support indexing with '[]'");
}

egg::ovum::Variant egg::yolk::EggProgramContext::bracketsSet(const egg::ovum::Variant& instance, const egg::ovum::Variant& index, const egg::ovum::Variant& value) {
  // Dispatch requests for complex types
  auto& direct = instance.direct();
  if (direct.hasObject()) {
    auto object = direct.getObject();
    return object->setIndex(*this, index, value);
  }
  if (direct.hasString()) {
    return this->raiseFormat("Strings do not support modification through indexing with '[]'");
  }
  return this->raiseFormat("Values of type '", instance.getRuntimeType().toString(), "' do not support indexing with '[]'");
}

egg::ovum::Variant egg::yolk::EggProgramContext::unexpected(const std::string& expectation, const egg::ovum::Variant& value) {
  return this->raiseFormat(expectation, ", but got '", value.getRuntimeType().toString(), "' instead");
}

egg::ovum::Variant egg::yolk::EggProgramContext::assertion(const egg::ovum::Variant& predicate) {
  auto& direct = predicate.direct();
  if (!direct.isBool()) {
    return this->unexpected("Expected assertion predicate to be 'bool'", direct);
  }
  if (!direct.getBool()) {
    return this->raiseFormat("Assertion is untrue");
  }
  return egg::ovum::Variant::Void;
}

void egg::yolk::EggProgramContext::print(const std::string& utf8) {
  return this->log(egg::ovum::ILogger::Source::User, egg::ovum::ILogger::Severity::Information, utf8);
}

egg::ovum::Variant egg::yolk::EggProgramContext::raise(const egg::ovum::String& message) {
  return EggProgramContext::createVanillaException(message);
}

egg::ovum::IAllocator& egg::yolk::EggProgramContext::getAllocator() const {
  return this->allocator;
}

egg::ovum::IBasket& egg::yolk::EggProgramContext::getBasket() const {
  auto* ptr = this->softGetBasket();
  assert(ptr != nullptr);
  return *ptr;
}

egg::ovum::Module egg::test::Compiler::compileFile(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, const std::string& path) {
  egg::yolk::FileTextStream stream(path);
  return compileModule(allocator, logger, stream);
}

egg::ovum::Module egg::test::Compiler::compileText(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, const std::string& source) {
  egg::yolk::StringTextStream stream(source);
  return compileModule(allocator, logger, stream);
}

egg::ovum::Variant egg::test::Compiler::run(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, const std::string& path) {
  auto module = egg::test::Compiler::compileFile(allocator, logger, path);
  if (module == nullptr) {
    return egg::ovum::Variant::Rethrow;
  }
  auto program = egg::ovum::ProgramFactory::createProgram(allocator, logger);
  auto result = program->run(*module);
  if (result.hasThrow()) {
    auto thrown = result;
    thrown.stripFlowControl(egg::ovum::VariantBits::Throw);
    if (!thrown.isVoid()) {
      // Don't log a rethrow
      logger.log(egg::ovum::ILogger::Source::User, egg::ovum::ILogger::Severity::Error, thrown.toString().toUTF8());
    }
  }
  return result;
}
