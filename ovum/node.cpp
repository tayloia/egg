#include "ovum/ovum.h"
#include "ovum/node.h"

namespace {
  using namespace egg::ovum;

  template<typename EXTRA>
  class NodeContiguous final : public HardReferenceCounted<INode> {
    NodeContiguous(const NodeContiguous&) = delete;
    NodeContiguous& operator=(const NodeContiguous&) = delete;
  private:
    Opcode opcode;
  public:
    NodeContiguous(IAllocator& allocator, Opcode opcode, typename EXTRA::Type operand)
      : HardReferenceCounted(allocator, 0),
        opcode(opcode) {
      new(this->extra()) EXTRA(operand);
    }
    virtual ~NodeContiguous() {
      this->extra()->~EXTRA();
    }
    virtual Opcode getOpcode() const override {
      return this->opcode;
    }
    virtual Operand getOperand() const override {
      return this->extra()->getOperand();
    }
    virtual size_t getChildren() const override {
      return this->extra()->children;
    }
    virtual INode& getChild(size_t index) const override {
      if (index >= this->extra()->children) {
        throw std::out_of_range("Invalid AST node child index");
      }
      return *(this->extra()->base)[index];
    }
    virtual Int getInt() const override {
      return this->extra()->getInt();
    }
    virtual Float getFloat() const override {
      return this->extra()->getFloat();
    }
    virtual String getString() const override {
      return this->extra()->getString();
    }
    virtual Operator getOperator() const override {
      return this->extra()->getOperator();
    }
    virtual size_t getAttributes() const override {
      return this->extra()->attributes;
    }
    virtual INode& getAttribute(size_t index) const override {
      if (index >= this->extra()->attributes) {
        throw std::out_of_range("Invalid AST node attribute index");
      }
      return *(this->extra()->base)[index + this->extra()->children];
    }
    virtual const NodeLocation* getLocation() const override {
      return this->extra()->getLocation();
    }
    virtual void setChild(size_t index, INode& value) override {
      if (index >= this->extra()->children) {
        throw std::out_of_range("Invalid AST node child index");
      }
      auto& slot = (this->extra()->base)[index];
      auto* before = slot;
      slot = HardPtr<INode>::hardAcquire(&value);
      if (before != nullptr) {
        before->hardRelease();
      }
    }
    void initChild(size_t index, const Node& node) {
      assert(index < this->extra()->children);
      this->extra()->base[index] = node.hardAcquire();
    }
    void initChildren(const Nodes& nodes) {
      this->extra()->children = nodes.size();
      auto* p = this->extra()->base;
      for (auto& node : nodes) {
        *(p++) = node.hardAcquire();
      }
    }
    void initAttribute(size_t index, const Node& node) {
      assert(index < this->extra()->attributes);
      this->extra()->base[index + this->extra()->children] = node.hardAcquire();
    }
    void initAttributes(const Nodes& nodes) {
      this->extra()->attributes = nodes.size();
      auto* p = this->extra()->base + this->extra()->children;
      for (auto& node : nodes) {
        *(p++) = node.hardAcquire();
      }
    }
    void initLocation(const NodeLocation* location) {
      if (location != nullptr) {
        auto* p = this->extra()->getLocation();
        p->line = location->line;
        p->column = location->column;
      }
    }
    EXTRA* extra() const {
      return reinterpret_cast<EXTRA*>(const_cast<NodeContiguous*>(this) + 1);
    }
  };

  struct NodeOperandNone {
    using Type = std::nullptr_t;
    explicit NodeOperandNone(Type) {
    }
    INode::Operand getOperand() const {
      return INode::Operand::None;
    }
    Int getInt() const {
      throw std::runtime_error("Attempt to read non-existent integer value of AST node");
    }
    Float getFloat() const {
      throw std::runtime_error("Attempt to read non-existent floating-point value of AST node");
    }
    String getString() const {
      throw std::runtime_error("Attempt to read non-existent string value of AST node");
    }
    Operator getOperator() const {
      throw std::runtime_error("Attempt to read non-existent operator value of AST node");
    }
  };

