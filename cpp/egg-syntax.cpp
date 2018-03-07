#include "yolk.h"

egg::yolk::IEggSyntaxNode& egg::yolk::IEggSyntaxNode::getChild(size_t index) const {
  auto* child = this->tryGetChild(index);
  if (child == nullptr) {
    EGG_THROW("No such child in syntax tree node");
  }
  return *child;
}
