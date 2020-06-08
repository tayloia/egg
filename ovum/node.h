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
#define EGG_VM_OPERATORS_ENUM(oper, opclass, index, text) oper = opclass * EGG_VM_OCSTEP + index,
    EGG_VM_OPERATORS(EGG_VM_OPERATORS_ENUM)
#undef EGG_VM_OPERATORS_ENUM
  };

  struct NodeLocation {
    size_t line;
    size_t column;
  };

  class INode : public IHardAcquireRelease {
  public:
    enum class Operand { None, Int, Float, String, Operator };
    virtual Opcode getOpcode() const = 0;
    virtual Operand getOperand() const = 0;
    virtual size_t getChildren() const = 0;
    virtual INode& getChild(size_t index) const = 0;
    virtual Int getInt() const = 0;
    virtual Float getFloat() const = 0;
    virtual String getString() const = 0;
    virtual Operator getOperator() const = 0;
    virtual size_t getAttributes() const = 0;
    virtual INode& getAttribute(size_t index) const = 0;
    virtual const NodeLocation* getLocation() const = 0;
    virtual void setChild(size_t index, INode& value) = 0;
  };

  class Node : public HardPtr<INode> {
  public:
    Node(std::nullptr_t = nullptr) {} // implicit
    explicit Node(const INode* node) : HardPtr(node) {}
    String toString() const {
      return Node::toString(this->get());
    }

    // Helpers
    static String toString(const INode* node);
  };

  using Nodes = std::vector<Node>;

  class NodeFactory {
  public:
    // Without location
    static Node create(IAllocator& allocator, Opcode opcode);
    static Node create(IAllocator& allocator, Opcode opcode, Node&& child0);
    static Node create(IAllocator& allocator, Opcode opcode, Node&& child0, Node&& child1);
    static Node create(IAllocator& allocator, Opcode opcode, Node&& child0, Node&& child1, Node&& child2);
    static Node create(IAllocator& allocator, Opcode opcode, Node&& child0, Node&& child1, Node&& child2, Node&& child3);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes& children);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes = nullptr);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Int operand);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Float operand);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, const String& operand);
    static Node create(IAllocator& allocator, Opcode opcode, const Nodes* children, const Nodes* attributes, Operator operand);
    // With location
    static Node create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes = nullptr);
    static Node create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes, Int operand);
    static Node create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes, Float operand);
    static Node create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes, const String& operand);
    static Node create(IAllocator& allocator, const NodeLocation& location, Opcode opcode, const Nodes* children, const Nodes* attributes, Operator operand);
    // Values
    static Node createValue(IAllocator& allocator, std::nullptr_t);
    static Node createValue(IAllocator& allocator, bool value);
    static Node createValue(IAllocator& allocator, int32_t value);
    static Node createValue(IAllocator& allocator, int64_t value);
    static Node createValue(IAllocator& allocator, float value);
    static Node createValue(IAllocator& allocator, double value);
    static Node createValue(IAllocator& allocator, const String& value);
    // Types
    static Node createBasalType(IAllocator& allocator, const NodeLocation& location, BasalBits basal);
    static Node createPointerType(IAllocator& allocator, const NodeLocation& location, const Type& pointee);
    static Node createFunctionType(IAllocator& allocator, const NodeLocation& location, const IFunctionSignature& signature);
  };

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
    static const OpcodeProperties& from(Opcode opcode);
    static std::string str(Opcode opcode); // never null
  };

  struct OperatorProperties {
    const char* name;
    Opclass opclass;
    size_t index; // within opclass
    size_t operands;
    bool validate(size_t args) const {
      return (this->name != nullptr) && (args == this->operands);
    }
    static const OperatorProperties& from(Operator oper);
    static std::string str(Operator oper); // never null
  };
}
