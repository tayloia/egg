#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-program.h"
#include "egg-engine.h"

#include <cmath>
#include <map>
#include <set>

namespace {
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

  class EggProgramFunction : public egg::gc::HardReferenceCounted<egg::lang::IObject>{
    EGG_NO_COPY(EggProgramFunction);
  private:
    egg::yolk::EggProgramContext& program;
    egg::lang::ITypeRef type;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    EggProgramFunction(egg::yolk::EggProgramContext& program, const egg::lang::IType& type, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : program(program), type(&type), block(block) {
      assert(block != nullptr);
    }
    virtual bool dispose() override {
      return false;
    }
    virtual egg::lang::Value toString() const override {
      return egg::lang::Value(egg::lang::String::concat("[", type->toString(), "]"));
    }
    virtual egg::lang::Value getRuntimeType() const override {
      return egg::lang::Value(*this->type);
    }
    virtual egg::lang::Value call(egg::lang::IExecution&, const egg::lang::IParameters& parameters) override {
      return this->program.executeFunctionCall(*this->type, parameters, *this->block);
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
    egg::lang::String name;
    egg::lang::ITypeRef type;
    egg::lang::Value value;
    Symbol(const egg::lang::String& name, const egg::lang::IType& type)
      : name(name), type(&type), value(egg::lang::Value::Void) {
    }
    void assign(const egg::lang::Value& rhs) {
      // Ask the type to assign the value so that type promotion can occur
      egg::lang::String problem;
      auto promoted = this->type->promoteAssignment(rhs);
      if (promoted.stripFlowControl(egg::lang::Discriminator::Exception)) {
        EGG_THROW(promoted.getString().toUTF8()); // TODO don't throw here!
      }
      this->value = promoted;
    }
  };
private:
  std::map<egg::lang::String, std::shared_ptr<Symbol>> map;
  SymbolTable* parent;
public:
  explicit SymbolTable(SymbolTable* parent = nullptr)
    : parent(parent) {
  }
  void addBuiltin(const std::string& name, const egg::lang::Value& value) {
    this->addSymbol(egg::lang::String::fromUTF8(name), value.getRuntimeType())->value = value;
  }
  std::shared_ptr<Symbol> addSymbol(const egg::lang::String& name, const egg::lang::IType& type) {
    auto result = this->map.emplace(name, std::make_shared<Symbol>(name, type));
    assert(result.second);
    return result.first->second;
  }
  std::shared_ptr<Symbol> findSymbol(const egg::lang::String& name, bool includeParents = true) const {
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
  this->engine->log(source, severity, message);
}

bool egg::yolk::EggProgramContext::findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Check for duplicate symbols
  bool error = false;
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  std::set<egg::lang::String> seen;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      if (!seen.insert(name).second) {
        // Already seen at this level
        this->log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Duplicate symbol declared at module level: '" + name.toUTF8() + "'");
        error = true;
      } else if (this->symtable->findSymbol(name) != nullptr) {
        // Seen at an enclosing level
        this->log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Warning, "Symbol name hides previously declared symbol in enclosing level: '" + name.toUTF8() + "'");
      }
    }
  }
  return error;
}

