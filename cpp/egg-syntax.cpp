#include "yolk.h"

namespace {
  using namespace egg::yolk;

  class EggSyntaxNodeVisitor : public IEggSyntaxNodeVisitor {
  private:
    std::ostream* os;
  public:
    explicit EggSyntaxNodeVisitor(std::ostream& os)
      : os(&os) {
    }
    virtual void node(const std::string& prefix, IEggSyntaxNode* children[], size_t count) override {
      *this->os << '(' << prefix;
      for (size_t i = 0; i < count; ++i) {
        *this->os << ' ';
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
    *dst = src.get();
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
  visitor.node("module", this->children);
}

void egg::yolk::EggSyntaxNode_Type::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("type");
}

void egg::yolk::EggSyntaxNode_VariableDeclaration::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("declare " + this->name, this->child);
}

void egg::yolk::EggSyntaxNode_VariableDefinition::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("define " + this->name, this->child);
}

void egg::yolk::EggSyntaxNode_Assignment::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("binary", this->child);
}

void egg::yolk::EggSyntaxNode_UnaryOperator::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("unary", this->child);
}

void egg::yolk::EggSyntaxNode_BinaryOperator::visit(IEggSyntaxNodeVisitor& visitor) {
  IEggSyntaxNode* pointers[] = { this->child[0].get(), this->child[1].get() };
  visitor.node("binary", pointers, EGG_NELEMS(pointers));
}

void egg::yolk::EggSyntaxNode_TernaryOperator::visit(IEggSyntaxNodeVisitor& visitor) {
  IEggSyntaxNode* pointers[] = { this->child[0].get(), this->child[1].get(), this->child[2].get() };
  visitor.node("ternary", pointers, 3);
}

void egg::yolk::EggSyntaxNode_Identifier::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("identifier " + this->name);
}

void egg::yolk::EggSyntaxNode_Literal::visit(IEggSyntaxNodeVisitor& visitor) {
  visitor.node("literal");
}

std::ostream& egg::yolk::EggSyntaxNode::printToStream(std::ostream& os, IEggSyntaxNode& tree) {
  EggSyntaxNodeVisitor visitor(os);
  tree.visit(visitor);
  return os;
};
