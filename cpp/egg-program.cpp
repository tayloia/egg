#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

#include <set>

namespace {
  using namespace egg::yolk;

  struct Symbol {
    std::string name;
    egg::lang::Value value;
    explicit Symbol(const std::string& name)
      : name(name), value(egg::lang::Value::Void) {
    }
  };

  class SymbolTable {
    EGG_NO_COPY(SymbolTable);
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
}

void EggEngineProgramContext::log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) {
  if (severity > this->maximumSeverity) {
    this->maximumSeverity = severity;
  }
  this->execution->log(source, severity, message);
}

egg::lang::Value EggEngineProgramContext::executeModule(const IEggParserNode&, const std::vector<std::shared_ptr<IEggParserNode>>& statements) {
  SymbolTable symtable(nullptr);
  symtable.addSymbol("print")->value = egg::lang::Value("TODO print");
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
        symtable.addSymbol(symbol.name)->value = egg::lang::Value("TODO omnipresent function");
      }
    }
  }
  if (this->getMaximumSeverity() == egg::lang::LogSeverity::Error) {
    return egg::lang::Value::Null; // TODO
  }
  // Now execute the statements
  for (auto& statement : statements) {
    EggParserSymbol symbol;
    if (statement->symbol(symbol) && !symbol.omnipresent) {
      // We've checked for duplicate symbols above
      symtable.addSymbol(symbol.name);
    }
    // WIBBLE statement->execute(*this);
  }
  this->execution->print("execute");
  return egg::lang::Value::Void;
}

static egg::lang::Value WIBBLE(std::string function, ...) {
  EGG_THROW(function + " unimplemented: TODO");
}

egg::lang::Value EggEngineProgramContext::executeBlock(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& statements) {
  return WIBBLE(__FUNCTION__, &self, &statements);
}

egg::lang::Value EggEngineProgramContext::executeType(const IEggParserNode& self, const IEggParserType& type) {
  return WIBBLE(__FUNCTION__, &self, &type);
}

egg::lang::Value EggEngineProgramContext::executeDeclare(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode* rvalue) {
  return WIBBLE(__FUNCTION__, &self, &name, &type, &rvalue);
}

egg::lang::Value EggEngineProgramContext::executeAssign(const IEggParserNode& self, EggParserAssign op, const IEggParserNode& lvalue, const IEggParserNode& rvalue) {
  return WIBBLE(__FUNCTION__, &self, &op, &lvalue, &rvalue);
}

egg::lang::Value EggEngineProgramContext::executeMutate(const IEggParserNode& self, EggParserMutate op, const IEggParserNode& lvalue) {
  return WIBBLE(__FUNCTION__, &self, &op, &lvalue);
}

egg::lang::Value EggEngineProgramContext::executeBreak(const IEggParserNode& self) {
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::Value EggEngineProgramContext::executeCatch(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &name, &type, &block);
}

egg::lang::Value EggEngineProgramContext::executeContinue(const IEggParserNode& self) {
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::Value EggEngineProgramContext::executeDo(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &condition, &block);
}

egg::lang::Value EggEngineProgramContext::executeIf(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& trueBlock, const IEggParserNode* falseBlock) {
  return WIBBLE(__FUNCTION__, &self, &condition, &trueBlock, &falseBlock);
}

egg::lang::Value EggEngineProgramContext::executeFor(const IEggParserNode& self, const IEggParserNode* pre, const IEggParserNode* cond, const IEggParserNode* post, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &pre, &cond, &post, &block);
}

egg::lang::Value EggEngineProgramContext::executeForeach(const IEggParserNode& self, const IEggParserNode& lvalue, const IEggParserNode& rvalue, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &lvalue, &rvalue, &block);
}

egg::lang::Value EggEngineProgramContext::executeReturn(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values) {
  return WIBBLE(__FUNCTION__, &self, &values);
}

egg::lang::Value EggEngineProgramContext::executeCase(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &values, &block);
}

egg::lang::Value EggEngineProgramContext::executeSwitch(const IEggParserNode& self, const IEggParserNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggParserNode>>& cases) {
  return WIBBLE(__FUNCTION__, &self, &value, &defaultIndex, &cases);
}

egg::lang::Value EggEngineProgramContext::executeThrow(const IEggParserNode& self, const IEggParserNode* exception) {
  return WIBBLE(__FUNCTION__, &self, &exception);
}

egg::lang::Value EggEngineProgramContext::executeTry(const IEggParserNode& self, const IEggParserNode& block, const std::vector<std::shared_ptr<IEggParserNode>>& catches, const IEggParserNode* final) {
  return WIBBLE(__FUNCTION__, &self, &block, &catches, &final);
}

egg::lang::Value EggEngineProgramContext::executeUsing(const IEggParserNode& self, const IEggParserNode& value, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &value, &block);
}

egg::lang::Value EggEngineProgramContext::executeWhile(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &condition, &block);
}

egg::lang::Value EggEngineProgramContext::executeYield(const IEggParserNode& self, const IEggParserNode& value) {
  return WIBBLE(__FUNCTION__, &self, &value);
}

egg::lang::Value EggEngineProgramContext::executeCall(const IEggParserNode& self, const IEggParserNode& callee, const std::vector<std::shared_ptr<IEggParserNode>>& parameters) {
  return WIBBLE(__FUNCTION__, &self, &callee, &parameters);
}

egg::lang::Value EggEngineProgramContext::executeIdentifier(const IEggParserNode& self, const std::string& name) {
  return WIBBLE(__FUNCTION__, &self, &name);
}

egg::lang::Value EggEngineProgramContext::executeLiteral(const IEggParserNode& self, ...) {
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::Value EggEngineProgramContext::executeLiteral(const IEggParserNode& self, const std::string& value) {
  return WIBBLE(__FUNCTION__, &self, &value);
}

egg::lang::Value EggEngineProgramContext::executeUnary(const IEggParserNode& self, EggParserUnary op, const IEggParserNode& value) {
  return WIBBLE(__FUNCTION__, &self, &op, &value);
}

egg::lang::Value EggEngineProgramContext::executeBinary(const IEggParserNode& self, EggParserBinary op, const IEggParserNode& lhs, const IEggParserNode& rhs) {
  return WIBBLE(__FUNCTION__, &self, &op, &lhs, &rhs);
}

egg::lang::Value EggEngineProgramContext::executeTernary(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& whenTrue, const IEggParserNode& whenFalse) {
  return WIBBLE(__FUNCTION__, &self, &condition, &whenTrue, &whenFalse);
}

void egg::yolk::EggEngineProgram::execute(EggEngineProgramContext& context) {
  this->root->execute(context);
}
