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

  class ObjectFactory {
  public:
    template<typename T, typename... ARGS>
    static Object create(IAllocator& allocator, ARGS&&... args) {
      // Use perfect forwarding
      return Object(*allocator.make<T>(std::forward<ARGS>(args)...));
    }
    static Object createEmpty(IAllocator& allocator);
  };
}
