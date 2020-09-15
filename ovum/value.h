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
  class Slot;

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
    virtual bool getMemory(Memory& value) const = 0;
    virtual bool getChild(Value& child) const = 0;
    virtual ValueFlags getFlags() const = 0;
    virtual Type getRuntimeType() const = 0;
    virtual bool equals(const IValue& rhs, ValueCompare compare) const = 0;
    // TODO virtual String toString() const = 0;
    virtual bool validate() const = 0;
  };

  class Value {
    friend class Slot;
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
    // Atomic assignment
    Value& operator=(const Value& rhs) {
      assert(this->validate());
      assert(rhs.validate());
      this->ptr = rhs.ptr;
      assert(this->validate());
      assert(rhs.validate());
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
    // Debugging and test
    bool validate() const;
    static void print(std::ostream& stream, ValueFlags flags);
    static void print(std::ostream& stream, const Value& value);
  private:
    explicit Value(IValue* rhs) : ptr(rhs) {
      // Used solely for factory/slot construction
    }
  };
}
