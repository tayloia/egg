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

egg::lang::ExecutionResult egg::yolk::IEggParserNode::execute(IEggEngineExecutionContext& execution) const {
  // TODO remove and make pure virtual
  std::stringstream ss;
  this->dump(ss);
  execution.print("EXECUTE:" + ss.str());
  return{};
}

egg::lang::ExecutionResult IEggEngineExecutionContext::executeModule(const IEggParserNode&, const std::vector<std::shared_ptr<IEggParserNode>>& statements) {
  for (auto& statement : statements) {
    statement->execute(*this);
  }
  return egg::lang::ExecutionResult::Void;
}

void egg::yolk::EggEngineProgram::execute(IEggEngineExecutionContext& execution) {
  this->root->execute(execution);
}