  struct NodeOperandInt {
    using Type = Int;
    explicit NodeOperandInt(Type operand) : operand(operand) {
    }
    INode::Operand getOperand() const {
      return INode::Operand::Int;
    }
    Int getInt() const {
      return this->operand;
    }
    Float getFloat() const {
      throw std::runtime_error("Attempt to read floating-point value of AST node with integer value");
    }
    String getString() const {
      throw std::runtime_error("Attempt to read string value of AST node with integer value");
    }
    Operator getOperator() const {
      throw std::runtime_error("Attempt to read operator value of AST node with integer value");
    }
    Int operand;
  };

  struct NodeOperandFloat {
    using Type = Float;
    explicit NodeOperandFloat(Type operand) : operand(operand) {
    }
    INode::Operand getOperand() const {
      return INode::Operand::Float;
    }
    Int getInt() const {
      throw std::runtime_error("Attempt to read integer value of AST node with floating-point value");
    }
    Float getFloat() const {
      return this->operand;
    }
    String getString() const {
      throw std::runtime_error("Attempt to read string value of AST node with floating-point value");
    }
    Operator getOperator() const {
      throw std::runtime_error("Attempt to read operator value of AST node with floating-point value");
    }
    Float operand;
  };

  struct NodeOperandString {
    using Type = const String&;
    explicit NodeOperandString(Type operand) : operand(operand) {
    }
    INode::Operand getOperand() const {
      return INode::Operand::String;
    }
    Int getInt() const {
      throw std::runtime_error("Attempt to read integer value of AST node with string value");
    }
    Float getFloat() const {
      throw std::runtime_error("Attempt to read floating-point value of AST node with string value");
    }
    String getString() const {
      return this->operand;
    }
    Operator getOperator() const {
      throw std::runtime_error("Attempt to read operator value of AST node with string value");
    }
    String operand;
  };

  struct NodeOperandOperator {
    using Type = Operator;
    explicit NodeOperandOperator(Type operand) : operand(operand) {
    }
    INode::Operand getOperand() const {
      return INode::Operand::Operator;
    }
    Int getInt() const {
      throw std::runtime_error("Attempt to read integer value of AST node with operator value");
    }
    Float getFloat() const {
      throw std::runtime_error("Attempt to read floating-point value of AST node with operator value");
    }
    String getString() const {
      throw std::runtime_error("Attempt to read string value of AST node with operator value");
    }
    Operator getOperator() const {
      return this->operand;
    }
    Operator operand;
  };

  struct NodeLocationNone {
    NodeLocation* getLocation() const {
      return nullptr;
    }
  };

  struct NodeLocationFixed {
    NodeLocation* getLocation() {
      return &this->location;
    }
    NodeLocation location;
  };

  template<size_t N>
  struct NodeChildrenFixed {
    static constexpr size_t children = N;
  };

  struct NodeChildrenVariable {
    size_t children;
  };

  template<size_t N>
  struct NodeAttributesFixed {
    static constexpr size_t attributes = N;
  };

  struct NodeAttributesVariable {
    size_t attributes;
  };

  EGG_WARNING_SUPPRESS_INTELLISENSE_BEGIN();
  template<typename CHILDREN, typename ATTRIBUTES, typename OPERAND, typename LOCATION>
  struct NodeExtra final : public CHILDREN, public ATTRIBUTES, public OPERAND, public LOCATION {
    // cppcheck-suppress uninitMemberVar
    explicit NodeExtra(typename OPERAND::Type operand) : OPERAND(operand) {
    }
    ~NodeExtra() {
      auto n = this->children + this->attributes;
      for (size_t i = 0; i < n; ++i) {
        this->base[i]->hardRelease();
      }
    }
    INode* base[1];
  };
  EGG_WARNING_SUPPRESS_INTELLISENSE_END();

