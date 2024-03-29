#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

egg::yolk::EggProgramCompilerNode& egg::yolk::EggProgramCompilerNode::add(const egg::ovum::Node& child) {
  this->nodes.push_back(child);
  return *this;
}

egg::yolk::EggProgramCompilerNode& egg::yolk::EggProgramCompilerNode::add(const IEggProgramNode& child) {
  auto node = child.compile(this->compiler);
  if (node == nullptr) {
    // We assume the error message was logged in the compile call immediate above
    this->failed = true;
  }
  this->nodes.emplace_back(std::move(node));
  return *this;
}

egg::yolk::EggProgramCompilerNode& egg::yolk::EggProgramCompilerNode::add(const std::shared_ptr<IEggProgramNode>& child) {
  if (child != nullptr) {
    return this->add(*child);
  }
  this->failed = true;
  this->nodes.push_back(this->compiler.raise("Unexpected null child at index ", std::to_string(this->nodes.size()), " of ", egg::ovum::OpcodeProperties::from(this->opcode).name));
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
    if (this->nodes.empty() && (this->opcode == egg::ovum::Opcode::BLOCK)) {
      // Handle the special case of empty blocks needing a noop
      this->add(this->compiler.create(this->location, egg::ovum::Opcode::NOOP, nullptr));
    }
    return this->compiler.create(this->location, this->opcode, &this->nodes);
  }
  return nullptr;
}

egg::ovum::Node egg::yolk::EggProgramCompilerNode::build(egg::ovum::Operator operand) {
  if (!this->failed) {
    return this->compiler.create(this->location, this->opcode, &this->nodes, nullptr, operand);
  }
  return nullptr;
}

egg::ovum::Node egg::yolk::EggProgramCompiler::opcode(const egg::ovum::LocationSource& location, egg::ovum::Opcode value) {
  egg::ovum::NodeLocation loc{ location.line, location.column };
  return this->create(loc, value, nullptr);
}

egg::ovum::Node egg::yolk::EggProgramCompiler::ivalue(const egg::ovum::LocationSource& location, egg::ovum::Int value) {
  egg::ovum::NodeLocation loc{ location.line, location.column };
  return this->create(loc, egg::ovum::Opcode::IVALUE, nullptr, nullptr, value);
}

egg::ovum::Node egg::yolk::EggProgramCompiler::fvalue(const egg::ovum::LocationSource& location, egg::ovum::Float value) {
  egg::ovum::NodeLocation loc{ location.line, location.column };
  return this->create(loc, egg::ovum::Opcode::FVALUE, nullptr, nullptr, value);
}

egg::ovum::Node egg::yolk::EggProgramCompiler::svalue(const egg::ovum::LocationSource& location, const egg::ovum::String& value) {
  egg::ovum::NodeLocation loc{ location.line, location.column };
  return this->create(loc, egg::ovum::Opcode::SVALUE, nullptr, nullptr, value);
}

egg::ovum::Node egg::yolk::EggProgramCompiler::type(const egg::ovum::LocationSource& location, const egg::ovum::Type& type) {
  auto& allocator = this->context.getAllocator();
  return egg::ovum::NodeFactory::createType(allocator, { location.line, location.column }, type);
}

egg::ovum::Node egg::yolk::EggProgramCompiler::identifier(const egg::ovum::LocationSource& location, const egg::ovum::String& id) {
  assert(!id.empty());
  egg::ovum::NodeLocation loc{ location.line, location.column };
  egg::ovum::Nodes children{ this->svalue(location, id) };
  return this->create(loc, egg::ovum::Opcode::IDENTIFIER, &children);
}

egg::ovum::Node egg::yolk::EggProgramCompiler::unary(const egg::ovum::LocationSource& location, EggProgramUnary op, const IEggProgramNode& a) {
  switch (op) {
  case EggProgramUnary::LogicalNot: return this->operation(location, egg::ovum::Opcode::UNARY, egg::ovum::Operator::LOGNOT, a);
  case EggProgramUnary::Ref: return this->operation(location, egg::ovum::Opcode::UNARY, egg::ovum::Operator::REF, a);
  case EggProgramUnary::Deref: return this->operation(location, egg::ovum::Opcode::UNARY, egg::ovum::Operator::DEREF, a);
  case EggProgramUnary::Negate: return this->operation(location, egg::ovum::Opcode::UNARY, egg::ovum::Operator::NEG, a);
  case EggProgramUnary::Ellipsis: return this->expression(location, egg::ovum::Opcode::ELLIPSIS, a);
  case EggProgramUnary::BitwiseNot: return this->operation(location, egg::ovum::Opcode::UNARY, egg::ovum::Operator::BITNOT, a);
  }
  return this->raise("Unsupported unary operator");
}

