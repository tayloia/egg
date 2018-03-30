#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

#include <set>

class egg::yolk::EggEngineProgramContext::SymbolTable {
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

void egg::yolk::EggEngineProgramContext::log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) {
  if (severity > this->maximumSeverity) {
    this->maximumSeverity = severity;
  }
  this->execution->log(source, severity, message);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeModule(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& statements) {
  this->statement(self);
  this->symtable->addSymbol("print")->value = egg::lang::Value("TODO print");
  {
    // Add all the omnipresent symbols whilst checking for duplicate symbols
    std::set<std::string> seen;
    for (auto& statement : statements) {
      EggParserSymbol symbol;
      if (statement->symbol(symbol)) {
        if (!seen.insert(symbol.name).second) {
          // Already seen
          this->log(egg::lang::LogSource::Compiler, egg::lang::LogSeverity::Error, "Duplicate symbol declared at module level: '" + symbol.name + "'");
        }
        if (symbol.omnipresent) {
          this->symtable->addSymbol(symbol.name)->value = egg::lang::Value("TODO omnipresent function");
        }
      }
    }
    if (this->getMaximumSeverity() == egg::lang::LogSeverity::Error) {
      return egg::lang::Value::Null; // TODO
    }
  }
  // Now execute the statements
  for (auto& statement : statements) {
    EggParserSymbol symbol;
    if (statement->symbol(symbol) && !symbol.omnipresent) {
      // We've checked for duplicate symbols above
      this->symtable->addSymbol(symbol.name);
    }
    statement->execute(*this);
  }
  this->execution->print("execute");
  return egg::lang::Value::Void;
}

static egg::lang::Value WIBBLE(std::string function, ...) {
  if (!function.empty()) {
    EGG_THROW(function + " unimplemented: TODO");
  }
  return egg::lang::Value::Null;
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeBlock(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& statements) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &statements);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeType(const IEggParserNode& self, const IEggParserType& type) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &type);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeDeclare(const IEggParserNode& self, const std::string& name, const IEggParserNode&, const IEggParserNode* rvalue) {
  // The type information has already been used in the symbol declaration phase
  this->statement(self);
  if (rvalue != nullptr) {
    this->set(name, *rvalue);
  }
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeAssign(const IEggParserNode& self, EggParserAssign op, const IEggParserNode& lvalue, const IEggParserNode& rvalue) {
  this->statement(self);
  this->assign(op, lvalue, rvalue);
  return egg::lang::Value::Void;
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeMutate(const IEggParserNode& self, EggParserMutate op, const IEggParserNode& lvalue) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &op, &lvalue);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeBreak(const IEggParserNode& self) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeCatch(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &name, &type, &block);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeContinue(const IEggParserNode& self) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeDo(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &condition, &block);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeIf(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& trueBlock, const IEggParserNode* falseBlock) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &condition, &trueBlock, &falseBlock);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeFor(const IEggParserNode& self, const IEggParserNode* pre, const IEggParserNode* cond, const IEggParserNode* post, const IEggParserNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &pre, &cond, &post, &block);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeForeach(const IEggParserNode& self, const IEggParserNode& lvalue, const IEggParserNode& rvalue, const IEggParserNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &lvalue, &rvalue, &block);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeReturn(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &values);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeCase(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values, const IEggParserNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &values, &block);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeSwitch(const IEggParserNode& self, const IEggParserNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggParserNode>>& cases) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &value, &defaultIndex, &cases);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeThrow(const IEggParserNode& self, const IEggParserNode* exception) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &exception);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeTry(const IEggParserNode& self, const IEggParserNode& block, const std::vector<std::shared_ptr<IEggParserNode>>& catches, const IEggParserNode* final) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &block, &catches, &final);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeUsing(const IEggParserNode& self, const IEggParserNode& value, const IEggParserNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &value, &block);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeWhile(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &condition, &block);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeYield(const IEggParserNode& self, const IEggParserNode& value) {
  this->statement(self);
  return WIBBLE(__FUNCTION__, &self, &value);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeCall(const IEggParserNode& self, const IEggParserNode& callee, const std::vector<std::shared_ptr<IEggParserNode>>& parameters) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &callee, &parameters);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeIdentifier(const IEggParserNode& self, const std::string& name) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &name);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeLiteral(const IEggParserNode& self, ...) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeLiteral(const IEggParserNode& self, const std::string& value) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &value);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeUnary(const IEggParserNode& self, EggParserUnary op, const IEggParserNode& value) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &op, &value);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeBinary(const IEggParserNode& self, EggParserBinary op, const IEggParserNode& lhs, const IEggParserNode& rhs) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &op, &lhs, &rhs);
}

egg::lang::Value egg::yolk::EggEngineProgramContext::executeTernary(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& whenTrue, const IEggParserNode& whenFalse) {
  this->expression(self);
  return WIBBLE(__FUNCTION__, &self, &condition, &whenTrue, &whenFalse);
}

egg::lang::LogSeverity egg::yolk::EggEngineProgram::execute(IEggEngineExecutionContext& execution) {
  EggEngineProgramContext::SymbolTable symtable(nullptr);
  // TODO add built-in symbol to symbol table here
  EggEngineProgramContext context(execution, symtable);
  this->root->execute(context);
  return context.getMaximumSeverity();
}

void egg::yolk::EggEngineProgramContext::statement(const IEggParserNode&) {
  // TODO
}

void egg::yolk::EggEngineProgramContext::expression(const IEggParserNode&) {
  // TODO
}

void egg::yolk::EggEngineProgramContext::set(const std::string& name, const IEggParserNode& rvalue) {
  // TODO
  auto symbol = this->symtable->findSymbol(name);
  assert(symbol != nullptr);
  symbol->value = rvalue.execute(*this);
}

void egg::yolk::EggEngineProgramContext::assign(EggParserAssign op, const IEggParserNode& lvalue, const IEggParserNode& rvalue) {
  // TODO
  WIBBLE(__FUNCTION__, &op, &lvalue, &rvalue);
}
