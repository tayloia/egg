namespace egg::ovum {
  class IValue : public ICollectable {
  public:
    virtual void softVisit(ICollectable::IVisitor& visitor) const = 0;
    virtual bool getVoid() const = 0;
    virtual bool getNull() const = 0;
    virtual bool getBool(Bool& value) const = 0;
    virtual bool getInt(Int& value) const = 0;
    virtual bool getFloat(Float& value) const = 0;
    virtual bool getString(String& value) const = 0;
    virtual bool getHardObject(HardObject& value) const = 0;
    virtual bool getHardType(Type& value) const = 0;
    virtual bool getInner(HardValue& inner) const = 0;
    virtual Type getRuntimeType() const = 0;
    virtual ValueFlags getPrimitiveFlag() const = 0;
    virtual bool validate() const = 0;
    virtual bool set(const IValue& rhs) = 0;
    virtual HardValue mutate(ValueMutationOp op, const IValue& value) = 0;
  };

  class HardValue {
  private:
    HardPtr<IValue> ptr;
  public:
    // Construction/destruction
    HardValue();
    HardValue(const HardValue& rhs) : ptr(rhs.ptr) {
      assert(this->validate());
    }
    HardValue(HardValue&& rhs) noexcept : ptr(std::move(rhs.ptr)) {
      assert(this->validate());
    }
    explicit HardValue(IValue& rhs) : ptr(&rhs) {
      assert(this->validate());
    }
    // Atomic assignment
    HardValue& operator=(const HardValue& rhs) {
      assert(this->validate());
      assert(rhs.validate());
      this->ptr = rhs.ptr;
      assert(this->validate());
      return *this;
    }
    HardValue& operator=(HardValue&& rhs) noexcept {
      assert(this != &rhs);
      assert(this->validate());
      assert(rhs.validate());
      this->ptr = std::move(rhs.ptr);
      assert(this->validate());
      return *this;
    }
    // Atomic access
    IValue& get() const {
      auto p = this->ptr.get();
      assert(p != nullptr);
      assert(p->validate());
      return *p;
    }
    IValue* operator->() const {
      return &this->get();
    }
    // Debugging
    bool validate() const;
    // Helpers
    bool hasAnyFlags(ValueFlags flags) const {
      return Bits::hasAnySet(this->get().getPrimitiveFlag(), flags);
    }
    bool hasFlowControl() const {
      return this->hasAnyFlags(ValueFlags::FlowControl);
    }
    // Constants
    static const HardValue Void;
    static const HardValue Null;
    static const HardValue False;
    static const HardValue True;
    static const HardValue Break;
    static const HardValue Continue;
    static const HardValue Rethrow;
    static const HardValue YieldBreak;
    static const HardValue YieldContinue;
  };

  class SoftKey {
    friend class IVM;
    SoftKey& operator=(const SoftKey&) = delete;
  private:
    const IValue* ptr;
  public:
    // Construction
    explicit SoftKey(const SoftKey& value);
    SoftKey(IVM& vm, const HardValue& value);
    // Atomic access
    const IValue& get() const {
      auto* p = this->ptr;
      assert(p != nullptr);
      assert(p->validate());
      return *p;
    }
    const IValue* operator->() const {
      return &this->get();
    }
    void visit(ICollectable::IVisitor& visitor) const {
      visitor.visit(*this->ptr);
    }
    // Comparison for containers
    static int compare(const IValue& lhs, const IValue& rhs);
    // Debugging
    bool validate() const;
  };

  class SoftValue {
    friend class IVM;
    SoftValue(const SoftValue&) = delete;
    SoftValue& operator=(const SoftValue&) = delete;
  private:
    SoftPtr<IValue, IValue> ptr;
  public:
    // Construction
    explicit SoftValue(IVM& vm);
    SoftValue(IVM& vm, const HardValue& init);
    // Atomic access
    IValue& get() const {
      auto p = this->ptr.get();
      assert(p != nullptr);
      assert(p->validate());
      return *p;
    }
    IValue* operator->() const {
      return &this->get();
    }
    void visit(ICollectable::IVisitor& visitor) const {
      this->ptr.visit(visitor);
    }
    // Debugging
    bool validate() const;
    // Factory
    static IValue* createPoly(IAllocator& allocator);
  };

  class SoftComparator {
  public:
    // See https://stackoverflow.com/a/31924435
    using is_transparent = std::true_type;
    bool operator()(const SoftKey& lhs, const SoftKey& rhs) const {
      return SoftKey::compare(lhs.get(), rhs.get()) < 0;
    }
    bool operator()(const SoftKey& lhs, const HardValue& rhs) const {
      return SoftKey::compare(lhs.get(), rhs.get()) < 0;
    }
    bool operator()(const HardValue& lhs, const SoftKey& rhs) const {
      return SoftKey::compare(lhs.get(), rhs.get()) < 0;
    }
  };

  class ValueFactory {
  public:
    static const HardValue& createBool(bool value) {
      return value ? HardValue::True : HardValue::False;
    }
    static HardValue createInt(IAllocator& allocator, Int value);
    static HardValue createFloat(IAllocator& allocator, Float value);
    static HardValue createString(IAllocator& allocator, const String& value);
    static HardValue createHardObject(IAllocator& allocator, const HardObject& value);
    static HardValue createHardThrow(IAllocator& allocator, const HardValue& inner);
    static HardValue createHardReturn(IAllocator& allocator, const HardValue& inner);
    static HardValue createHardYield(IAllocator& allocator, const HardValue& inner);
    static HardValue createType(IAllocator& allocator, const Type& value);

    // Helpers
    template<size_t N>
    static HardValue createStringLiteral(IAllocator& allocator, const char(&value)[N]) {
      return createStringASCII(allocator, value, N - 1);
    }
    static HardValue createStringASCII(IAllocator& allocator, const char* value, size_t codepoints = SIZE_MAX);
    static HardValue createStringUTF8(IAllocator& allocator, const void* value, size_t bytes = SIZE_MAX, size_t codepoints = SIZE_MAX);
    static HardValue createStringUTF8(IAllocator& allocator, const std::u8string& value, size_t codepoints = SIZE_MAX);
    static HardValue createStringUTF32(IAllocator& allocator, const std::u32string& value);

    // Overloaded without implicit promotion
    template<typename T>
    static HardValue create(IAllocator& allocator, T value) = delete;
    static HardValue create(IAllocator&, std::nullptr_t) {
      return HardValue::Null;
    }
    static HardValue create(IAllocator&, bool value) {
      return ValueFactory::createBool(value);
    }
    static HardValue create(IAllocator& allocator, int32_t value) {
      return ValueFactory::createInt(allocator, value);
    }
    static HardValue create(IAllocator& allocator, int64_t value) {
      return ValueFactory::createInt(allocator, value);
    }
    static HardValue create(IAllocator& allocator, float value) {
      return ValueFactory::createFloat(allocator, value);
    }
    static HardValue create(IAllocator& allocator, double value) {
      return ValueFactory::createFloat(allocator, value);
    }
    static HardValue create(IAllocator& allocator, const String& value) {
      return ValueFactory::createString(allocator, value);
    }
  };
}
