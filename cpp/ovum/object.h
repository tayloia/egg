namespace egg::ovum {
  class IVM;

  class HardObject : public HardPtr<IObject> {
  public:
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

  class ObjectFactory {
  public:
    // Builtin factories
    static HardObject createBuiltinAssert(IVM& vm);
    static HardObject createBuiltinPrint(IVM& vm);
    static HardObject createBuiltinExpando(IVM& vm); // TODO deprecate
    static HardObject createBuiltinCollector(IVM& vm); // TODO testing only
  };
}
