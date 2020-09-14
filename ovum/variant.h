namespace egg::ovum {
  class Variant;
  class VariantFactory;
  class VariantSoft;

  class IVariantSoft : public ICollectable {
  public:
    virtual Variant& getVariant() = 0;
    virtual Type getPointerType() const = 0;
  };

  class Variant final {
    // Stop type promotion for implicit constructors
    template<typename T> Variant(T rhs) = delete;
    friend class VariantFactory;
    friend class VariantSoft;
  private:
    Value underlying;
  public:
    // Construction/destruction
    Variant() {}
    Variant(const Variant& rhs) : underlying(rhs.underlying) {}
    Variant(Variant&& rhs) noexcept : underlying(std::move(rhs.underlying)) {}
    ~Variant() {}
    Variant(const Value& rhs) : underlying(rhs) {}
    // Conversion
    operator const Value&() const {
      return this->underlying;
    }
    operator Value& () {
      return this->underlying;
    }
    // From VariantKind
    bool hasOne(ValueFlags mask) const { return Bits::hasOneSet(this->getKind(), mask); }
    bool hasAny(ValueFlags mask) const { return Bits::hasAnySet(this->getKind(), mask); }
    bool hasAll(ValueFlags mask) const { return Bits::hasAllSet(this->getKind(), mask); }
    bool hasBool() const { return Bits::hasAnySet(this->getKind(), ValueFlags::Bool); }
    bool hasString() const { return Bits::hasAnySet(this->getKind(), ValueFlags::String); }
    bool hasObject() const { return Bits::hasAnySet(this->getKind(), ValueFlags::Object); }
    bool hasPointer() const { return Bits::hasAnySet(this->getKind(), ValueFlags::Pointer); }
    bool hasThrow() const { return Bits::hasAnySet(this->getKind(), ValueFlags::Throw); }
    bool hasYield() const { return Bits::hasAnySet(this->getKind(), ValueFlags::Yield); }
    bool hasFlowControl() const { return Bits::hasAnySet(this->getKind(), ValueFlags::FlowControl); }
    bool is(ValueFlags value) const { return this->getKind() == value; }
    bool isVoid() const { return this->getKind() == ValueFlags::Void; }
    bool isNull() const { return this->getKind() == ValueFlags::Null; }
    bool isBool() const { return this->getKind() == ValueFlags::Bool; }
    bool isInt() const { return this->getKind() == ValueFlags::Int; }
    bool isFloat() const { return this->getKind() == ValueFlags::Float; }
    bool isString() const { return this->getKind() == ValueFlags::String; }
    ValueFlags getKind() const;
    // Assignment
    Variant& operator=(const Variant& rhs) = default;
    Variant& operator=(Variant&& rhs) noexcept = default;
    // Equality
    static bool equals(const Variant& lhs, const Variant& rhs, ValueCompare compare) {
      return Value::equals(lhs.underlying, rhs.underlying, compare);
    }
    // Null
    Variant(std::nullptr_t);
    // Bool
    Variant(bool value);
    Bool getBool() const {
      Bool value;
      if (this->underlying->getBool(value)) {
        return value;
      }
      assert(false);
      return value;
    }
    // Int (support automatic promotion of 32-bit integers)
    Variant(int32_t value);
    Variant(int64_t value);
    Int getInt() const {
      Int value;
      if (this->underlying->getInt(value)) {
        return value;
      }
      assert(false);
      return value;
    }
    // Float (support automatic promotion of 32-bit IEEE)
    Variant(float value);
    Variant(double value);
    Float getFloat() const {
      Float value;
      if (this->underlying->getFloat(value)) {
        return value;
      }
      assert(false);
      return value;
    }
    // String
    Variant(const String& value);
    Variant(const std::string& value);
    Variant(const char* value);
    String getString() const {
      String value;
      if (this->underlying->getString(value)) {
        return value;
      }
      assert(false);
      return value;
    }
    // Memory
    Variant(const Memory& value);
    Memory getMemory() const {
      Memory value;
      if (this->underlying->getMemory(value)) {
        return value;
      }
      assert(false);
      return value;
    }
    // Object
    Variant(const Object& value);
    Object getObject() const;
    // Pointer
    Variant getPointee() const;
    // Properties
    Type getRuntimeType() const;
    String toString() const;
    void addFlowControl(ValueFlags bits);
    bool stripFlowControl(ValueFlags bits);
    void soften(IBasket& basket);
    void softVisitLink(const ICollectable::Visitor& visitor) const;
    bool validate() const;
    // Constants
    static const Variant Void;
    static const Variant Null;
    static const Variant False;
    static const Variant True;
    static const Variant Break;
    static const Variant Continue;
    static const Variant Rethrow;
  private:
    static const IMemory* acquireFallbackString(const char* utf8, size_t bytes);
  };
}
