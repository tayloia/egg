namespace egg::ovum {
  enum class VariantBits {
    Void = 1 << 0,
    Null = 1 << 1,
    Bool = 1 << 2,
    Int = 1 << 3,
    Float = 1 << 4,
    String = 1 << 5,
    Object = 1 << 6,
    Memory = 1 << 7,
    Pointer = 1 << 8,
    Indirect = 1 << 9,
    Exception = 1 << 10
  };
  inline VariantBits operator|(VariantBits lhs, VariantBits rhs) {
    using Underlying = std::underlying_type_t<VariantBits>;
    return static_cast<VariantBits>(static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));
  }

  class VariantKind {
  public:
    using Underlying = std::underlying_type_t<VariantBits>;
  private:
    Underlying bits;
  public:
    explicit VariantKind(VariantBits kind) : bits(static_cast<Underlying>(kind)) {
    }
    bool hasAny(VariantBits mask) const {
      auto underlying = static_cast<Underlying>(mask);
      return (this->bits & underlying) != 0;
    }
    bool hasAll(VariantBits mask) const {
      auto underlying = static_cast<Underlying>(mask);
      return (this->bits & underlying) == underlying;
    }
    bool is(VariantBits value) const {
      auto underlying = static_cast<Underlying>(value);
      return this->bits == underlying;
    }
    VariantBits getKind() const {
      return static_cast<VariantBits>(this->bits);
    }
  protected:
    void setKind(VariantBits kind) {
      this->bits = static_cast<Underlying>(kind);
    }
  };

  class Variant final : public VariantKind {
  private:
    union {
      Bool b;
      Int i;
      Float f;
      const IString* s;
      void* o;
      const IMemory* m;
      void* p;
    } u;
  public:
    // Construction/destruction
    Variant() : VariantKind(VariantBits::Void) {
    }
    Variant(const Variant&);
    Variant(Variant&&);
    ~Variant() {
      this->destroyInternals();
    }
    // Assignment
    Variant& operator=(const Variant&);
    Variant& operator=(Variant&& rhs) {
      this->moveInternals(rhs);
      rhs.setKind(VariantBits::Void);
      return *this;
    }
    // Bool
    Variant(Bool value) : VariantKind(VariantBits::Bool) {
      this->u.b = value;
    }
    Bool getBool() const {
      assert(this->hasAny(VariantBits::Bool));
      return this->u.b;
    }
    // Int
    Variant(int value) : VariantKind(VariantBits::Int) {
      this->u.i = value;
    }
    Variant(Int value) : VariantKind(VariantBits::Int) {
      this->u.i = value;
    }
    Int getInt() const {
      assert(this->hasAny(VariantBits::Int));
      return this->u.i;
    }
    // Float
    Variant(Float value) : VariantKind(VariantBits::Float) {
      this->u.f = value;
    }
    Float getFloat() const {
      assert(this->hasAny(VariantBits::Float));
      return this->u.f;
    }
  private:
    void destroyInternals() {
      // WIBBLE
    }
    void copyInternals();
    void moveInternals(const Variant& src) {
      this->setKind(src.getKind());
      this->u = src.u;
    }
  };
}
