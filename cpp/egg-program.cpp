#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-program.h"
#include "egg-engine.h"

#include <set>

namespace {
  class EggProgramAssignee : public egg::yolk::IEggProgramAssignee {
  public:
    virtual egg::lang::Value get() const override {
      return egg::lang::Value::raise("TODO EggProgramAssignee get()"); // TODO
    }
    virtual egg::lang::Value set(const egg::lang::Value&) {
      return egg::lang::Value::raise("TODO EggProgramAssignee set()"); // TODO
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

#define EGG_PROGRAM_OPERATOR_STRING(name, text) text,

std::string egg::yolk::EggProgram::unaryToString(egg::yolk::EggProgramUnary op) {
  static const char* const table[] = {
    EGG_PROGRAM_UNARY_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::binaryToString(egg::yolk::EggProgramBinary op) {
  static const char* const table[] = {
    EGG_PROGRAM_BINARY_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::assignToString(egg::yolk::EggProgramAssign op) {
  static const char* const table[] = {
    EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

std::string egg::yolk::EggProgram::mutateToString(egg::yolk::EggProgramMutate op) {
  static const char* const table[] = {
    EGG_PROGRAM_MUTATE_OPERATORS(EGG_PROGRAM_OPERATOR_STRING)
  };
  auto index = static_cast<size_t>(op);
  assert(index < EGG_NELEMS(table));
  return table[index];
}

class egg::yolk::EggProgram::SymbolTable {
  EGG_NO_COPY(SymbolTable);
public:
  struct Symbol {
    std::string name;
    egg::lang::Value value;
    explicit Symbol(const std::string& name)
      : name(name), value(egg::lang::Value::Void) {
    }
  };
private:
  std::map<std::string, std::shared_ptr<Symbol>> map;
  SymbolTable* parent;
public:
  explicit SymbolTable(SymbolTable* parent = nullptr)
    : parent(parent) {
  }
  std::shared_ptr<Symbol> addSymbol(const std::string& name) {
    auto result = this->map.emplace(name, std::make_shared<Symbol>(name));
    assert(result.second);
    return result.first->second;
  }
  std::shared_ptr<Symbol> findSymbol(const std::string& name, bool includeParents = true) const {
    auto found = this->map.find(name);
    if (found != this->map.end()) {
      return found->second;
    }
    if (includeParents && (this->parent != nullptr)) {
      return this->parent->findSymbol(name);
    }
    return nullptr;
  }
};

void egg::yolk::EggProgramContext::log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) {
  if (severity > *this->maximumSeverity) {
    *this->maximumSeverity = severity;
  }
  this->execution->log(source, severity, message);
}

bool egg::yolk::EggProgramContext::findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Check for duplicate symbols
  bool error = false;
  std::string name;
  std::shared_ptr<IEggProgramType> type;
  std::set<std::string> seen;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      if (!seen.insert(name).second) {
        // Already seen at this level
        this->log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Duplicate symbol declared at module level: '" + name + "'");
        error = true;
      } else if (this->symtable->findSymbol(name) != nullptr) {
        // Seen at an enclosing level
        this->log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Warning, "Symbol name hides previously declared symbol in enclosing level: '" + name + "'");
      }
    }
  }
  return error;
}

egg::lang::Value egg::yolk::EggProgramContext::executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Execute all the statements one after another
  std::string name;
  std::shared_ptr<IEggProgramType> type;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      // We've checked for duplicate symbols already
      this->symtable->addSymbol(name);
    }
    statement->execute(*this);
  }
  this->execution->print("execute");
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  this->statement(self);
  if (this->findDuplicateSymbols(statements)) {
    return egg::lang::Value::raise("Execution halted due to previous errors");
  }
  return this->executeStatements(statements);
}

static egg::lang::Value WIBBLE(std::string function, ...) {
  if (!function.empty()) {
    EGG_THROW(function + " unimplemented: TODO");
  }
  return egg::lang::Value::Null;
}

egg::lang::Value egg::yolk::EggProgramContext::executeBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  this->statement(self);
  EggProgram::SymbolTable nested(this->symtable);
  EggProgramContext context(*this, nested);
  if (context.findDuplicateSymbols(statements)) {
    return egg::lang::Value::raise("Execution halted due to previous errors");
  }
  return context.executeStatements(statements);
}

