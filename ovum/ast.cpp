#include "ovum/ovum.h"
#include "ovum/ast.h"

#include <cmath>

namespace {
  using namespace egg::ovum;
  using namespace egg::ovum::ast;

  struct Table {
    static const Table instance;
    Opcode opcode[256];
    OpcodeProperties properties[256];
  private:
    Table() {
      std::memset(this->opcode, -1, sizeof(this->opcode));
      std::memset(this->properties, 0, sizeof(this->properties));
      const size_t N = EGG_VM_NARGS; // used in the macro below
#define EGG_VM_OPCODES_TABLE(opcode, minbyte, minargs, maxargs, text) this->fill(opcode, minargs, maxargs, text);
      EGG_VM_OPCODES(EGG_VM_OPCODES_TABLE)
#undef EGG_VM_OPCODES_TABLE
    }
    void fill(Opcode code, size_t minargs, size_t maxargs, const char* text) {
      assert(code != OPCODE_reserved);
      assert(text != nullptr);
      assert(minargs <= maxargs);
      assert(maxargs <= EGG_VM_NARGS);
      assert((code >= 0x00) && (code <= 0xFF));
      auto& prop = this->properties[code];
      assert(prop.minbyte == 0);
      prop.name = text;
      prop.minargs = minargs;
      prop.maxargs = (maxargs < EGG_VM_NARGS) ? maxargs : SIZE_MAX;
      prop.minbyte = uint8_t(code);
      prop.maxbyte = uint8_t(code + maxargs - minargs);
      prop.operand = (code < EGG_VM_ISTART);
      auto index = size_t(code);
      while (minargs++ <= maxargs) {
        assert(index <= 0xFF);
        assert(this->opcode[index] == OPCODE_reserved);
        this->opcode[index++] = code;
      }
    }
  };
  const Table Table::instance{};

  template<typename EXTRA>
  class NodeContiguous final : public HardReferenceCounted<INode> {
    NodeContiguous(const NodeContiguous&) = delete;
    NodeContiguous& operator=(const NodeContiguous&) = delete;
  private:
    Opcode opcode;
  public:
    NodeContiguous(IAllocator& allocator, Opcode opcode, typename EXTRA::Type operand)
      : HardReferenceCounted(allocator),
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
    virtual size_t getAttributes() const override {
      return this->extra()->attributes;
    }
    virtual INode& getAttribute(size_t index) const override {
      if (index >= this->extra()->attributes) {
        throw std::out_of_range("Invalid AST node attribute index");
      }
      return *(this->extra()->base)[index + this->extra()->children];
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
    EXTRA* extra() const {
      return reinterpret_cast<EXTRA*>(const_cast<NodeContiguous*>(this) + 1);
    }
  };

  struct NodeOperandNone {
    using Type = nullptr_t;
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
    String operand;
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

  template<typename CHILDREN, typename ATTRIBUTES, typename OPERAND>
  struct NodeExtra final : public CHILDREN, public ATTRIBUTES, public OPERAND {
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

  template<typename CHILDREN, typename ATTRIBUTES, typename OPERAND>
  NodeContiguous<NodeExtra<CHILDREN, ATTRIBUTES, OPERAND>>* createNodeExtra(IAllocator& allocator, Opcode opcode, size_t slots, typename OPERAND::Type operand) {
    using Extra = NodeExtra<CHILDREN, ATTRIBUTES, OPERAND>;
    auto bytes = sizeof(Extra) + slots * sizeof(INode*) - sizeof(INode*);
    return allocator.create<NodeContiguous<Extra>>(bytes, allocator, opcode, operand);
  }

  template<typename OPERAND>
  Node createNodeExtra(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, typename OPERAND::Type operand) {
    auto nchildren = (children == nullptr) ? size_t(0) : children->size();
    auto nattributes = (attributes == nullptr) ? size_t(0) : attributes->size();
    if (nattributes == 0) {
      // No attributes
      switch (nchildren) {
      case 0:
        return Node(createNodeExtra<NodeChildrenFixed<0>, NodeAttributesFixed<0>, OPERAND>(allocator, opcode, 0, operand));
      case 1: {
        auto* node1 = createNodeExtra<NodeChildrenFixed<1>, NodeAttributesFixed<0>, OPERAND>(allocator, opcode, 1, operand);
        node1->initChild(0, (*children)[0]);
        return Node(node1); }
      case 2: {
        auto* node2 = createNodeExtra<NodeChildrenFixed<2>, NodeAttributesFixed<0>, OPERAND>(allocator, opcode, 2, operand);
        node2->initChild(0, (*children)[0]);
        node2->initChild(1, (*children)[1]);
        return Node(node2); }
      case 3: {
        auto* node3 = createNodeExtra<NodeChildrenFixed<3>, NodeAttributesFixed<0>, OPERAND>(allocator, opcode, 3, operand);
        node3->initChild(0, (*children)[0]);
        node3->initChild(1, (*children)[1]);
        node3->initChild(2, (*children)[2]);
        return Node(node3); }
      }
      auto* node = createNodeExtra<NodeChildrenVariable, NodeAttributesFixed<0>, OPERAND>(allocator, opcode, nchildren, operand);
      node->initChildren(*children);
      return Node(node);
    }
    if (nchildren == 0) {
      // No children but some attributes
      auto* node = createNodeExtra<NodeChildrenFixed<0>, NodeAttributesVariable, OPERAND>(allocator, opcode, nattributes, operand);
      node->initAttributes(*attributes);
      return Node(node);
    }
    auto* node = createNodeExtra<NodeChildrenVariable, NodeAttributesVariable, OPERAND>(allocator, opcode, nchildren + nattributes, operand);
    node->initChildren(*children);
    node->initAttributes(*attributes);
    return Node(node);
  }

