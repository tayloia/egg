namespace egg::ovum {
  class Object : public HardPtr<IObject> {
  public:
    Object() : HardPtr(nullptr) {
      assert(this->get() == nullptr);
    }
    explicit Object(const IObject& rhs) : HardPtr(&rhs) {
      assert(this->get() != nullptr);
    }
    bool validate() const {
      auto* underlying = this->get();
      return (underlying != nullptr) && underlying->validate();
    }

    // Useful constant for calling function with no parameters
    static const IParameters& ParametersNone;
  };
}
