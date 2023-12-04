namespace egg::ovum {
  class IVM;
  class IVMCallStack;

  class HardObject : public HardPtr<IObject> {
  public:
    HardObject(const HardObject&) = default;
    HardObject& operator=(const HardObject&) = default;
    explicit HardObject(IObject* object = nullptr)
      : HardPtr<IObject>(object) {
      assert((object == nullptr) || object->validate());
    }
    bool validate() const {
      auto* object = this->get();
      return (object != nullptr) && object->validate();
    }
    bool equals(const HardObject& other) const {
      // Equality is identity for objects
      return this->get() == other.get();
    }
  };

  class SoftObject : public SoftPtr<IObject> {
    SoftObject(const SoftObject&) = delete;
    SoftObject& operator=(const SoftObject&) = delete;
  public:
    SoftObject()
      : SoftPtr<IObject>() {
    }
    bool validate() const {
      auto* object = this->get();
      return (object != nullptr) && object->validate();
    }
    bool equals(const SoftObject& other) const {
      // Equality is identity for objects
      return this->get() == other.get();
    }
  };

  class ObjectFactory {
  public:
    // Builtin factories
    static HardObject createBuiltinAssert(IVM& vm);
    static HardObject createBuiltinPrint(IVM& vm);
    static HardObject createBuiltinExpando(IVM& vm); // TODO deprecate
    static HardObject createBuiltinCollector(IVM& vm); // TODO testing only
    // Vanilla factories
    static HardObject createVanillaArray(IVM& vm);
    static HardObject createVanillaObject(IVM& vm);
    // String proxy factories
    static HardObject createStringProxyCompareTo(IVM& vm, const String& instance);
    static HardObject createStringProxyContains(IVM& vm, const String& instance);
    static HardObject createStringProxyEndsWith(IVM& vm, const String& instance);
    static HardObject createStringProxyHash(IVM& vm, const String& instance);
    static HardObject createStringProxyIndexOf(IVM& vm, const String& instance);
    static HardObject createStringProxyJoin(IVM& vm, const String& instance);
    static HardObject createStringProxyLastIndexOf(IVM& vm, const String& instance);
    static HardObject createStringProxyPadLeft(IVM& vm, const String& instance);
    static HardObject createStringProxyPadRight(IVM& vm, const String& instance);
    static HardObject createStringProxyRepeat(IVM& vm, const String& instance);
    static HardObject createStringProxyReplace(IVM& vm, const String& instance);
    static HardObject createStringProxySlice(IVM& vm, const String& instance);
    static HardObject createStringProxyStartsWith(IVM& vm, const String& instance);
    static HardObject createStringProxyToString(IVM& vm, const String& instance);
    // Error factories
    static HardObject createRuntimeError(IVM& vm, const String& message, const HardPtr<IVMCallStack>& callstack = nullptr);
  };
}
