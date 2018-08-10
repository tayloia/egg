#include "ovum/ovum.h"
#include "ovum/ast.h"

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
      auto index = size_t(code);
      while (minargs++ <= maxargs) {
        assert(index <= 0xFF);
        assert(this->opcode[index] == OPCODE_reserved);
        this->opcode[index++] = code;
      }
    }
  };
  const Table Table::instance{};

  class NodeFixed : public HardReferenceCounted<INode> {
    NodeFixed(const NodeFixed&) = delete;
    NodeFixed& operator=(const NodeFixed&) = delete;
  private:
    Opcode opcode;
    size_t count;
  public:
    explicit NodeFixed(IAllocator& allocator, Opcode opcode, size_t count)
      : HardReferenceCounted(allocator), opcode(opcode), count(count) {
      // Zero out the child pointers even though we're going to overwrite them
      std::memset(this->base(), 0, sizeof(INode*) * count);
    }
    void initChild(size_t index, const INode& value) {
      // Only used by the factory methods below
      assert(index < this->count);
      auto& slot = this->base()[index];
      assert(slot == nullptr);
      slot = HardPtr<INode>::hardAcquire(&value);
      assert(slot != nullptr);
    }
    virtual ~NodeFixed() {
      auto** ptr = this->base();
      for (size_t i = 0; i < this->count; ++i) {
        if (*ptr != nullptr) {
          (*ptr)->hardRelease();
        }
        ptr++;
      }
    }
    virtual Opcode getOpcode() const override {
      return this->opcode;
    }
    virtual INode& getChild(size_t index) const override {
      if (index >= this->count) {
        throw std::out_of_range("Invalid AST node child index");
      }
      return *(this->base()[index]);
    }
    virtual size_t getChildren() const override {
      return this->count;
    }
    virtual Int getInt() const override {
      throw std::runtime_error("Attempt to read integer value of non-value AST node");
    }
    virtual Float getFloat() const override {
      throw std::runtime_error("Attempt to read floating-point value of non-value AST node");
    }
    virtual String getString() const override {
      throw std::runtime_error("Attempt to read string value of non-value AST node");
    }
    virtual void setChild(size_t index, const INode& value) override {
      if (index >= this->count) {
        throw std::out_of_range("Invalid AST node child index");
      }
      auto* acquired = HardPtr<INode>::hardAcquire(&value);
      assert(acquired != nullptr);
      auto& slot = this->base()[index];
      assert(slot != nullptr);
      slot->hardRelease();
      slot = acquired;
    }
  private:
    INode** base() const {
      return reinterpret_cast<INode**>(const_cast<NodeFixed*>(this) + 1);
    }
  };

  NodeFixed* createFixed(IAllocator& allocator, Opcode opcode, size_t count) {
    return allocator.create<NodeFixed>(sizeof(INode*) * count, allocator, opcode, count);
  }
}

egg::ovum::ast::Opcode egg::ovum::ast::opcodeFromMachineByte(uint8_t byte) {
  return Table::instance.opcode[byte];
}

const egg::ovum::ast::OpcodeProperties& egg::ovum::ast::opcodeProperties(Opcode opcode) {
  assert((opcode >= 1) && (opcode <= 255));
  return Table::instance.properties[opcode];
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode) {
  return Node(createFixed(allocator, opcode, 0));
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, INode& child0) {
  auto* node = createFixed(allocator, opcode, 1);
  node->initChild(0, child0);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, INode& child0, INode& child1) {
  auto* node = createFixed(allocator, opcode, 2);
  node->initChild(0, child0);
  node->initChild(1, child1);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, INode& child0, INode& child1, INode& child2) {
  auto* node = createFixed(allocator, opcode, 3);
  node->initChild(0, child0);
  node->initChild(1, child1);
  node->initChild(2, child2);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, INode& child0, INode& child1, INode& child2, INode& child3) {
  auto* node = createFixed(allocator, opcode, 4);
  node->initChild(0, child0);
  node->initChild(1, child1);
  node->initChild(2, child2);
  node->initChild(3, child3);
  return Node(node);
}

egg::ovum::ast::Node egg::ovum::ast::NodeFactory::create(IAllocator& allocator, ast::Opcode opcode, const std::vector<Node>& children) {
  auto* node = createFixed(allocator, opcode, children.size());
  size_t index = 0;
  for (auto& i : children) {
    node->initChild(index++, *i);
  }
  return Node(node);
}