  bool validateOpcode(Opcode opcode, const Nodes* children, bool has_operand) {
    auto args = (children == nullptr) ? size_t(0) : children->size();
    return opcodeProperties(opcode).validate(args, has_operand);
  }
}

void egg::ovum::ast::MantissaExponent::fromFloat(Float f) {
  switch (std::fpclassify(f)) {
  case FP_NORMAL:
    // Finite
    break;
  case FP_ZERO:
    // Zero
    this->mantissa = 0;
    this->exponent = 0;
    return;
  case FP_INFINITE:
    // Positive or negative infinity
    this->mantissa = 0;
    this->exponent = std::signbit(f) ? ExponentNegativeInfinity : ExponentPositiveInfinity;
    return;
  case FP_NAN:
    // Not-a-Number
    this->mantissa = 0;
    this->exponent = ExponentNaN;
    return;
  }
  constexpr int bitsInMantissa = std::numeric_limits<Float>::digits; // DBL_MANT_DIG
  constexpr double scale = 1ull << bitsInMantissa;
  int e;
  double m = std::frexp(f, &e);
  m = std::floor(scale * m);
  this->mantissa = std::llround(m);
  if (this->mantissa == 0) {
    // Failed to convert
    this->exponent = ExponentNaN;
    return;
  }
  this->exponent = e - bitsInMantissa;
  while ((this->mantissa & 1) == 0) {
    // Reduce the mantissa
    this->mantissa >>= 1;
    this->exponent++;
  }
}

egg::ovum::Float egg::ovum::ast::MantissaExponent::toFloat() const {
  if (this->mantissa == 0) {
    switch (this->exponent) {
    case 0:
      return 0.0;
    case ExponentPositiveInfinity:
      return std::numeric_limits<Float>::infinity();
    case ExponentNegativeInfinity:
      return -std::numeric_limits<Float>::infinity();
    case ExponentNaN:
    default:
      return std::numeric_limits<Float>::quiet_NaN();
    }
  }
  return std::ldexp(this->mantissa, int(this->exponent));
}

egg::ovum::Opcode egg::ovum::ast::opcodeFromMachineByte(uint8_t byte) {
  return Table::instance.opcode[byte];
}

const egg::ovum::ast::OpcodeProperties& egg::ovum::ast::opcodeProperties(Opcode opcode) {
  assert((opcode >= 1) && (opcode <= 255));
  return Table::instance.properties[opcode];
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode) {
  assert(opcodeProperties(opcode).validate(0, false));
  return Node(createNodeExtra<NodeChildrenFixed<0>, NodeAttributesFixed<0>, NodeOperandNone>(allocator, opcode, 0, nullptr));
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Node& child0) {
  assert(opcodeProperties(opcode).validate(1, false));
  auto* node = createNodeExtra<NodeChildrenFixed<1>, NodeAttributesFixed<0>, NodeOperandNone>(allocator, opcode, 1, nullptr);
  node->initChild(0, child0);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Node& child0, const Node& child1) {
  assert(opcodeProperties(opcode).validate(2, false));
  auto* node = createNodeExtra<NodeChildrenFixed<2>, NodeAttributesFixed<0>, NodeOperandNone>(allocator, opcode, 2, nullptr);
  node->initChild(0, child0);
  node->initChild(1, child1);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Node& child0, const Node& child1, const Node& child2) {
  assert(opcodeProperties(opcode).validate(3, false));
  auto* node = createNodeExtra<NodeChildrenFixed<3>, NodeAttributesFixed<0>, NodeOperandNone>(allocator, opcode, 3, nullptr);
  node->initChild(0, child0);
  node->initChild(1, child1);
  node->initChild(2, child2);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Node& child0, const Node& child1, const Node& child2, const Node& child3) {
  assert(opcodeProperties(opcode).validate(4, false));
  auto* node = createNodeExtra<NodeChildrenFixed<4>, NodeAttributesFixed<0>, NodeOperandNone>(allocator, opcode, 4, nullptr);
  node->initChild(0, child0);
  node->initChild(1, child1);
  node->initChild(2, child2);
  node->initChild(3, child3);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes& children) {
  assert(validateOpcode(opcode, &children, false));
  return createNodeExtra<NodeOperandNone>(allocator, opcode, &children, nullptr, nullptr);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes) {
  assert(validateOpcode(opcode, children, false));
  return createNodeExtra<NodeOperandNone>(allocator, opcode, children, attributes, nullptr);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Int value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandInt>(allocator, opcode, children, attributes, value);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Float value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandFloat>(allocator, opcode, children, attributes, value);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, const String& value) {
  assert(validateOpcode(opcode, children, true));
  return createNodeExtra<NodeOperandString>(allocator, opcode, children, attributes, value);
}
