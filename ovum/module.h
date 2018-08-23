namespace egg::ovum {
  enum Section {
#define EGG_VM_SECTIONS_ENUM(section, value) section = value,
    EGG_VM_SECTIONS(EGG_VM_SECTIONS_ENUM)
#undef EGG_VM_SECTIONS_ENUM
  };

  class IModule : public IHardAcquireRelease {
  public:
    virtual INode& getRootNode() const = 0;
  };
  using Module = HardPtr<IModule>;

  class ModuleFactory {
  public:
    static Module fromBinaryStream(IAllocator& allocator, std::istream& stream);
    static Module fromMemory(IAllocator& allocator, const uint8_t* begin, const uint8_t* end);
    static Module fromRootNode(IAllocator& allocator, INode& root);
    static void toBinaryStream(const IModule& module, std::ostream& stream);
    static Memory toMemory(IAllocator& allocator, const IModule& module);
  };

  class ModuleBuilder {
    ModuleBuilder(const ModuleBuilder&) = delete;
    ModuleBuilder& operator=(const ModuleBuilder&) = delete;
  public:
    IAllocator& allocator;
    Nodes attributes;
  public:
    explicit ModuleBuilder(IAllocator& allocator);
    Node createModule(const Node& block);
    Node createValueInt(Int value);
    Node createValueFloat(Float value);
    Node createValueString(const String& value);
    Node createValueArray(const Nodes& elements);
    Node createValueObject(const Nodes& fields);
    Node createOperator(Opcode opcode, Int op, const Nodes& children);
    Node createNode(Opcode opcode);
    Node createNode(Opcode opcode, const Node& child0);
    Node createNode(Opcode opcode, const Node& child0, const Node& child1);
    Node createNode(Opcode opcode, const Node& child0, const Node& child1, const Node& child2);
    Node createNode(Opcode opcode, const Node& child0, const Node& child1, const Node& child2, const Node& child3);
    Node createNode(Opcode opcode, const Nodes& children);
  };
}
