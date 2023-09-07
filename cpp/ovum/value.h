// At first glance, it looks like this has far too many levels of indirection.
// However, the old 'Variant' mechanism before egg-2020 had serious flaws.
// It conflated VALUEs with SLOTs.
// VALUEs hold bools, ints, floats, pointers, etc.
// SLOTs are stable (in terms of location in memory).
// The competing features that we need to support are:
//  * Slots may have soft/weak links to them
//  * Slot modifications must be atomic.
//  * Value mutations may be atomic.
//  * Neither values nor slots are immutable.

namespace egg::ovum {
  enum class ValueCompare {
    Binary = 0x00,
    PromoteInts = 0x01
  };

  class IValue : public IHardAcquireRelease {
  public:
    virtual IValue* softAcquire() const = 0;
    virtual void softRelease() const = 0;
    virtual void softVisit(const ICollectable::Visitor& visitor) const = 0;
    virtual bool getVoid() const = 0;
    virtual bool getNull() const = 0;
    virtual bool getBool(Bool& value) const = 0;
    virtual bool getInt(Int& value) const = 0;
    virtual bool getFloat(Float& value) const = 0;
    virtual bool getString(String& value) const = 0;
    virtual bool getHardObject(HardObject& value) const = 0;
    virtual bool getInner(HardValue& inner) const = 0;
    virtual ValueFlags getFlags() const = 0;
    virtual Type getRuntimeType() const = 0;
    virtual bool equals(const IValue& rhs, ValueCompare compare) const = 0;
    virtual bool validate() const = 0;
    virtual void print(Printer& printer) const = 0;
  };

  class HardValue {
    friend class ValueFactory;
    // Stop type promotion for implicit constructors
    template<typename T> HardValue(T rhs) = delete;
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
    // Equality
    static bool equals(const HardValue& lhs, const HardValue& rhs, ValueCompare compare) {
      const auto& p = lhs.get();
      const auto& q = rhs.get();
      return p.equals(q, compare);
    }
    // Debugging
    bool validate() const;
    // Helpers
    bool hasAnyFlags(ValueFlags flags) const {
      return Bits::hasAnySet(this->get().getFlags(), flags);
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
    static HardValue createHardFlowControl(IAllocator& allocator, ValueFlags flags, const HardValue& value);

    // Helpers
    template<size_t N>
    static HardValue createStringLiteral(IAllocator& allocator, const char(&value)[N]) {
      return createStringASCII(allocator, value, N - 1);
    }
    static HardValue createStringASCII(IAllocator& allocator, const char* value, size_t codepoints = SIZE_MAX);
    static HardValue createStringUTF8(IAllocator& allocator, const std::u8string& value, size_t codepoints = SIZE_MAX);
    static HardValue createStringUTF32(IAllocator& allocator, const std::u32string& value);

    // Overloaded without implicit promotion
    template<typename T>
    static HardValue create(IAllocator& allocator, T value) = delete;
    static HardValue create(IAllocator&, std::nullptr_t) {
      return HardValue::Null;
    }
    static HardValue create(IAllocator&, bool value) {
      return createBool(value);
    }
    static HardValue create(IAllocator& allocator, int32_t value) {
      return createInt(allocator, value);
    }
    static HardValue create(IAllocator& allocator, int64_t value) {
      return createInt(allocator, value);
    }
    static HardValue create(IAllocator& allocator, float value) {
      return createFloat(allocator, value);
    }
    static HardValue create(IAllocator& allocator, double value) {
      return createFloat(allocator, value);
    }
    static HardValue create(IAllocator& allocator, const String& value) {
      return createString(allocator, value);
    }
  };
}
