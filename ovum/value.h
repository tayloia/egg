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
  class Object;
  class Type;
  class Value;

  enum class ValueCompare {
    Binary = 0x00,
    PromoteInts = 0x01
  };

  class IValue : public IHardAcquireRelease {
  public:
    virtual IValue* softAcquire() const = 0;
    virtual void softRelease() const = 0;
    virtual bool getVoid() const = 0;
    virtual bool getNull() const = 0;
    virtual bool getBool(Bool& value) const = 0;
    virtual bool getInt(Int& value) const = 0;
    virtual bool getFloat(Float& value) const = 0;
    virtual bool getString(String& value) const = 0;
    virtual bool getObject(Object& value) const = 0;
    virtual bool getInner(Value& inner) const = 0;
    virtual ValueFlags getFlags() const = 0;
    virtual Type getRuntimeType() const = 0;
    virtual bool equals(const IValue& rhs, ValueCompare compare) const = 0;
    virtual bool validate() const = 0;
    virtual void toStringBuilder(StringBuilder& sb) const = 0;
    String toString() const {
      StringBuilder sb;
      this->toStringBuilder(sb);
      return sb.str();
    }
  };

  class Value {
    friend class ValueFactory;
    // Stop type promotion for implicit constructors
    template<typename T> Value(T rhs) = delete;
  private:
    HardPtr<IValue> ptr;
  public:
    // Construction/destruction
    Value();
    Value(const Value& rhs) : ptr(rhs.ptr) {
      assert(this->validate());
    }
    Value(Value&& rhs) noexcept : ptr(std::move(rhs.ptr)) {
      assert(this->validate());
    }
    explicit Value(IValue& rhs) : ptr(&rhs) {
      assert(this->validate());
    }
    // Atomic assignment
    Value& operator=(const Value& rhs) {
      assert(this->validate());
      assert(rhs.validate());
      this->ptr = rhs.ptr;
      assert(this->validate());
      return *this;
    }
    Value& operator=(Value&& rhs) noexcept {
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
    static bool equals(const Value& lhs, const Value& rhs, ValueCompare compare) {
      auto& p = lhs.get();
      auto& q = rhs.get();
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
    static const Value Void;
    static const Value Null;
    static const Value False;
    static const Value True;
    static const Value Break;
    static const Value Continue;
    static const Value Rethrow;
  };

  class ValueFactory {
  public:
    static Value createInt(IAllocator& allocator, Int value);
    static Value createFloat(IAllocator& allocator, Float value);
    static Value createString(IAllocator& allocator, const String& value);
    static Value createObject(IAllocator& allocator, const Object& value);
    static Value createFlowControl(IAllocator& allocator, ValueFlags flags, const Value& value);
    static Value createThrowError(IAllocator& allocator, const LocationSource& location, const String& message);
    template<typename... ARGS>
    static Value createThrowString(IAllocator& allocator, ARGS&&... args) {
      auto message = StringBuilder::concat(std::forward<ARGS>(args)...);
      auto value = ValueFactory::createString(allocator, message);
      return ValueFactory::createFlowControl(allocator, ValueFlags::Throw, value);
    }

    // Overloaded without implicit promotion
    template<typename T>
    static Value create(IAllocator& allocator, T value) = delete;
    static Value create(IAllocator&, std::nullptr_t) {
      return Value::Null;
    }
    static Value create(IAllocator&, bool value) {
      return value ? Value::True : Value::False;
    }
    static Value create(IAllocator& allocator, int32_t value) {
      return createInt(allocator, value);
    }
    static Value create(IAllocator& allocator, int64_t value) {
      return createInt(allocator, value);
    }
    static Value create(IAllocator& allocator, float value) {
      return createFloat(allocator, value);
    }
    static Value create(IAllocator& allocator, double value) {
      return createFloat(allocator, value);
    }
    static Value create(IAllocator& allocator, const String& value) {
      return createString(allocator, value);
    }
    static Value create(IAllocator& allocator, const Object& value) {
      return createObject(allocator, value);
    }
    static Value createUTF8(IAllocator& allocator, const std::string& value) {
      return createString(allocator, StringFactory::fromUTF8(allocator, value));
    }
    static Value createASCIIZ(IAllocator& allocator, const char* value) {
      if (value == nullptr) {
        return Value::Null;
      }
      return createString(allocator, StringFactory::fromASCIIZ(allocator, value));
    }
  };
}