egg::lang::Value egg::yolk::EggProgramContext::executeType(const IEggProgramNode& self, const IEggProgramType& type) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &type);
}

egg::lang::Value egg::yolk::EggProgramContext::executeDeclare(const IEggProgramNode& self, const std::string& name, const IEggProgramNode&, const IEggProgramNode* rvalue) {
  // The type information has already been used in the symbol declaration phase
  this->statement(self);
  if (rvalue != nullptr) {
    // The declaration contains an initial value
    return this->set(name, *rvalue);
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue) {
  this->statement(self);
  return this->assign(op, lvalue, rvalue);
}

egg::lang::Value egg::yolk::EggProgramContext::executeMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue) {
  this->statement(self);
  return this->mutate(op, lvalue);
}

egg::lang::Value egg::yolk::EggProgramContext::executeBreak(const IEggProgramNode& self) {
  this->statement(self);
  return egg::lang::Value::Break;
}

egg::lang::Value egg::yolk::EggProgramContext::executeCatch(const IEggProgramNode& self, const std::string&, const IEggProgramNode&, const IEggProgramNode& block) {
  // The symbol information has already been used to match this catch clause
  this->statement(self);
  return block.execute(*this);
}

egg::lang::Value egg::yolk::EggProgramContext::executeContinue(const IEggProgramNode& self) {
  this->statement(self);
  return egg::lang::Value::Continue;
}

egg::lang::Value egg::yolk::EggProgramContext::executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  egg::lang::Value retval;
  do {
    retval = block.execute(*this);
    if (retval.is(egg::lang::Discriminator::Break)) {
      // Just leave the loop
      return egg::lang::Value::Void;
    }
    if (!retval.is(egg::lang::Discriminator::Void | egg::lang::Discriminator::Continue)) {
      // Probably an exception
      return retval;
    }
    retval = this->condition(cond);
    if (!retval.is(egg::lang::Discriminator::Bool)) {
      // Condition evaluation failed
      return retval;
    }
  } while (retval.getBool());
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock) {
  this->statement(self);
  egg::lang::Value retval = this->condition(cond);
  if (!retval.is(egg::lang::Discriminator::Bool)) {
    return retval;
  }
  if (retval.getBool()) {
    return trueBlock.execute(*this);
  }
  if (falseBlock != nullptr) {
    return falseBlock ->execute(*this);
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block) {
  this->statement(self);
  egg::lang::Value retval;
  if (pre != nullptr) {
    retval = pre->execute(*this);
    if (!retval.is(egg::lang::Discriminator::Void)) {
      // Probably an exception in the pre-loop statement
      return retval;
    }
  }
  if (cond == nullptr) {
    // There's no explicit condition
    for (;;) {
      retval = block.execute(*this);
      if (retval.is(egg::lang::Discriminator::Break)) {
        // Just leave the loop
        return egg::lang::Value::Void;
      }
      if (!retval.is(egg::lang::Discriminator::Void | egg::lang::Discriminator::Continue)) {
        // Probably an exception in the condition expression
        return retval;
      }
      if (post != nullptr) {
        retval = post->execute(*this);
        if (!retval.is(egg::lang::Discriminator::Void)) {
          // Probably an exception in the post-loop statement
          return retval;
        }
      }
    }
  }
  retval = this->condition(*cond);
  while (retval.is(egg::lang::Discriminator::Bool)) {
    if (!retval.getBool()) {
      // The condition was false
      return egg::lang::Value::Void;
    }
    retval = block.execute(*this);
    if (retval.is(egg::lang::Discriminator::Break)) {
      // Just leave the loop
      return egg::lang::Value::Void;
    }
    if (!retval.is(egg::lang::Discriminator::Void | egg::lang::Discriminator::Continue)) {
      // Probably an exception in the condition expression
      return retval;
    }
    if (post != nullptr) {
      retval = post->execute(*this);
      if (!retval.is(egg::lang::Discriminator::Void)) {
        // Probably an exception in the post-loop statement
        return retval;
      }
    }
    retval = this->condition(*cond);
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &lvalue, &rvalue, &block);
}

egg::lang::Value egg::yolk::EggProgramContext::executeReturn(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &values);
}

