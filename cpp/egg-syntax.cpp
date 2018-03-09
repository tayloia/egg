#include "yolk.h"

namespace {
  using namespace egg::yolk;

  class EggSyntaxNodeVisitor : public IEggSyntaxNodeVisitor {
  private:
    std::ostream* os;
    size_t indent;
  public:
    explicit EggSyntaxNodeVisitor(std::ostream& os)
      : os(&os), indent(0) {
    }
    virtual void node(const std::string& prefix, IEggSyntaxNode* children[], size_t count) override {
      for (size_t i = 0; i < this->indent; ++i) {
        this->os->write("  ", 2);
      }
      *this->os << '(' << prefix;
      this->indent++;
      for (size_t i = 0; i < count; ++i) {
        *this->os << std::endl;
        assert(children[i] != nullptr);
        children[i]->visit(*this);
      }
      this->indent--;
      *this->os << ')';
    }
  };

  class EggSyntaxNodeVisitorConcise : public IEggSyntaxNodeVisitor {
  private:
    std::ostream* os;
  public:
    explicit EggSyntaxNodeVisitorConcise(std::ostream& os)
      : os(&os) {
    }
    virtual void node(const std::string& prefix, IEggSyntaxNode* children[], size_t count) override {
      *this->os << '(' << prefix;
      for (size_t i = 0; i < count; ++i) {
        *this->os << ' ';
        assert(children[i] != nullptr);
        children[i]->visit(*this);
      }
      *this->os << ')';
    }
  };
}

void egg::yolk::IEggSyntaxNodeVisitor::node(const std::string & prefix, const std::vector<std::unique_ptr<IEggSyntaxNode>>& children) {
  std::vector<IEggSyntaxNode*> pointers(children.size());
  auto dst = pointers.begin();
  for (auto& src : children) {
    *(dst++) = src.get();
  }
  this->node(prefix, pointers.data(), pointers.size());
}

void egg::yolk::IEggSyntaxNodeVisitor::node(const std::string & prefix, const std::unique_ptr<IEggSyntaxNode>& child) {
  IEggSyntaxNode* pointer = child.get();
  this->node(prefix, &pointer, 1);
}

void egg::yolk::IEggSyntaxNodeVisitor::node(const std::string & prefix) {
  this->node(prefix, nullptr, 0);
}

egg::yolk::IEggSyntaxNode& egg::yolk::IEggSyntaxNode::getChild(size_t index) const {
  auto* child = this->tryGetChild(index);
  if (child == nullptr) {
    EGG_THROW("No such child in syntax tree node");
  }
  return *child;
}

void egg::yolk::EggSyntaxNode_Module::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("module", this->child);
}

void egg::yolk::EggSyntaxNode_Block::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("block", this->child);
}

void egg::yolk::EggSyntaxNode_Type::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("type '" + EggSyntaxNode_Type::to_string(this->allowed) + "'");
}

static void EggSyntaxNode_Type_component(std::string& dst, const char* text, int mask) {
  if (mask) {
    if (!dst.empty()) {
      dst.assign("|");
    }
    dst.append(text);
  }
}

std::string egg::yolk::EggSyntaxNode_Type::to_string(Allowed allowed) {
  if (allowed == Inferred) {
    return "var";
  }
  if (allowed == Void) {
    return "void";
  }
  if (allowed == (Bool|Int|Float|String|Object)) {
    return "any";
  }
  if (allowed == (Void|Bool|Int|Float|String|Object)) {
    return "any?";
  }
  std::string result;
  EggSyntaxNode_Type_component(result, "bool", allowed & Bool);
  EggSyntaxNode_Type_component(result, "int", allowed & Int);
  EggSyntaxNode_Type_component(result, "float", allowed & Float);
  EggSyntaxNode_Type_component(result, "string", allowed & String);
  EggSyntaxNode_Type_component(result, "object", allowed & Object);
  if (allowed & Void) {
    result.append("?");
  }
  return result;
}

void egg::yolk::EggSyntaxNode_VariableDeclaration::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("declare '" + this->name + "'", this->child);
}

