namespace egg::ovum {
  enum Opcode {
#define EGG_VM_OPCODES_ENUM(opcode, minbyte, minargs, maxargs, text) opcode = minbyte,
    EGG_VM_OPCODES(EGG_VM_OPCODES_ENUM)
#undef EGG_VM_OPCODES_ENUM
    OPCODE_reserved = -1
  };

  enum Opclass {
#define EGG_VM_OPCLASSES_ENUM(opclass, value, text) opclass = value,
    EGG_VM_OPCLASSES(EGG_VM_OPCLASSES_ENUM)
#undef EGG_VM_OPCLASSES_ENUM
  };

  enum Operator {
#define EGG_VM_OPERATORS_ENUM(oper, opclass, unique, text) oper = opclass * EGG_VM_OSTEP + unique,
    EGG_VM_OPERATORS(EGG_VM_OPERATORS_ENUM)
#undef EGG_VM_OPERATORS_ENUM
  };

  class INode : public IHardAcquireRelease {
  public:
    enum class Operand { None, Int, Float, String };
    virtual Opcode getOpcode() const = 0;
    virtual Operand getOperand() const = 0;
    virtual size_t getChildren() const = 0;
    virtual INode& getChild(size_t index) const = 0;
    virtual Int getInt() const = 0;
    virtual Float getFloat() const = 0;
    virtual String getString() const = 0;
    virtual size_t getAttributes() const = 0;
    virtual INode& getAttribute(size_t index) const = 0;
    virtual void setChild(size_t index, INode& value) = 0;
  };
  using Node = HardPtr<INode>;
  using Nodes = std::vector<Node>;

  class NodeFactory {
  public:
    static Node create(IAllocator& allocator, Opcode opcode);
    static Node create(IAllocator& allocator, Opcode opcode, const Node& child0); // WIBBLE move
    static Node create(IAllocator& allocator, Opcode opcode, const Node& child0, const Node& child1);
    static Node create(IAllocator& allocator, Opcode opcode, const Node& child0, const Node& child1, const Node& child2);
    static Node create(IAllocator& allocator, Opcode opcode, const Node& child0, const Node& child1, const Node& child2, const Node& child3);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes& children);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes = nullptr);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Int operand);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Float operand);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, const String& operand);
    static Node createValue(IAllocator& allocator, nullptr_t);
    static Node createValue(IAllocator& allocator, bool value);
    static Node createValue(IAllocator& allocator, int32_t value);
    static Node createValue(IAllocator& allocator, int64_t value);
    static Node createValue(IAllocator& allocator, float value);
    static Node createValue(IAllocator& allocator, double value);
    static Node createValue(IAllocator& allocator, const String& value);
  };
  inline size_t childrenFromMachineByte(uint8_t byte) {
    // Compute the number of children from a VM code byte
    auto following = byte % (EGG_VM_NARGS + 1);
    return (following < EGG_VM_NARGS) ? following : SIZE_MAX;
  }

  Opcode opcodeFromMachineByte(uint8_t byte);

  struct OpcodeProperties {
    const char* name;
    size_t minargs;
    size_t maxargs;
    uint8_t minbyte;
    uint8_t maxbyte;
    bool operand;
    uint8_t encode(size_t args) const {
      // Returns zero if this is a bad encoding
      if ((args < this->minargs) || (args > this->maxargs)) {
        return 0;
      }
      if (args > EGG_VM_NARGS) {
        args = EGG_VM_NARGS;
      }
      auto result = this->minbyte + args - this->minargs;
      assert((result > 0) && (result <= 0xFF));
      return uint8_t(result);
    }
    bool validate(size_t args, bool has_operand) const {
      return (this->name != nullptr) && (args >= this->minargs) && (args <= this->maxargs) && (has_operand == this->operand);
    }
  };
  const OpcodeProperties& opcodeProperties(Opcode opcode);
}
