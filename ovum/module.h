namespace egg::ovum {
  enum Section {
#define EGG_VM_SECTIONS_ENUM(section, value) section = value,
    EGG_VM_SECTIONS(EGG_VM_SECTIONS_ENUM)
#undef EGG_VM_SECTIONS_ENUM
  };

  class IModule : public IHardAcquireRelease {
  public:
    virtual String getResourceName() const = 0;
    virtual INode& getRootNode() const = 0;
  };

  class Module : public HardPtr<IModule> {
  public:
    Module(nullptr_t = nullptr) {} // implicit
    explicit Module(const IModule* module) : HardPtr(module) {}

    // Helpers
    static size_t childrenFromMachineByte(uint8_t byte) {
      // Compute the number of children from a VM code byte
      auto following = byte % (EGG_VM_NARGS + 1);
      return (following < EGG_VM_NARGS) ? following : SIZE_MAX;
    }
    static Opcode opcodeFromMachineByte(uint8_t byte);
  };


  class ModuleFactory {
  public:
    static Module fromBinaryStream(IAllocator& allocator, const String& resource, std::istream& stream);
    static Module fromMemory(IAllocator& allocator, const String& resource, const uint8_t* begin, const uint8_t* end);
    static Module fromRootNode(IAllocator& allocator, const String& resource, INode& root);
    static void toBinaryStream(const IModule& module, std::ostream& stream);
    static Memory toMemory(IAllocator& allocator, const IModule& module);
  };

  class ModuleBuilderBase {
  public:
    IAllocator& allocator;
    Nodes attributes;
  protected:
    explicit ModuleBuilderBase(IAllocator& allocator);
    ModuleBuilderBase(const ModuleBuilderBase&) = delete;
    ModuleBuilderBase& operator=(const ModuleBuilderBase&) = delete;
    ModuleBuilderBase(ModuleBuilderBase&&) = default;
    template<typename T>
    void addAttribute(const String& key, const T& value) {
      this->addAttribute(key, NodeFactory::createValue(this->allocator, value));
    }
    void addAttribute(const String& key, const char* value) = delete;
    void addAttribute(const String& key, Node&& value);
  public:
    Node createModule(Node&& block);
    Node createValueInt(Int value);
    Node createValueFloat(Float value);
    Node createValueString(const String& value);
    Node createValueArray(const Nodes& elements);
    Node createValueObject(const Nodes& fields);
    Node createOperator(Opcode opcode, Operator oper, const Nodes& children);
    Node createNode(Opcode opcode);
    Node createNode(Opcode opcode, Node&& child0);
    Node createNode(Opcode opcode, Node&& child0, Node&& child1);
    Node createNode(Opcode opcode, Node&& child0, Node&& child1, Node&& child2);
    Node createNode(Opcode opcode, Node&& child0, Node&& child1, Node&& child2, Node&& child3);
    Node createNode(Opcode opcode, const Nodes& children);
  private:
    Node createNodeWithAttributes(Opcode opcode, const Nodes* children);
  };

  class ModuleBuilderWithAttribute : public ModuleBuilderBase {
    ModuleBuilderWithAttribute(const ModuleBuilderWithAttribute&) = delete;
    ModuleBuilderWithAttribute& operator=(const ModuleBuilderWithAttribute&) = delete;
    friend class ModuleBuilder;
  private:
    explicit ModuleBuilderWithAttribute(IAllocator& allocator) : ModuleBuilderBase(allocator) {
    }
  public:
    ModuleBuilderWithAttribute(ModuleBuilderWithAttribute&&) = default;
    template<typename T>
    ModuleBuilderWithAttribute& withAttribute(const String& key, const T& value) {
      this->addAttribute(key, value);
      return *this;
    }
  };

  class ModuleBuilder : public ModuleBuilderBase {
    ModuleBuilder(const ModuleBuilder&) = delete;
    ModuleBuilder& operator=(const ModuleBuilder&) = delete;
  public:
    explicit ModuleBuilder(IAllocator& allocator) : ModuleBuilderBase(allocator) {
    }
    template<typename T>
    ModuleBuilderWithAttribute withAttribute(const String& key, const T& value) const {
      ModuleBuilderWithAttribute with(this->allocator);
      with.addAttribute(key, value);
      return with;
    }
  };
}