  template<typename CHILDREN, typename ATTRIBUTES, typename OPERAND, typename LOCATION>
  NodeContiguous<NodeExtra<CHILDREN, ATTRIBUTES, OPERAND, LOCATION>>* createNodeExtra(IAllocator& allocator, Opcode opcode, size_t slots, typename OPERAND::Type operand) {
    using Extra = NodeExtra<CHILDREN, ATTRIBUTES, OPERAND, LOCATION>;
    auto bytes = sizeof(Extra) + slots * sizeof(INode*) - sizeof(INode*);
    return allocator.create<NodeContiguous<Extra>>(bytes, allocator, opcode, operand);
  }

  template<typename OPERAND>
  Node createNodeOperand(IAllocator& allocator, Opcode opcode, typename OPERAND::Type operand) {
    return Node(createNodeExtra<NodeChildrenFixed<0>, NodeAttributesFixed<0>, OPERAND, NodeLocationNone>(allocator, opcode, 0, operand));
  }

  template<typename OPERAND, typename LOCATION>
  Node createNodeExtra(IAllocator& allocator, const NodeLocation* location, Opcode opcode, const Nodes* children, const Nodes* attributes, typename OPERAND::Type operand) {
    auto nchildren = (children == nullptr) ? size_t(0) : children->size();
    auto nattributes = (attributes == nullptr) ? size_t(0) : attributes->size();
    if (nattributes == 0) {
      // No attributes
      switch (nchildren) {
      case 0: {
        auto* node0 = createNodeExtra<NodeChildrenFixed<0>, NodeAttributesFixed<0>, OPERAND, LOCATION>(allocator, opcode, 0, operand);
        node0->initLocation(location);
        return Node(node0); }
      case 1: {
        auto* node1 = createNodeExtra<NodeChildrenFixed<1>, NodeAttributesFixed<0>, OPERAND, LOCATION>(allocator, opcode, 1, operand);
        node1->initChild(0, (*children)[0]);
        node1->initLocation(location);
        return Node(node1); }
      case 2: {
        auto* node2 = createNodeExtra<NodeChildrenFixed<2>, NodeAttributesFixed<0>, OPERAND, LOCATION>(allocator, opcode, 2, operand);
        node2->initChild(0, (*children)[0]);
        node2->initChild(1, (*children)[1]);
        node2->initLocation(location);
        return Node(node2); }
      case 3: {
        auto* node3 = createNodeExtra<NodeChildrenFixed<3>, NodeAttributesFixed<0>, OPERAND, LOCATION>(allocator, opcode, 3, operand);
        node3->initChild(0, (*children)[0]);
        node3->initChild(1, (*children)[1]);
        node3->initChild(2, (*children)[2]);
        node3->initLocation(location);
        return Node(node3); }
      }
      auto* node = createNodeExtra<NodeChildrenVariable, NodeAttributesFixed<0>, OPERAND, LOCATION>(allocator, opcode, nchildren, operand);
      node->initChildren(*children);
      node->initLocation(location);
      return Node(node);
    }
    if (nchildren == 0) {
      // No children but some attributes
      auto* node = createNodeExtra<NodeChildrenFixed<0>, NodeAttributesVariable, OPERAND, LOCATION>(allocator, opcode, nattributes, operand);
      node->initAttributes(*attributes);
      node->initLocation(location);
      return Node(node);
    }
    auto* node = createNodeExtra<NodeChildrenVariable, NodeAttributesVariable, OPERAND, LOCATION>(allocator, opcode, nchildren + nattributes, operand);
    node->initChildren(*children);
    node->initAttributes(*attributes);
    node->initLocation(location);
    return Node(node);
  }

#if !defined(NDEBUG)
  bool validateOpcode(Opcode opcode, const Nodes* children, bool has_operand) {
    auto args = (children == nullptr) ? size_t(0) : children->size();
    return OpcodeProperties::from(opcode).validate(args, has_operand);
  }
#endif

