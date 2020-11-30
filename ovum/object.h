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

    // An "omni" function looks like 'any?(...any?)'
    static const IFunctionSignature& OmniFunctionSignature;
  };

  class ObjectFactory {
  public:
    static Object createEmpty(IAllocator& allocator);
    static Object createPointer(IAllocator& allocator, ISlot& slot, const Type& pointee, Modifiability modifiability);
  };
}
