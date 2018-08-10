namespace egg::ovum::ast {
  enum Opcode {
#define EGG_VM_OPCODES_ENUM(opcode, minbyte, minargs, maxargs, text) opcode = minbyte,
    EGG_VM_OPCODES(EGG_VM_OPCODES_ENUM)
#undef EGG_VM_OPCODES_ENUM
    OPCODE_reserved = -1
  };

  class INode : public IHardAcquireRelease {
  public:
    virtual ~INode() {}
    virtual Opcode getOpcode() const = 0;
    virtual INode& getChild(size_t index) const = 0;
    virtual size_t getChildren() const = 0;
    virtual Int getInt() const = 0;
    virtual Float getFloat() const = 0;
    virtual String getString() const = 0;
    virtual void setChild(size_t index, const INode& value) = 0;
  };
  using Node = HardPtr<INode>;

  class NodeFactory {
  public:
    static Node create(IAllocator& allocator, Opcode opcode);
    static Node create(IAllocator& allocator, Opcode opcode, INode& child0);
    static Node create(IAllocator& allocator, Opcode opcode, INode& child0, INode& child1);
    static Node create(IAllocator& allocator, Opcode opcode, INode& child0, INode& child1, INode& child2);
    static Node create(IAllocator& allocator, Opcode opcode, INode& child0, INode& child1, INode& child2, INode& child3);
    static Node create(IAllocator& allocator, Opcode opcode, const std::vector<Node>& children);
  };

  inline size_t childrenFromMachineByte(uint8_t byte) {
    // Compute the number of children from a VM code byte
    auto following = byte % (EGG_VM_NARGS + 1);
    return (following < EGG_VM_NARGS) ? following : SIZE_MAX;
  }

  Opcode opcodeFromMachineByte(uint8_t byte);

  struct OpcodeProperties {
    std::string name;
    size_t minargs;
    size_t maxargs;
    uint8_t minbyte;
    uint8_t maxbyte;
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
  };
  const OpcodeProperties& opcodeProperties(Opcode opcode);
}