  void buildNodeString(StringBuilder& sb, const INode* node) {
    if (node == nullptr) {
      sb.add("<null>");
    } else {
      auto opcode = node->getOpcode();
      auto& props = OpcodeProperties::from(opcode);
      sb.add('(', props.name);
      // Attributes
      auto n = node->getAttributes();
      for (size_t i = 0; i < n; ++i) {
        sb.add(' ');
        buildNodeString(sb, &node->getAttribute(i));
      }
      // Operand
      switch (node->getOperand()) {
      case INode::Operand::Int:
        sb.add(' ', node->getInt());
        break;
      case INode::Operand::Float:
        sb.add(' ', node->getFloat());
        break;
      case INode::Operand::String:
        sb.add(' ', '"', node->getString(), '"');
        break;
      case INode::Operand::Operator:
        sb.add(' ', OperatorProperties::str(node->getOperator()));
        break;
      case INode::Operand::None:
        break;
      }
      // Children
      n = node->getChildren();
      for (size_t i = 0; i < n; ++i) {
        sb.add(' ');
        buildNodeString(sb, &node->getChild(i));
      }
      sb.add(')');
    }
  }

  Opcode simpleTypeToOpcode(ValueFlags flags) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
    switch (flags) {
    case ValueFlags::Void:
      return Opcode::VOID;
    case ValueFlags::Null:
      return Opcode::NULL_;
    case ValueFlags::Bool:
      return Opcode::BOOL;
    case ValueFlags::Int:
      return Opcode::INT;
    case ValueFlags::Float:
      return Opcode::FLOAT;
    case ValueFlags::String:
      return Opcode::STRING;
    case ValueFlags::Object:
      return Opcode::OBJECT;
    case ValueFlags::Any:
      return Opcode::ANY;
    case ValueFlags::AnyQ:
      return Opcode::ANYQ;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END();
    return Opcode::END;
  }
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode) {
  assert(OpcodeProperties::from(opcode).validate(0, false));
  return Node(createNodeExtra<NodeChildrenFixed<0>, NodeAttributesFixed<0>, NodeOperandNone, NodeLocationNone>(allocator, opcode, 0, nullptr));
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, Node&& child0) {
  assert(OpcodeProperties::from(opcode).validate(1, false));
  auto* node = createNodeExtra<NodeChildrenFixed<1>, NodeAttributesFixed<0>, NodeOperandNone, NodeLocationNone>(allocator, opcode, 1, nullptr);
  node->initChild(0, std::move(child0));
  return Node(node);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, Node&& child0, Node&& child1) {
  assert(OpcodeProperties::from(opcode).validate(2, false));
  auto* node = createNodeExtra<NodeChildrenFixed<2>, NodeAttributesFixed<0>, NodeOperandNone, NodeLocationNone>(allocator, opcode, 2, nullptr);
  node->initChild(0, child0);
  node->initChild(1, child1);
  return Node(node);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, Node&& child0, Node&& child1, Node&& child2) {
  assert(OpcodeProperties::from(opcode).validate(3, false));
  auto* node = createNodeExtra<NodeChildrenFixed<3>, NodeAttributesFixed<0>, NodeOperandNone, NodeLocationNone>(allocator, opcode, 3, nullptr);
  node->initChild(0, child0);
  node->initChild(1, child1);
  node->initChild(2, child2);
  return Node(node);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, Node&& child0, Node&& child1, Node&& child2, Node&& child3) {
  assert(OpcodeProperties::from(opcode).validate(4, false));
  auto* node = createNodeExtra<NodeChildrenFixed<4>, NodeAttributesFixed<0>, NodeOperandNone, NodeLocationNone>(allocator, opcode, 4, nullptr);
  node->initChild(0, child0);
  node->initChild(1, child1);
  node->initChild(2, child2);
  node->initChild(3, child3);
  return Node(node);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes& children) {
  assert(validateOpcode(opcode, &children, false));
  return createNodeExtra<NodeOperandNone, NodeLocationNone>(allocator, nullptr, opcode, &children, nullptr, nullptr);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes) {
  assert(validateOpcode(opcode, children, false));
  return createNodeExtra<NodeOperandNone, NodeLocationNone>(allocator, nullptr, opcode, children, attributes, nullptr);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Int value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandInt, NodeLocationNone>(allocator, nullptr, opcode, children, attributes, value);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Float value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandFloat, NodeLocationNone>(allocator, nullptr, opcode, children, attributes, value);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, const String& value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandString, NodeLocationNone>(allocator, nullptr, opcode, children, attributes, value);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Operator value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandOperator, NodeLocationNone>(allocator, nullptr, opcode, children, attributes, value);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes) {
  assert(validateOpcode(opcode, children, false));
  return createNodeExtra<NodeOperandNone, NodeLocationFixed>(allocator, &location, opcode, children, attributes, nullptr);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes, Int value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandInt, NodeLocationFixed>(allocator, &location, opcode, children, attributes, value);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes, Float value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandFloat, NodeLocationFixed>(allocator, &location, opcode, children, attributes, value);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes, const String& value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandString, NodeLocationFixed>(allocator, &location, opcode, children, attributes, value);
}

