namespace egg::ovum {
  class IVM;

  class Object : public HardPtr<IObject> {
  public:
    explicit Object(IObject* object = nullptr)
      : HardPtr<IObject>(object) {
      assert((object == nullptr) || object->validate());
    }
    bool validate() const {
      auto* object = this->get();
      return (object != nullptr) && object->validate();
    }
    bool equals(const Object& other) const {
      // Equality is identity for objects
      return this->get() == other.get();
    }
  };

  class ObjectFactory {
  public:
    // Builtin factories
    static Object createBuiltinAssert(IVM& vm);
    static Object createBuiltinPrint(IVM& vm);
  };
}
