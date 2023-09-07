namespace egg::ovum {
  class IVM;

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
  };
}
