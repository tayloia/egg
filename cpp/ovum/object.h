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
    void swap(HardObject& that) {
      HardObject::hardSwap(*this, that);
    }
  };

  class SoftObject : public SoftPtr<IObject> {
    SoftObject(const SoftObject&) = delete;
    SoftObject& operator=(const SoftObject&) = delete;
  public:
    SoftObject(IVM& vm, const HardObject& instance);
    bool validate() const {
      auto* object = this->get();
      return (object != nullptr) && object->validate();
    }
    bool equals(const SoftObject& other) const {
      // Equality is identity for objects
      return this->get() == other.get();
    }
  };

  class IObjectBuilder : public IHardAcquireRelease {
  public:
    // Interface
    virtual void addProperty(const HardValue& property, const HardValue& value, Accessability accessability) = 0;
    virtual HardObject build() = 0;
  };

  class ObjectFactory {
  public:
    // Builtin factories
    static HardObject createBuiltinAssert(IVM& vm);
    static HardObject createBuiltinPrint(IVM& vm);
    static HardObject createBuiltinExpando(IVM& vm); // TODO deprecate
    static HardObject createBuiltinCollector(IVM& vm); // TODO testing only
    static HardObject createBuiltinSymtable(IVM& vm); // TODO testing only
    // Manifestation factories
    static HardObject createManifestationType(IVM& vm);
    static HardObject createManifestationVoid(IVM& vm);
    static HardObject createManifestationBool(IVM& vm);
    static HardObject createManifestationInt(IVM& vm);
    static HardObject createManifestationFloat(IVM& vm);
    static HardObject createManifestationString(IVM& vm);
    static HardObject createManifestationObject(IVM& vm);
    static HardObject createManifestationAny(IVM& vm);
    // Vanilla factories
    static HardObject createVanillaArray(IVM& vm, const Type& elementType, Accessability accessability);
    static HardObject createVanillaObject(IVM& vm, const Type& runtimeType, Accessability accessability);
    static HardObject createVanillaKeyValue(IVM& vm, const HardValue& key, const HardValue& value, Accessability accessability);
    static HardObject createVanillaManifestation(IVM& vm, const Type& infratype, const Type& metatype);
    // Pointer factories
    static HardObject createPointerToValue(IVM& vm, const HardValue& value, Modifiability modifiability);
    static HardObject createPointerToIndex(IVM& vm, const HardObject& instance, const HardValue& index, Modifiability modifiability, const Type& pointerType);
    static HardObject createPointerToProperty(IVM& vm, const HardObject& instance, const HardValue& property, Modifiability modifiability, const Type& pointerType);
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
    // Builder factories
    static HardPtr<IObjectBuilder> createObjectBuilder(IVM& vm, Accessability accessability);
    static HardPtr<IObjectBuilder> createRuntimeErrorBuilder(IVM& vm, const String& message, const HardPtr<IVMCallStack>& callstack);
  };
}