egg::lang::Value egg::yolk::EggProgramContext::executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  // Execute all the statements one after another
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& statement : statements) {
    if (statement->symbol(name, type)) {
      // We've checked for duplicate symbols already
      this->symtable->addSymbol(name, *type);
    }
    auto retval = statement->execute(*this);
    if (!retval.has(egg::lang::Discriminator::Void)) {
      return retval;
    }
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements) {
  this->statement(self);
  if (this->findDuplicateSymbols(statements)) {
    return egg::lang::Value::raise("Execution halted due to previous errors");
  }
  return this->executeStatements(statements);
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

egg::lang::Value egg::yolk::EggProgramContext::executeDeclare(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::IType&, const IEggProgramNode* rvalue) {
  // The type information has already been used in the symbol declaration phase
  this->statement(self);
  if (rvalue != nullptr) {
    // The declaration contains an initial value
    return this->set(name, rvalue->execute(*this));
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

egg::lang::Value egg::yolk::EggProgramContext::executeContinue(const IEggProgramNode& self) {
  this->statement(self);
  return egg::lang::Value::Continue;
}

egg::lang::Value egg::yolk::EggProgramContext::executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  egg::lang::Value retval;
  do {
    retval = block.execute(*this);
    if (retval.has(egg::lang::Discriminator::Break)) {
      // Just leave the loop
      return egg::lang::Value::Void;
    }
    if (!retval.has(egg::lang::Discriminator::Void | egg::lang::Discriminator::Continue)) {
      // Probably an exception
      return retval;
    }
    retval = this->condition(cond);
    if (!retval.has(egg::lang::Discriminator::Bool)) {
      // Condition evaluation failed
      return retval;
    }
  } while (retval.getBool());
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock) {
  this->statement(self);
  egg::lang::Value retval = this->condition(cond);
  if (!retval.has(egg::lang::Discriminator::Bool)) {
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
    if (!retval.has(egg::lang::Discriminator::Void)) {
      // Probably an exception in the pre-loop statement
      return retval;
    }
  }
  if (cond == nullptr) {
    // There's no explicit condition
    for (;;) {
      retval = block.execute(*this);
      if (retval.has(egg::lang::Discriminator::Break)) {
        // Just leave the loop
        return egg::lang::Value::Void;
      }
      if (!retval.has(egg::lang::Discriminator::Void | egg::lang::Discriminator::Continue)) {
        // Probably an exception in the condition expression
        return retval;
      }
      if (post != nullptr) {
        retval = post->execute(*this);
        if (!retval.has(egg::lang::Discriminator::Void)) {
          // Probably an exception in the post-loop statement
          return retval;
        }
      }
    }
  }
  retval = this->condition(*cond);
  while (retval.has(egg::lang::Discriminator::Bool)) {
    if (!retval.getBool()) {
      // The condition was false
      return egg::lang::Value::Void;
    }
    retval = block.execute(*this);
    if (retval.has(egg::lang::Discriminator::Break)) {
      // Just leave the loop
      return egg::lang::Value::Void;
    }
    if (!retval.has(egg::lang::Discriminator::Void | egg::lang::Discriminator::Continue)) {
      // Probably an exception in the condition expression
      return retval;
    }
    if (post != nullptr) {
      retval = post->execute(*this);
      if (!retval.has(egg::lang::Discriminator::Void)) {
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
  auto dst = lvalue.assignee(*this);
  if (dst == nullptr) {
    return egg::lang::Value::raise("Iteration target in 'for' statement is not valid");
  }
  auto src = rvalue.execute(*this);
  if (src.has(egg::lang::Discriminator::FlowControl)) {
    return src;
  }
  auto retval = dst->set(src);
  while (!retval.has(egg::lang::Discriminator::FlowControl | egg::lang::Discriminator::Void)) {
    retval = block.execute(*this);
    if (retval.has(egg::lang::Discriminator::Break)) {
      // Just leave the loop
      return egg::lang::Value::Void;
    }
    if (!retval.has(egg::lang::Discriminator::Void | egg::lang::Discriminator::Continue)) {
      // Probably an exception in the condition expression
      return retval;
    }
    retval = dst->set(src);
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeFunctionDefinition(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::IType& type, const std::shared_ptr<IEggProgramNode>& block) {
  // This defines a function, it doesn't call it
  this->statement(self);
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  // We can store this directly in the symbol table without going through the type system
  // otherwise we get issues with function assignment
  assert(symbol->value.has(egg::lang::Discriminator::Void));
  symbol->value = egg::lang::Value(*new EggProgramFunction(*this, type, block));
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeFunctionCall(const egg::lang::IType& type, const egg::lang::IParameters& parameters, const IEggProgramNode& block) {
  // This actually calls a function
  EggProgram::SymbolTable nested(this->symtable);
  egg::lang::IType::Setter setter = [&nested](const egg::lang::String& k, const egg::lang::IType& t, const egg::lang::Value& v) {
    nested.addSymbol(k, t)->assign(v);
  };
  auto retval = type.decantParameters(parameters, setter);
  if (!retval.has(egg::lang::Discriminator::FlowControl)) {
    EggProgramContext context(*this, nested);
    retval = block.execute(context);
    if (retval.stripFlowControl(egg::lang::Discriminator::Return)) {
      return retval;
    }
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeReturn(const IEggProgramNode& self, const IEggProgramNode* value) {
  this->statement(self);
  if (value == nullptr) {
    // This is a void return
    return egg::lang::Value::ReturnVoid;
  }
  auto result = value->execute(*this);
  if (!result.has(egg::lang::Discriminator::FlowControl)) {
    // Need to convert the result to a return flow control
    result.addFlowControl(egg::lang::Discriminator::Return);
  }
  return result;
}

egg::lang::Value egg::yolk::EggProgramContext::executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases) {
  this->statement(self);
  // This is a two-phase process:
  // Phase 1 evaluates the case values
  // Phase 2 executes the block(s) as appropriate
  auto expr = value.execute(*this);
  if (expr.has(egg::lang::Discriminator::FlowControl)) {
    return expr;
  }
  auto matched = size_t(defaultIndex);
  for (size_t index = 0; index < cases.size(); ++index) {
    auto retval = cases[index]->executeWithExpression(*this, expr);
    if (!retval.has(egg::lang::Discriminator::Bool)) {
      // Failed to evaluate a case label
      return retval;
    }
    if (retval.getBool()) {
      // This was a match
      matched = index;
      break;
    }
  }
  while (matched < cases.size()) {
    auto retval = cases[matched]->execute(*this);
    if (retval.has(egg::lang::Discriminator::Break)) {
      // Explicit end of case clause
      break;
    }
    if (!retval.has(egg::lang::Discriminator::Continue)) {
      // Probably some other flow control such as a return or exception
      return retval;
    }
    matched++;
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block, const egg::lang::Value* against) {
  if (against != nullptr) {
    // We're matching against values
    for (auto& i : values) {
      auto value = i->execute(*this);
      if (value.has(egg::lang::Discriminator::FlowControl)) {
        return value;
      }
      if (value == *against) {
        // Found a match, so return 'true'
        return egg::lang::Value::True;
      }
    }
    // No match; the switch may have a 'default' clause however
    return egg::lang::Value::False;
  }
  this->statement(self);
  return block.execute(*this);
}

egg::lang::Value egg::yolk::EggProgramContext::executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception) {
  this->statement(self);
  if (exception == nullptr) {
    // This is a rethrow
    return egg::lang::Value::Rethrow;
  }
  auto value = exception->execute(*this);
  if (value.has(egg::lang::Discriminator::FlowControl)) {
    return value;
  }
  if (!value.has(egg::lang::Discriminator::Any)) {
    return egg::lang::Value::raise("Cannot 'throw' a value of type '", value.getTagString(), "'");
  }
  return egg::lang::Value::raise(value.getString());
}

egg::lang::Value egg::yolk::EggProgramContext::executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final) {
  this->statement(self);
  auto retval = block.execute(*this);
  if (retval.has(egg::lang::Discriminator::Exception)) {
    // An exception has indeed been thrown
    for (auto& i : catches) {
      auto match = i->executeWithExpression(*this, retval);
      if (!match.has(egg::lang::Discriminator::Bool)) {
        // Failed to evaluate the catch condition
        return this->executeFinally(match, final);
      }
      if (match.getBool()) {
        // This catch clause has been successfully executed
        return this->executeFinally(egg::lang::Value::Void, final);
      }
    }
  }
  return this->executeFinally(retval, final);
}

egg::lang::Value egg::yolk::EggProgramContext::executeCatch(const IEggProgramNode& self, const egg::lang::String&, const IEggProgramNode&, const IEggProgramNode& block, const egg::lang::Value& exception) {
  this->statement(self);
  // TODO return false if typeof(exception) != type
  auto retval = block.execute(*this);
  if (retval.has(egg::lang::Discriminator::FlowControl)) {
    // Check for a rethrow
    if (retval.has(egg::lang::Discriminator::Exception | egg::lang::Discriminator::Void)) {
      return exception;
    }
    return retval;
  }
  if (retval.has(egg::lang::Discriminator::Void)) {
    // Return 'true' to indicate to the 'try' statement that we ran this 'catch' block
    return egg::lang::Value::True;
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeFinally(const egg::lang::Value& retval, const IEggProgramNode* final) {
  if (final != nullptr) {
    auto secondary = final->execute(*this);
    if (!secondary.has(egg::lang::Discriminator::Void)) {
      return secondary;
    }
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeUsing(const IEggProgramNode& self, const IEggProgramNode& value, const IEggProgramNode& block) {
  this->statement(self);
  auto expr = value.execute(*this);
  if (expr.has(egg::lang::Discriminator::FlowControl)) {
    return expr;
  }
  if (!expr.has(egg::lang::Discriminator::Null | egg::lang::Discriminator::Object)) {
    return this->unexpected("Expected expression in 'using' statement to be 'null' or an object", expr);
  }
  auto retval = block.execute(*this);
  if (expr.has(egg::lang::Discriminator::Object)) {
    auto& object = expr.getObject();
    if (!object.dispose()) {
      return egg::lang::Value::raise("Failed to 'dispose' object instance at end of 'using' statement");
    }
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block) {
  this->statement(self);
  auto retval = this->condition(cond);
  while (retval.has(egg::lang::Discriminator::Bool)) {
    retval = block.execute(*this);
    if (retval.has(egg::lang::Discriminator::Break)) {
      // Just leave the loop
      return egg::lang::Value::Void;
    }
    if (!retval.has(egg::lang::Discriminator::Void | egg::lang::Discriminator::Continue)) {
      // Probably an exception
      break;
    }
    retval = this->condition(cond);
  }
  return retval;
}

egg::lang::Value egg::yolk::EggProgramContext::executeYield(const IEggProgramNode& self, const IEggProgramNode& value) {
  this->statement(self);
  auto result = value.execute(*this);
  if (!result.has(egg::lang::Discriminator::FlowControl)) {
    // Need to convert the result to a return flow control
    result.addFlowControl(egg::lang::Discriminator::Yield);
  }
  return result;
}

egg::lang::Value egg::yolk::EggProgramContext::executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters) {
  this->expression(self);
  auto func = callee.execute(*this);
  if (func.has(egg::lang::Discriminator::FlowControl)) {
    return func;
  }
  EggProgramParameters params(parameters.size());
  egg::lang::String name;
  auto type = egg::lang::Type::Void;
  for (auto& parameter : parameters) {
    auto value = parameter->execute(*this);
    if (value.has(egg::lang::Discriminator::FlowControl)) {
      return value;
    }
    if (parameter->symbol(name, type)) {
      params.addNamed(name, value);
    } else {
      params.addPositional(value);
    }
  }
  return this->call(func, params);
}

egg::lang::Value egg::yolk::EggProgramContext::executeIdentifier(const IEggProgramNode& self, const egg::lang::String& name) {
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
  if (retval.has(egg::lang::Discriminator::Bool)) {
    return retval.getBool() ? whenTrue.execute(*this) : whenFalse.execute(*this);
  }
  return retval;
}

std::unique_ptr<egg::yolk::IEggProgramAssignee> egg::yolk::EggProgramContext::assigneeIdentifier(const IEggProgramNode& self, const egg::lang::String& name) {
  this->expression(self);
  return std::make_unique<EggProgramAssigneeIdentifier>(*this, name);
}

egg::lang::LogSeverity egg::yolk::EggProgram::execute(IEggEngineExecutionContext& execution) {
  EggProgram::SymbolTable symtable(nullptr);
  symtable.addBuiltin("print", egg::lang::Value::Print);
  // TODO add built-in symbol to symbol table here
  return this->execute(execution, symtable);
}

egg::lang::LogSeverity egg::yolk::EggProgram::execute(IEggEngineExecutionContext& execution, EggProgram::SymbolTable& symtable) {
  egg::lang::LogSeverity severity = egg::lang::LogSeverity::None;
  EggProgramContext context(execution, symtable, severity);
  auto retval = this->root->execute(context);
  if (!retval.has(egg::lang::Discriminator::Void)) {
    if (retval.stripFlowControl(egg::lang::Discriminator::Exception)) {
      // TODO exception location
      context.log(egg::lang::LogSource::Runtime, egg::lang::LogSeverity::Error, retval.getString().toUTF8());
    } else {
      context.log(egg::lang::LogSource::Runtime, egg::lang::LogSeverity::Error, "Expected statement to return 'void', but got '" + retval.getTagString() + "' instead");
    }
  }
  return severity;
}

void egg::yolk::EggProgramContext::statement(const IEggProgramNode&) {
  // TODO
}

void egg::yolk::EggProgramContext::expression(const IEggProgramNode&) {
  // TODO
}

egg::lang::Value egg::yolk::EggProgramContext::get(const egg::lang::String& name) {
  auto symbol = this->symtable->findSymbol(name);
  if (symbol == nullptr) {
    return egg::lang::Value::raise("Unknown identifier: '", name, "'");
  }
  if (symbol->value.has(egg::lang::Discriminator::Void)) {
    return egg::lang::Value::raise("Uninitialized identifier: '", name.toUTF8(), "'");
  }
  return symbol->value;
}

egg::lang::Value egg::yolk::EggProgramContext::set(const egg::lang::String& name, const egg::lang::Value& rvalue) {
  if (rvalue.has(egg::lang::Discriminator::FlowControl)) {
    return rvalue;
  }
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  symbol->assign(rvalue);
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggProgramContext::assign(EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue) {
  auto dst = lvalue.assignee(*this);
  if (dst == nullptr) {
    return egg::lang::Value::raise("Left-hand side of assignment operator '", EggProgram::assignToString(op), "' is not a valid target");
  }
  auto lhs = dst->get();
  if (lhs.has(egg::lang::Discriminator::FlowControl)) {
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
    return egg::lang::Value::raise("Internal runtime error: Unknown assignment operator: '", EggProgram::assignToString(op), "'");
  }
  if (rhs.has(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return dst->set(rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::mutate(EggProgramMutate op, const IEggProgramNode& lvalue) {
  auto dst = lvalue.assignee(*this);
  if (dst == nullptr) {
    return egg::lang::Value::raise("Operand of mutation operator '", EggProgram::mutateToString(op), "' is not a valid target");
  }
  auto lhs = dst->get();
  if (lhs.has(egg::lang::Discriminator::FlowControl)) {
    return lhs;
  }
  egg::lang::Value rhs;
  switch (op) {
  case EggProgramMutate::Increment:
    if (!lhs.has(egg::lang::Discriminator::Int)) {
      return EggProgramContext::unexpected("Expected operand of increment operator '++' to be 'int'", lhs);
    }
    rhs = plusInt(lhs.getInt(), 1);
    break;
  case EggProgramMutate::Decrement:
    if (!lhs.has(egg::lang::Discriminator::Int)) {
      return EggProgramContext::unexpected("Expected operand of decrement operator '--' to be 'int'", lhs);
    }
    rhs = minusInt(lhs.getInt(), 1);
    break;
  default:
    return egg::lang::Value::raise("Internal runtime error: Unknown mutation operator: '", EggProgram::mutateToString(op), "'");
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
  return egg::lang::Value::raise("Expected condition to evaluate to a 'bool', but got '", retval.getTagString(), "' instead");
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
      return result.has(egg::lang::Discriminator::Int) ? egg::lang::Value(-result.getInt()) : egg::lang::Value(-result.getFloat());
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
    return egg::lang::Value::raise("TODO unary() not fully implemented"); // TODO
  default:
    return egg::lang::Value::raise("Internal runtime error: Unknown unary operator: '", EggProgram::unaryToString(op), "'");
  }
}

egg::lang::Value egg::yolk::EggProgramContext::binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs) {
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
    return EggProgramContext::unexpected("Expected left operand of inequality '!=' to be a value", left);
  case EggProgramBinary::Remainder:
    return this->arithmeticIntFloat(left, rhs, "remainder '%'", remainderInt, remainderFloat);
  case EggProgramBinary::BitwiseAnd:
    return this->arithmeticInt(left, rhs, "bitwise-and '&'", bitwiseAndInt);
  case EggProgramBinary::LogicalAnd:
    if (left.has(egg::lang::Discriminator::Bool)) {
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
    return egg::lang::Value::raise("TODO binary(Lambda) not fully implemented"); // TODO
  case EggProgramBinary::Dot:
    return egg::lang::Value::raise("TODO binary(Dot) not fully implemented"); // TODO
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
    return left.has(egg::lang::Discriminator::Null) ? rhs.execute(*this) : left;
  case EggProgramBinary::Brackets:
    return egg::lang::Value::raise("TODO binary(Brackets) not fully implemented"); // TODO
  case EggProgramBinary::BitwiseXor:
    return this->arithmeticInt(left, rhs, "bitwise-xor '^'", bitwiseXorInt);
  case EggProgramBinary::BitwiseOr:
    return this->arithmeticInt(left, rhs, "bitwise-or '|'", bitwiseOrInt);
  case EggProgramBinary::LogicalOr:
    if (left.has(egg::lang::Discriminator::Bool)) {
      if (left.getBool()) {
        return left;
      }
      egg::lang::Value right;
      (void)this->operand(right, rhs, egg::lang::Discriminator::Bool, "Expected right operand of logical-or '||' to be 'bool'");
      return right;
    }
    return EggProgramContext::unexpected("Expected left operand of logical-or '||' to be 'bool'", left);
  default:
    return egg::lang::Value::raise("Internal runtime error: Unknown binary operator: '", EggProgram::binaryToString(op), "'");
  }
}

bool egg::yolk::EggProgramContext::operand(egg::lang::Value& dst, const IEggProgramNode& src, egg::lang::Discriminator expected, const char* expectation) {
  dst = src.execute(*this);
  if (dst.has(expected)) {
    return true;
  }
  if (!dst.has(egg::lang::Discriminator::FlowControl)) {
    dst = EggProgramContext::unexpected(expectation, dst);
  }
  return false;
}

egg::lang::Value egg::yolk::EggProgramContext::arithmeticIntFloat(const egg::lang::Value& lhs, const IEggProgramNode& rvalue, const char* operation, ArithmeticInt ints, ArithmeticFloat floats) {
  if (!lhs.has(egg::lang::Discriminator::Arithmetic)) {
    return EggProgramContext::unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int' or 'float'", lhs);
  }
  egg::lang::Value rhs = rvalue.execute(*this);
  if (rhs.has(egg::lang::Discriminator::Int)) {
    if (lhs.has(egg::lang::Discriminator::Int)) {
      return ints(lhs.getInt(), rhs.getInt());
    }
    return floats(lhs.getFloat(), static_cast<double>(rhs.getInt()));
  }
  if (rhs.has(egg::lang::Discriminator::Float)) {
    if (lhs.has(egg::lang::Discriminator::Int)) {
      return floats(static_cast<double>(lhs.getInt()), rhs.getFloat());
    }
    return floats(lhs.getFloat(), rhs.getFloat());
  }
  if (rhs.has(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return EggProgramContext::unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int' or 'float'", rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::arithmeticInt(const egg::lang::Value& lhs, const IEggProgramNode& rvalue, const char* operation, ArithmeticInt ints) {
  if (!lhs.has(egg::lang::Discriminator::Int)) {
    return EggProgramContext::unexpected("Expected left-hand side of " + std::string(operation) + " to be 'int'", lhs);
  }
  egg::lang::Value rhs = rvalue.execute(*this);
  if (rhs.has(egg::lang::Discriminator::Int)) {
    return ints(lhs.getInt(), rhs.getInt());
  }
  if (rhs.has(egg::lang::Discriminator::FlowControl)) {
    return rhs;
  }
  return EggProgramContext::unexpected("Expected right-hand side of " + std::string(operation) + " to be 'int'", rhs);
}

egg::lang::Value egg::yolk::EggProgramContext::call(const egg::lang::Value& callee, const egg::lang::IParameters& parameters) {
  if (!callee.has(egg::lang::Discriminator::Object)) {
    return EggProgramContext::unexpected("Expected function-like expression to be 'object'", callee);
  }
  auto& object = callee.getObject();
  return object.call(*this, parameters);
}

egg::lang::Value egg::yolk::EggProgramContext::unexpected(const std::string& expectation, const egg::lang::Value& value) {
  return egg::lang::Value::raise(expectation, ", but got '", value.getTagString(), "' instead");
}

void egg::yolk::EggProgramContext::print(const std::string & utf8) {
  return this->log(egg::lang::LogSource::User, egg::lang::LogSeverity::Information, utf8);
}
