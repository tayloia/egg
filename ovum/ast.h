namespace egg::ovum::ast {
  // Helper for converting IEEE to/from mantissa/exponents
  struct MantissaExponent {
    static constexpr int64_t ExponentNaN = 1;
    static constexpr int64_t ExponentPositiveInfinity = 2;
    static constexpr int64_t ExponentNegativeInfinity = -2;
    int64_t mantissa;
    int64_t exponent;
    void fromFloat(Float f);
    Float toFloat() const;
  };

  class INode : public IHardAcquireRelease {
  public:
    enum class Operand { None, Int, Float, String };
    virtual ~INode() {}
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
    static Node create(IAllocator& allocator, Opcode opcode, INode& child0);
    static Node create(IAllocator& allocator, Opcode opcode, INode& child0, INode& child1);
    static Node create(IAllocator& allocator, Opcode opcode, INode& child0, INode& child1, INode& child2);
    static Node create(IAllocator& allocator, Opcode opcode, INode& child0, INode& child1, INode& child2, INode& child3);
    static Node create(IAllocator& allocator, Opcode opcode, Nodes&& children);
    static Node create(IAllocator& allocator, Opcode opcode, Nodes&& children, Nodes&& attributes);
    static Node create(IAllocator& allocator, Opcode opcode, Nodes&& children, Nodes&& attributes, Int value);
    static Node create(IAllocator& allocator, Opcode opcode, Nodes&& children, Nodes&& attributes, Float value);
    static Node create(IAllocator& allocator, Opcode opcode, Nodes&& children, Nodes&& attributes, String value);
    static void writeModuleToBinaryStream(std::istream& stream, const Node& module);
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

  class ModuleBuilder {
    ModuleBuilder(const ModuleBuilder&) = delete;
    ModuleBuilder& operator=(const ModuleBuilder&) = delete;
  public:
    IAllocator& allocator;
    Nodes attributes;
  public:
    explicit ModuleBuilder(IAllocator& allocator);
    Node createModule(Node&& block);
    Node createBlock(Nodes&& statements);
    Node createNoop();
    Node createValueArray(Nodes&& elements);
    Node createValueInt(Int value);
    Node createValueFloat(Float value);
    Node createValueString(String value);
    Node createNode(Opcode opcode, Nodes&& children);
  };
}