egg::ovum::Node egg::ovum::NodeFactory::create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes, Operator value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandOperator, NodeLocationFixed>(allocator, &location, opcode, children, attributes, value);
}

egg::ovum::Node egg::ovum::NodeFactory::createValue(IAllocator& allocator, std::nullptr_t) {
  return createNodeOperand<NodeOperandNone>(allocator, Opcode::NULL_, nullptr);
}

egg::ovum::Node egg::ovum::NodeFactory::createValue(IAllocator& allocator, bool value) {
  return createNodeOperand<NodeOperandNone>(allocator, value ? Opcode::TRUE : Opcode::FALSE, nullptr);
}

egg::ovum::Node egg::ovum::NodeFactory::createValue(IAllocator& allocator, int32_t value) {
  return createNodeOperand<NodeOperandInt>(allocator, Opcode::IVALUE, value);
}

egg::ovum::Node egg::ovum::NodeFactory::createValue(IAllocator& allocator, int64_t value) {
  return createNodeOperand<NodeOperandInt>(allocator, Opcode::IVALUE, value);
}

egg::ovum::Node egg::ovum::NodeFactory::createValue(IAllocator& allocator, float value) {
  return createNodeOperand<NodeOperandFloat>(allocator, Opcode::FVALUE, value);
}

egg::ovum::Node egg::ovum::NodeFactory::createValue(IAllocator& allocator, double value) {
  return createNodeOperand<NodeOperandFloat>(allocator, Opcode::FVALUE, value);
}

egg::ovum::Node egg::ovum::NodeFactory::createValue(IAllocator& allocator, const String& value) {
  return createNodeOperand<NodeOperandString>(allocator, Opcode::SVALUE, value);
}

egg::ovum::Node egg::ovum::NodeFactory::createSimpleType(IAllocator& allocator, const NodeLocation& location, ValueFlags flags) {
  auto opcode = simpleTypeToOpcode(flags);
  if (opcode != Opcode::END) {
    // This is a well-known simple type
    return NodeFactory::create(allocator, opcode);
  }
  Nodes parts;
  while (flags != ValueFlags::None) {
    // Construct a vector of all the constituent parts
    auto top = Bits::topmost(flags);
    opcode = simpleTypeToOpcode(top);
    assert(opcode != Opcode::END);
    parts.push_back(NodeFactory::create(allocator, opcode));
    flags = Bits::clear(flags, top);
  }
  return NodeFactory::create(allocator, location, Opcode::UNION, &parts);
}

egg::ovum::String egg::ovum::Node::toString(const INode* node) {
  StringBuilder sb;
  buildNodeString(sb, node);
  return sb.str();
}