egg::lang::Value egg::yolk::EggProgramContext::executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block) {
  if (this->switchExpression != nullptr) {
    // We're matching against values
    for (size_t i = 0; i < values.size(); ++i) {
      auto value = values[i]->execute(*this);
      if (value.is(egg::lang::Discriminator::FlowControl)) {
        return value;
      }
      if (value == *this->switchExpression) {
        // Found a match, so return the index of the block
        return egg::lang::Value(int64_t(i));
      }
    }
    // No match; the switch may have a 'default' clause however
    return egg::lang::Value::Null;
  }
  this->statement(self);
  return block.execute(*this);
}

egg::lang::Value egg::yolk::EggProgramContext::executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  this->statement(self);
  // This is a nasty, two-phase process:
  // Phase 1 evaluates the case values (switchExpression != nullptr)
  // Phase 2 executes the blocks as appropriate (switchExpression == nullptr)
  assert(this->switchExpression == nullptr);
  auto expr = value.execute(*this);
  if (expr.is(egg::lang::Discriminator::FlowControl)) {
    return expr;
  }
  egg::lang::Value matched;
  try {
    this->switchExpression = &expr;
    matched = this->executeSwitchPhase1(cases);
    this->switchExpression = nullptr;
  } catch (...) {
    this->switchExpression = nullptr;
    throw;
  }
  if (matched.is(egg::lang::Discriminator::Int)) {
    // An integer indicates the index of the case statement to actually execute
    return this->executeSwitchPhase2(matched.getInt(), cases);
  }
  return this->executeSwitchPhase2(defaultIndex, cases);
}

egg::lang::Value egg::yolk::EggProgramContext::executeSwitchPhase1(const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  // Phase 1 evaluates the case values (switchExpression != nullptr)
  for (size_t index = 0; index < cases.size(); ++index) {
    auto retval = cases[index]->execute(*this);
    if (retval.is(egg::lang::Discriminator::FlowControl)) {
      // Failed to evaluate a case label
      return retval;
    }
    if (retval.is(egg::lang::Discriminator::Bool) && retval.getBool()) {
      // This was a match
      return egg::lang::Value(int64_t(index));
    }
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeSwitchPhase2(int64_t index, const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  // Phase 2 executes the blocks as appropriate (switchExpression == nullptr)
  for (size_t i = size_t(index); i < cases.size(); ++i) {
    auto retval = cases[i]->execute(*this);
    if (retval.is(egg::lang::Discriminator::Break)) {
      // Explicit end of case clause
      break;
    }
    if (!retval.is(egg::lang::Discriminator::Continue)) {
      // Probably some other flow control such as a return or exception
      return retval;
    }
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &exception);
}

egg::lang::Value egg::yolk::EggProgramContext::executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &block, &catches, &final);
}

egg::lang::Value egg::yolk::EggProgramContext::executeUsing(const IEggProgramNode& self, const IEggProgramNode& value, const IEggProgramNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &value, &block);
}

egg::lang::Value egg::yolk::EggProgramContext::executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &cond, &block);
}

egg::lang::Value egg::yolk::EggProgramContext::executeYield(const IEggProgramNode& self, const IEggProgramNode& value) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &value);
}

egg::lang::Value egg::yolk::EggProgramContext::executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &callee, &parameters);
}

egg::lang::Value egg::yolk::EggProgramContext::executeIdentifier(const IEggProgramNode& self, const std::string& name) {
  this->expression(self);
  return this->get(name);
}

egg::lang::Value egg::yolk::EggProgramContext::executeLiteral(const IEggProgramNode& self, const egg::lang::Value& value) {
  this->expression(self);
  return value;
}

egg::lang::Value egg::yolk::EggProgramContext::executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& value) {
  this->expression(self);
  return this->unary(op, value);
}

egg::lang::Value egg::yolk::EggProgramContext::executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  this->expression(self);
  return this->binary(op, lhs, rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse) {
  this->expression(self);
  auto retval = this->condition(cond);
  if (retval.is(egg::lang::Discriminator::Bool)) {
    return retval.getBool() ? whenTrue.execute(*this) : whenFalse.execute(*this);
  }
  return retval;
}

