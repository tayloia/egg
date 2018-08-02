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
    template<typename T>
    Variant(T) = delete;
  private:
    union {
      Bool b;
      Int i;
      Float f;
      const IString* s;
      void* o;
      const IMemory* m;
      void* p;
      void* x;
    } u;
  public:
    // Construction/destruction
    Variant() : VariantKind(VariantBits::Void) {
      this->u.x = nullptr;
    }
    Variant(const Variant&);
    Variant(Variant&&);
    ~Variant() {
      Variant::destroyInternals(*this);
    }
    // Assignment
    Variant& operator=(const Variant& rhs) {
      Variant::copyInternals(*this, rhs);
      return *this;
    }
    Variant& operator=(Variant&& rhs) {
      Variant::destroyInternals(*this);
      Variant::moveInternals(*this, rhs);
      rhs.setKind(VariantBits::Void);
      return *this;
    }
    // Null
    Variant(nullptr_t) : VariantKind(VariantBits::Null) {
      this->u.x = nullptr;
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
    // String
    Variant(const String& value) : VariantKind(VariantBits::String) {
      // We've got to create a string without an allocator
      this->u.s = String::hardAcquire(value.underlying());
    }
    Variant(const std::string& value) : VariantKind(VariantBits::String) {
      // We've got to create a string without an allocator
      this->u.s = Variant::acquireFallbackString(value.data(), value.size());
    }
    Variant(const char* value) : VariantKind(VariantBits::String) {
      if (value == nullptr) {
        this->setKind(VariantBits::Null);
        this->u.x = nullptr;
      } else {
        // We've got to create a string without an allocator
        this->u.s = Variant::acquireFallbackString(value, std::strlen(value));
      }
    }
    String getString() const {
      assert(this->hasAny(VariantBits::String));
      assert(this->u.s != nullptr);
      return String(*this->u.s);
    }
  private:
    static void destroyInternals(Variant& dst) {
      // Leaves 'dst' invalid
      if (dst.hasAny(VariantBits::String)) {
        assert(dst.u.s != nullptr);
        dst.u.s->hardRelease();
      }
    }
    static void copyInternals(Variant& dst, const Variant& src);
    static void moveInternals(Variant& dst, const Variant& src) {
      // Leaves 'src' invalid
      dst.setKind(src.getKind());
      dst.u = src.u;
    }
    static const IString* acquireFallbackString(const char* utf8, size_t bytes);
  };
}