void egg::yolk::EggSyntaxNode_VariableInitialization::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("initialize '" + this->name + "'", this->child);
}

void egg::yolk::EggSyntaxNode_Assignment::visit(IEggSyntaxNodeVisitor& visitor) {
  auto prefix = "assign '" + EggTokenizerValue::getOperatorString(this->op) + "'";
  visitor.node(prefix, this->child);
}

void egg::yolk::EggSyntaxNode_Mutate::visit(IEggSyntaxNodeVisitor& visitor) {
  auto prefix = "mutate '" + EggTokenizerValue::getOperatorString(this->op) + "'";
  visitor.node(prefix, this->child);
}

void egg::yolk::EggSyntaxNode_Break::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("break");
}

void egg::yolk::EggSyntaxNode_Case::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("case", this->child);
}

void egg::yolk::EggSyntaxNode_Catch::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("catch '" + this->name + "'", this->child);
}

void egg::yolk::EggSyntaxNode_Continue::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("continue");
}

void egg::yolk::EggSyntaxNode_Default::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("default");
}

void egg::yolk::EggSyntaxNode_Do::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("do", this->child);
}

void egg::yolk::EggSyntaxNode_If::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("if", this->child);
}

void egg::yolk::EggSyntaxNode_Finally::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("finally", this->child);
}

void egg::yolk::EggSyntaxNode_Return::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("return", this->child);
}

void egg::yolk::EggSyntaxNode_Switch::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("switch", this->child);
}

void egg::yolk::EggSyntaxNode_Try::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("try", this->child);
}

void egg::yolk::EggSyntaxNode_While::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("while", this->child);
}

void egg::yolk::EggSyntaxNode_Yield::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("yield", this->child);
}

void egg::yolk::EggSyntaxNode_UnaryOperator::visit(IEggSyntaxNodeVisitor& visitor) {
  auto prefix = "unary '" + EggTokenizerValue::getOperatorString(this->op) + "'";
  visitor.node(prefix, this->child);
}

void egg::yolk::EggSyntaxNode_BinaryOperator::visit(IEggSyntaxNodeVisitor& visitor) {
  IEggSyntaxNode* pointers[] = { this->child[0].get(), this->child[1].get() };
  auto prefix = "binary '" + EggTokenizerValue::getOperatorString(this->op) + "'";
  visitor.node(prefix, pointers, EGG_NELEMS(pointers));
}

void egg::yolk::EggSyntaxNode_TernaryOperator::visit(IEggSyntaxNodeVisitor& visitor) {
  IEggSyntaxNode* pointers[] = { this->child[0].get(), this->child[1].get(), this->child[2].get() };
  visitor.node("ternary", pointers, 3);
}

void egg::yolk::EggSyntaxNode_Call::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("call", this->child);
}

void egg::yolk::EggSyntaxNode_Named::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("named '" + this->name + "'", this->child);
}

void egg::yolk::EggSyntaxNode_Identifier::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("identifier '" + this->name + "'");
}

void egg::yolk::EggSyntaxNode_Literal::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("literal " + this->to_string());
}

std::string egg::yolk::EggSyntaxNode_Literal::to_string() const {
  switch (this->kind) {
  case EggTokenizerKind::Integer:
    return "int " + this->value.s;
  case EggTokenizerKind::Float:
    return "float " + this->value.s;
  case EggTokenizerKind::String:
    return "string '" + this->value.s + "'";
  case EggTokenizerKind::Keyword:
  case EggTokenizerKind::Operator:
  case EggTokenizerKind::Identifier:
  case EggTokenizerKind::Attribute:
  case EggTokenizerKind::EndOfFile:
  default:
    return "unknown type";
  }
}

std::ostream& egg::yolk::EggSyntaxNode::printToStream(std::ostream& os, IEggSyntaxNode& tree, bool concise) {
  if (concise) {
    EggSyntaxNodeVisitorConcise visitor(os);
    tree.visit(visitor);
  } else {
    EggSyntaxNodeVisitor visitor(os);
    tree.visit(visitor);
  }
  return os;
};
