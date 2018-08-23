#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

#include "ovum/node.h"
#include "ovum/module.h"

egg::ovum::ILogger::Severity egg::yolk::EggProgram::compile(IEggEngineCompilationContext& compilation, egg::ovum::Module& out) {
  auto& allocator = compilation.allocator();
  auto node = egg::ovum::NodeFactory::create(allocator, egg::ovum::OPCODE_NOOP);
  node = egg::ovum::NodeFactory::create(allocator, egg::ovum::OPCODE_BLOCK, node);
  node = egg::ovum::NodeFactory::create(allocator, egg::ovum::OPCODE_MODULE, node);
  out = egg::ovum::ModuleFactory::fromRootNode(allocator, *node);
  return egg::ovum::ILogger::Severity::Warning;
}
