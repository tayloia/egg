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

  class NodeBase : public HardReferenceCounted<INode> {
    NodeBase(const NodeBase&) = delete;
    NodeBase& operator=(const NodeBase&) = delete;
  private:
    Opcode opcode;
  public:
    std::vector<Node> children;
    std::vector<Node> attributes;
  public:
    NodeBase(IAllocator& allocator, Opcode opcode)
      : HardReferenceCounted(allocator),
        opcode(opcode) {
    }
    virtual Opcode getOpcode() const override {
      return this->opcode;
    }
    virtual size_t getChildren() const override {
      return this->children.size();
    }
    virtual INode& getChild(size_t index) const override {
      if (index >= this->children.size()) {
        throw std::out_of_range("Invalid AST node child index");
      }
      return *this->children[index];
    }
    virtual Int getInt() const override {
      throw std::runtime_error("Attempt to read non-existent integer value of AST node");
    }
    virtual Float getFloat() const override {
      throw std::runtime_error("Attempt to read non-existent floating-point value of AST node");
    }
    virtual String getString() const override {
      throw std::runtime_error("Attempt to read non-existent string value of AST node");
    }
    virtual size_t getAttributes() const override {
      return this->attributes.size();
    }
    virtual INode& getAttribute(size_t index) const override {
      if (index >= this->attributes.size()) {
        throw std::out_of_range("Invalid AST node attribute index");
      }
      return *this->attributes[index];
    }
    virtual void setChild(size_t index, INode& value) override {
      if (index >= this->children.size()) {
        throw std::out_of_range("Invalid AST node child index");
      }
      this->children[index].set(&value);
    }
    static NodeBase* create(IAllocator& allocator, Opcode opcode) {
      return allocator.create<NodeBase>(0, allocator, opcode);
    }
  private:
    virtual INode** base() const {
      return reinterpret_cast<INode**>(const_cast<NodeBase*>(this) + 1);
    }
  };

  class NodeWithInt : public NodeBase {
    NodeWithInt(const NodeWithInt&) = delete;
    NodeWithInt& operator=(const NodeWithInt&) = delete;
  private:
    Int operand;
  public:
    NodeWithInt(IAllocator& allocator, Opcode opcode, Int operand)
      : NodeBase(allocator, opcode),
        operand(operand) {
    }
    virtual Int getInt() const override {
      return this->operand;
    }
  private:
    virtual INode** base() const override {
      return reinterpret_cast<INode**>(const_cast<NodeWithInt*>(this) + 1);
    }
  };

  class NodeWithFloat : public NodeBase {
    NodeWithFloat(const NodeWithFloat&) = delete;
    NodeWithFloat& operator=(const NodeWithFloat&) = delete;
  private:
    Float operand;
  public:
    NodeWithFloat(IAllocator& allocator, Opcode opcode, Float operand)
      : NodeBase(allocator, opcode),
      operand(operand) {
    }
    virtual Float getFloat() const override {
      return this->operand;
    }
  private:
    virtual INode** base() const override {
      return reinterpret_cast<INode**>(const_cast<NodeWithFloat*>(this) + 1);
    }
  };

  class NodeWithString : public NodeBase {
    NodeWithString(const NodeWithString&) = delete;
    NodeWithString& operator=(const NodeWithString&) = delete;
  private:
    String operand;
  public:
    NodeWithString(IAllocator& allocator, Opcode opcode, String operand)
      : NodeBase(allocator, opcode),
      operand(operand) {
    }
    virtual String getString() const override {
      return this->operand;
    }
  private:
    virtual INode** base() const override {
      return reinterpret_cast<INode**>(const_cast<NodeWithString*>(this) + 1);
    }
  };

  template<typename T, typename... ARGS>
  Node createWithoutAttributes(IAllocator& allocator, Opcode opcode, const std::vector<Node>& children, ARGS&&... args) {
    // Use perfect forwarding to the constructor
    auto* node = allocator.create<T>(0, allocator, opcode, std::forward<ARGS>(args)...);
    node->children = children;
    return Node(node);
  }

  template<typename T, typename... ARGS>
  Node createWithAttributes(IAllocator& allocator, Opcode opcode, const std::vector<Node>& children, const std::vector<Node>& attributes, ARGS&&... args) {
    // Use perfect forwarding to the constructor
    auto* node = allocator.create<T>(0, allocator, opcode, std::forward<ARGS>(args)...);
    node->children = children;
    node->attributes = attributes;
    return Node(node);
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

egg::ovum::ast::Opcode egg::ovum::ast::opcodeFromMachineByte(uint8_t byte) {
  return Table::instance.opcode[byte];
}

const egg::ovum::ast::OpcodeProperties& egg::ovum::ast::opcodeProperties(Opcode opcode) {
  assert((opcode >= 1) && (opcode <= 255));
  return Table::instance.properties[opcode];
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode) {
  assert(opcodeProperties(opcode).validate(0, false));
  return Node(NodeBase::create(allocator, opcode));
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, INode& child0) {
  assert(opcodeProperties(opcode).validate(1, false));
  auto* node = NodeBase::create(allocator, opcode);
  node->children.emplace_back(&child0);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, INode& child0, INode& child1) {
  assert(opcodeProperties(opcode).validate(2, false));
  auto* node = NodeBase::create(allocator, opcode);
  node->children.emplace_back(&child0);
  node->children.emplace_back(&child1);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, INode& child0, INode& child1, INode& child2) {
  assert(opcodeProperties(opcode).validate(3, false));
  auto* node = NodeBase::create(allocator, opcode);
  node->children.emplace_back(&child0);
  node->children.emplace_back(&child1);
  node->children.emplace_back(&child2);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, INode& child0, INode& child1, INode& child2, INode& child3) {
  assert(opcodeProperties(opcode).validate(4, false));
  auto* node = NodeBase::create(allocator, opcode);
  node->children.emplace_back(&child0);
  node->children.emplace_back(&child1);
  node->children.emplace_back(&child2);
  node->children.emplace_back(&child3);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, const std::vector<Node>& children) {
  assert(opcodeProperties(opcode).validate(children.size(), false));
  return createWithoutAttributes<NodeBase>(allocator, opcode, children);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, const std::vector<Node>& children, const std::vector<Node>& attributes) {
  assert(opcodeProperties(opcode).validate(children.size(), false));
  return createWithAttributes<NodeBase>(allocator, opcode, children, attributes);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, const std::vector<Node>& children, const std::vector<Node>& attributes, Int value) {
  assert(opcodeProperties(opcode).validate(children.size(), true));
  return createWithAttributes<NodeWithInt>(allocator, opcode, children, attributes, value);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, const std::vector<Node>& children, const std::vector<Node>& attributes, Float value) {
  assert(opcodeProperties(opcode).validate(children.size(), true));
  return createWithAttributes<NodeWithFloat>(allocator, opcode, children, attributes, value);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, const std::vector<Node>& children, const std::vector<Node>& attributes, String value) {
  assert(opcodeProperties(opcode).validate(children.size(), true));
  return createWithAttributes<NodeWithString>(allocator, opcode, children, attributes, value);
}