egg::lang::LogSeverity egg::yolk::EggProgram::execute(IEggEngineExecutionContext& execution) {
  EggProgram::SymbolTable symtable(nullptr);
  symtable.addSymbol("print")->value = egg::lang::Value("TODO print");
  // TODO add built-in symbol to symbol table here
  return this->execute(execution, symtable);
}

egg::lang::LogSeverity egg::yolk::EggProgram::execute(IEggEngineExecutionContext& execution, EggProgram::SymbolTable& symtable) {
  egg::lang::LogSeverity severity = egg::lang::LogSeverity::None;
  EggProgramContext context(execution, symtable, severity);
  this->root->execute(context);
  return severity;
}

void egg::yolk::EggProgramContext::statement(const IEggProgramNode&) {
  // TODO
}

void egg::yolk::EggProgramContext::expression(const IEggProgramNode&) {
  // TODO
}

egg::lang::Value egg::yolk::EggProgramContext::get(const std::string& name) {
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return egg::lang::Value::raise("Unknown identifier: '" + name + "'");
  }
  if (symbol->value.is(egg::lang::Discriminator::Void)) {
    return egg::lang::Value::raise("Uninitialized identifier: '" + name + "'");
  }
  return symbol->value;
}

egg::lang::Value egg::yolk::EggProgramContext::set(const std::string& name, const IEggProgramNode& rvalue) {
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  symbol->value = rvalue.execute(*this);
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::assign(EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue) {
  auto dst = this->assignee(lvalue);
  auto lhs = dst->get();
  if (lhs.is(egg::lang::Discriminator::FlowControl)) {
    return lhs;
  }
  egg::lang::Value rhs;
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
  case EggProgramAssign::Equal:
    rhs = rvalue.execute(*this);
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
  default:
    return egg::lang::Value::raise("Internal runtime error: Unknown assignment operator: '" + EggProgram::assignToString(op) + "'");
  }
  if (rhs.is(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return dst->set(rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::mutate(EggProgramMutate op, const IEggProgramNode& lvalue) {
  auto dst = this->assignee(lvalue);
  auto lhs = dst->get();
  if (lhs.is(egg::lang::Discriminator::FlowControl)) {
    return lhs;
  }
  egg::lang::Value rhs;
  switch (op) {
  case EggProgramMutate::Increment:
    if (!lhs.is(egg::lang::Discriminator::Int)) {
      return EggProgramContext::unexpected("Expected operand of increment operator '++' to be 'int'", lhs);
    }
    rhs = plusInt(lhs.getInt(), 1);
    break;
  case EggProgramMutate::Decrement:
    if (!lhs.is(egg::lang::Discriminator::Int)) {
      return EggProgramContext::unexpected("Expected operand of decrement operator '--' to be 'int'", lhs);
    }
    rhs = minusInt(lhs.getInt(), 1);
    break;
  default:
    return egg::lang::Value::raise("Internal runtime error: Unknown mutation operator: '" + EggProgram::mutateToString(op) + "'");
  }
  if (rhs.is(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return dst->set(rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::condition(const IEggProgramNode& expression) {
  auto retval = expression.execute(*this);
  if (retval.is(egg::lang::Discriminator::Bool | egg::lang::Discriminator::FlowControl)) {
    return retval;
  }
  return egg::lang::Value::raise("Expected condition to evaluate to a 'bool', but got '" + retval.getTagString() + "' instead");
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
    return egg::lang::Value::raise("TODO " __FUNCTION__ " not fully implemented"); // TODO
  default:
    return egg::lang::Value::raise("Internal runtime error: Unknown unary operator: '" + EggProgram::unaryToString(op) + "'");
  }
}

egg::lang::Value egg::yolk::EggProgramContext::binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
  egg::lang::Value left = lhs.execute(*this);
  if (left.is(egg::lang::Discriminator::FlowControl)) {
    return left;
  }
  switch (op) {
  case EggProgramBinary::Unequal:
    if (left.is(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null)) {
      egg::lang::Value right;
      if (!this->operand(right, rhs, egg::lang::Discriminator::Any | egg::lang::Discriminator::Null, "Expected right operand of inequality '!=' to be a value")) {
        return right;
      }
      return egg::lang::Value(left != right);
    }
    return EggProgramContext::unexpected("Expected left operand of inequality '!=' to be a value", left);
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
    return EggProgramContext::unexpected("Expected left operand of logical-and '&&' to be 'bool'", left);
  case EggProgramBinary::Multiply:
    return this->arithmeticIntFloat(left, rhs, "multiplication '*'", multiplyInt, multiplyFloat);
  case EggProgramBinary::Plus:
    return this->arithmeticIntFloat(left, rhs, "addition '+'", plusInt, plusFloat);
  case EggProgramBinary::Minus:
    return this->arithmeticIntFloat(left, rhs, "subtraction '-'", minusInt, minusFloat);
  case EggProgramBinary::Lambda:
    return egg::lang::Value::raise("TODO " __FUNCTION__ " not fully implemented"); // TODO
  case EggProgramBinary::Dot:
    return egg::lang::Value::raise("TODO " __FUNCTION__ " not fully implemented"); // TODO
  case EggProgramBinary::Divide:
    return this->arithmeticIntFloat(left, rhs, "division '/'", divideInt, divideFloat);
  case EggProgramBinary::Less:
    return this->arithmeticIntFloat(left, rhs, "comparison '<'", lessInt, lessFloat);
  case EggProgramBinary::ShiftLeft:
    return this->arithmeticInt(left, rhs, "shift-left '<<'", shiftLeftInt);
  case EggProgramBinary::LessEqual:
    return this->arithmeticIntFloat(left, rhs, "comparison '<='", lessEqualInt, lessEqualFloat);
  case EggProgramBinary::Equal:
    if (left.is(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null)) {
      egg::lang::Value right;
      if (!this->operand(right, rhs, egg::lang::Discriminator::Any | egg::lang::Discriminator::Null, "Expected right operand of equality '==' to be a value")) {
        return right;
      }
      return egg::lang::Value(left == right);
    }
    return EggProgramContext::unexpected("Expected left operand of equality '==' to be a value", left);
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
    return egg::lang::Value::raise("TODO " __FUNCTION__ " not fully implemented"); // TODO
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
    return EggProgramContext::unexpected("Expected left operand of logical-or '||' to be 'bool'", left);
  default:
    return egg::lang::Value::raise("Internal runtime error: Unknown binary operator: '" + EggProgram::binaryToString(op) + "'");
  }
}

bool egg::yolk::EggProgramContext::operand(egg::lang::Value& dst, const IEggProgramNode& src, egg::lang::Discriminator expected, const char* expectation) {
  dst = src.execute(*this);
  if (dst.is(expected)) {
    return true;
  }
  if (!dst.is(egg::lang::Discriminator::FlowControl)) {
    dst = EggProgramContext::unexpected(expectation, dst);
  }
  return false;
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assignee(const IEggProgramNode&) {
  return std::make_unique<EggProgramAssignee>();
}

egg::lang::Value egg::yolk::EggProgramContext::arithmeticIntFloat(const egg::lang::Value& lhs, const IEggProgramNode& rvalue, const char* operation, ArithmeticInt ints, ArithmeticFloat floats) {
  if (!lhs.is(egg::lang::Discriminator::Arithmetic)) {
    return EggProgramContext::unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int' or 'float'", lhs);
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
  if (rhs.is(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return EggProgramContext::unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int' or 'float'", rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::arithmeticInt(const egg::lang::Value& lhs, const IEggProgramNode& rvalue, const char* operation, ArithmeticInt ints) {
  if (!lhs.is(egg::lang::Discriminator::Int)) {
    return EggProgramContext::unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int'", lhs);
  }
  egg::lang::Value rhs = rvalue.execute(*this);
  if (rhs.is(egg::lang::Discriminator::Int)) {
    return ints(lhs.getInt(), rhs.getInt());
  }
  if (rhs.is(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return EggProgramContext::unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int'", rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::unexpected(const std::string& expectation, const egg::lang::Value& value) {
  return egg::lang::Value::raise(expectation + ", but got '" + value.getTagString() + "' instead");
}
