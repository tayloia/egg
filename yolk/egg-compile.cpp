#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

egg::ovum::Node egg::yolk::EggProgramCompiler::WIBBLE(const IEggProgramNode& node) {
  std::stringstream ss;
  node.dump(ss);
  return this->raise("Unimplemented compilation for ", ss.str());
}

egg::yolk::EggProgramCompilerNode& egg::yolk::EggProgramCompilerNode::add(const egg::ovum::Node& child) {
  this->nodes.push_back(child);
  return *this;
}

egg::yolk::EggProgramCompilerNode& egg::yolk::EggProgramCompilerNode::add(const std::shared_ptr<IEggProgramNode>& child) {
  if (child == nullptr) {
    this->failed = true;
    this->nodes.push_back(this->compiler.raise("Unexpected null child at index ", std::to_string(this->nodes.size()), " of ", egg::ovum::opcodeProperties(this->opcode).name));
  } else {
    auto node = child->compile(this->compiler);
    if (node == nullptr) {
      // We assume the error message was logged in the compile call immediate above
      this->failed = true;
    }
    this->nodes.emplace_back(std::move(node));
  }
  return *this;
}

egg::yolk::EggProgramCompilerNode& egg::yolk::EggProgramCompilerNode::add(const std::vector<std::shared_ptr<IEggProgramNode>>& children) {
  for (auto& child : children) {
    this->add(child);
  }
  return *this;
}

egg::ovum::Node egg::yolk::EggProgramCompilerNode::build() {
  if (!this->failed) {
    if (this->nodes.empty() && (this->opcode == egg::ovum::OPCODE_BLOCK)) {
      // Handle the special case of empty blocks needing a noop
      this->nodes.emplace_back(egg::ovum::NodeFactory::create(this->compiler.context.allocator(), egg::ovum::OPCODE_NOOP));
    }
    return egg::ovum::NodeFactory::create(this->compiler.context.allocator(), this->opcode, this->nodes);
  }
  return nullptr;
}

egg::ovum::Node egg::yolk::EggProgramCompiler::svalue(const egg::ovum::String& value) {
  return egg::ovum::NodeFactory::create(this->context.allocator(), egg::ovum::OPCODE_SVALUE, nullptr, nullptr, value);
}

egg::ovum::Node egg::yolk::EggProgramCompiler::identifier(const egg::ovum::String& id) {
  return egg::ovum::NodeFactory::create(this->context.allocator(), egg::ovum::OPCODE_IDENTIFIER, this->svalue(id));
}

egg::ovum::ILogger::Severity egg::yolk::EggProgram::compile(IEggEngineCompilationContext& compilation, egg::ovum::Module& out) {
  assert(this->root != nullptr);
  EggProgramCompiler compiler(compilation);
  auto node = this->root->compile(compiler);
  if (node != nullptr) {
    out = egg::ovum::ModuleFactory::fromRootNode(compilation.allocator(), *node);
    return egg::ovum::ILogger::Severity::None;
  }
  return egg::ovum::ILogger::Severity::Error;
}