egg::ovum::Node egg::yolk::EggProgramCompiler::binary(const egg::ovum::LocationSource& location, EggProgramBinary op, const IEggProgramNode& a, const IEggProgramNode& b) {
  switch (op) {
  case EggProgramBinary::Unequal: return this->operation(location, egg::ovum::Opcode::COMPARE, egg::ovum::Operator::NE, a, b);
  case EggProgramBinary::Remainder: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::REM, a, b);
  case EggProgramBinary::BitwiseAnd: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::BITAND, a, b);
  case EggProgramBinary::LogicalAnd: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::LOGAND, a, b);
  case EggProgramBinary::Multiply: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::MUL, a, b);
  case EggProgramBinary::Plus: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::ADD, a, b);
  case EggProgramBinary::Minus: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::SUB, a, b);
  case EggProgramBinary::Divide: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::DIV, a, b);
  case EggProgramBinary::Less: return this->operation(location, egg::ovum::Opcode::COMPARE, egg::ovum::Operator::LT, a, b);
  case EggProgramBinary::ShiftLeft: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::SHIFTL, a, b);
  case EggProgramBinary::LessEqual: return this->operation(location, egg::ovum::Opcode::COMPARE, egg::ovum::Operator::LE, a, b);
  case EggProgramBinary::Equal: return this->operation(location, egg::ovum::Opcode::COMPARE, egg::ovum::Operator::EQ, a, b);
  case EggProgramBinary::Greater: return this->operation(location, egg::ovum::Opcode::COMPARE, egg::ovum::Operator::GT, a, b);
  case EggProgramBinary::GreaterEqual: return this->operation(location, egg::ovum::Opcode::COMPARE, egg::ovum::Operator::GE, a, b);
  case EggProgramBinary::ShiftRight: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::SHIFTR, a, b);
  case EggProgramBinary::ShiftRightUnsigned: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::SHIFTU, a, b);
  case EggProgramBinary::NullCoalescing: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::IFNULL, a, b);
  case EggProgramBinary::BitwiseXor: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::BITXOR, a, b);
  case EggProgramBinary::BitwiseOr: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::BITOR, a, b);
  case EggProgramBinary::LogicalOr: return this->operation(location, egg::ovum::Opcode::BINARY, egg::ovum::Operator::LOGOR, a, b);
  case EggProgramBinary::Lambda:
    break;
  }
  return this->raise("Unsupported binary operator");
}

egg::ovum::Node egg::yolk::EggProgramCompiler::ternary(const egg::ovum::LocationSource& location, EggProgramTernary op, const IEggProgramNode& a, const IEggProgramNode& b, const IEggProgramNode& c) {
  switch (op) {
  case EggProgramTernary::Ternary: return this->operation(location, egg::ovum::Opcode::TERNARY, egg::ovum::Operator::TERNARY, a, b, c);
  }
  return this->raise("Unsupported ternary operator");
}

egg::ovum::Node egg::yolk::EggProgramCompiler::mutate(const egg::ovum::LocationSource& location, EggProgramMutate op, const IEggProgramNode& a) {
  switch (op) {
  case EggProgramMutate::Decrement: return this->statement(location, egg::ovum::Opcode::DECREMENT, a);
  case EggProgramMutate::Increment: return this->statement(location, egg::ovum::Opcode::INCREMENT, a);
  }
  return this->raise("Unsupported mutation operator");
}

egg::ovum::Node egg::yolk::EggProgramCompiler::assign(const egg::ovum::LocationSource& location, EggProgramAssign op, const IEggProgramNode& a, const IEggProgramNode& b) {
  switch (op) {
  case EggProgramAssign::Remainder: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::REM, a, b);
  case EggProgramAssign::BitwiseAnd: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::BITAND, a, b);
  case EggProgramAssign::LogicalAnd: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::LOGAND, a, b);
  case EggProgramAssign::Multiply: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::MUL, a, b);
  case EggProgramAssign::Plus: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::ADD, a, b);
  case EggProgramAssign::Minus: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::SUB, a, b);
  case EggProgramAssign::Divide: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::DIV, a, b);
  case EggProgramAssign::ShiftLeft: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::SHIFTL, a, b);
  case EggProgramAssign::Equal: return this->statement(location, egg::ovum::Opcode::ASSIGN, a, b);
  case EggProgramAssign::ShiftRight: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::SHIFTR, a, b);
  case EggProgramAssign::ShiftRightUnsigned: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::SHIFTU, a, b);
  case EggProgramAssign::NullCoalescing: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::IFNULL, a, b);
  case EggProgramAssign::BitwiseXor: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::BITXOR, a, b);
  case EggProgramAssign::BitwiseOr: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::BITOR, a, b);
  case EggProgramAssign::LogicalOr: return this->operation(location, egg::ovum::Opcode::MUTATE, egg::ovum::Operator::LOGOR, a, b);
  }
  return this->raise("Unsupported assignment operator");
}

egg::ovum::Node egg::yolk::EggProgramCompiler::predicate(const egg::ovum::LocationSource& location, EggProgramBinary op, const IEggProgramNode& a, const IEggProgramNode& b) {
  auto child = this->binary(location, op, a, b);
  return this->expression(location, egg::ovum::Opcode::PREDICATE, child);
}

egg::ovum::Node egg::yolk::EggProgramCompiler::noop(const egg::ovum::LocationSource& location, const IEggProgramNode* node) {
  if (node == nullptr) {
    return this->opcode(location, egg::ovum::Opcode::NOOP);
  }
  return node->compile(*this);
}

egg::ovum::ILogger::Severity egg::yolk::EggProgram::compile(IEggEngineContext& context, egg::ovum::Module& out) {
  assert(this->root != nullptr);
  EggProgramCompiler compiler(context);
  auto node = this->root->compile(compiler);
  if (node != nullptr) {
    out = egg::ovum::ModuleFactory::fromRootNode(context.getTypeFactory(), this->resource, *node);
    return egg::ovum::ILogger::Severity::None;
  }
  return egg::ovum::ILogger::Severity::Error;
}
