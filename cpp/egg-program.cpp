#include "yolk.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"
#include "egg-program.h"

namespace {
  using namespace egg::yolk;
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeModule(const IEggParserNode&, const std::vector<std::shared_ptr<IEggParserNode>>& statements) {
  for (auto& statement : statements) {
    //statement->dump(std::cout);
    //std::cout << std::endl;
    //statement->execute(*this);
    EggParserSymbol symbol;
    if (statement->symbol(symbol)) {
      std::cout << "SYMBOL:" << symbol.name << std::endl;
    }
  }
  this->print("execute");
  return egg::lang::ExecutionResult::Void;
}

static egg::lang::ExecutionResult WIBBLE(std::string function, ...) {
  EGG_THROW(function + " unimplemented: TODO");
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeBlock(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& statements) {
  return WIBBLE(__FUNCTION__, &self, &statements);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeType(const IEggParserNode& self, const IEggParserType& type) {
  return WIBBLE(__FUNCTION__, &self, &type);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeDeclare(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode* rvalue) {
  return WIBBLE(__FUNCTION__, &self, &name, &type, &rvalue);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeAssign(const IEggParserNode& self, EggParserAssign op, const IEggParserNode& lvalue, const IEggParserNode& rvalue) {
  return WIBBLE(__FUNCTION__, &self, &op, &lvalue, &rvalue);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeMutate(const IEggParserNode& self, EggParserMutate op, const IEggParserNode& lvalue) {
  return WIBBLE(__FUNCTION__, &self, &op, &lvalue);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeBreak(const IEggParserNode& self) {
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeCatch(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &name, &type, &block);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeContinue(const IEggParserNode& self) {
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeDo(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &condition, &block);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeIf(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& trueBlock, const IEggParserNode* falseBlock) {
  return WIBBLE(__FUNCTION__, &self, &condition, &trueBlock, &falseBlock);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeFor(const IEggParserNode& self, const IEggParserNode* pre, const IEggParserNode* cond, const IEggParserNode* post, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &pre, &cond, &post, &block);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeForeach(const IEggParserNode& self, const IEggParserNode& lvalue, const IEggParserNode& rvalue, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &lvalue, &rvalue, &block);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeReturn(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values) {
  return WIBBLE(__FUNCTION__, &self, &values);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeCase(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &values, &block);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeSwitch(const IEggParserNode& self, const IEggParserNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggParserNode>>& cases) {
  return WIBBLE(__FUNCTION__, &self, &value, &defaultIndex, &cases);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeThrow(const IEggParserNode& self, const IEggParserNode* exception) {
  return WIBBLE(__FUNCTION__, &self, &exception);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeTry(const IEggParserNode& self, const IEggParserNode& block, const std::vector<std::shared_ptr<IEggParserNode>>& catches, const IEggParserNode* final) {
  return WIBBLE(__FUNCTION__, &self, &block, &catches, &final);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeUsing(const IEggParserNode& self, const IEggParserNode& value, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &value, &block);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeWhile(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block) {
  return WIBBLE(__FUNCTION__, &self, &condition, &block);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeYield(const IEggParserNode& self, const IEggParserNode& value) {
  return WIBBLE(__FUNCTION__, &self, &value);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeCall(const IEggParserNode& self, const IEggParserNode& callee, const std::vector<std::shared_ptr<IEggParserNode>>& parameters) {
  return WIBBLE(__FUNCTION__, &self, &callee, &parameters);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeIdentifier(const IEggParserNode& self, const std::string& name) {
  return WIBBLE(__FUNCTION__, &self, &name);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeLiteral(const IEggParserNode& self, ...) {
  return WIBBLE(__FUNCTION__, &self);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeLiteral(const IEggParserNode& self, const std::string& value) {
  return WIBBLE(__FUNCTION__, &self, &value);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeUnary(const IEggParserNode& self, EggParserUnary op, const IEggParserNode& value) {
  return WIBBLE(__FUNCTION__, &self, &op, &value);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeBinary(const IEggParserNode& self, EggParserBinary op, const IEggParserNode& lhs, const IEggParserNode& rhs) {
  return WIBBLE(__FUNCTION__, &self, &op, &lhs, &rhs);
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeTernary(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& whenTrue, const IEggParserNode& whenFalse) {
  return WIBBLE(__FUNCTION__, &self, &condition, &whenTrue, &whenFalse);
}

void egg::yolk::EggEngineProgram::execute(IEggEngineExecutionContext& execution) {
  this->root->execute(execution);
}
