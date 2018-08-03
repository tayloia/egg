namespace egg::ovum {
  class Variant;
  class VariantFactory;

  enum class VariantBits {
    Void = 1 << 0,
    Null = 1 << 1,
    Bool = 1 << 2,
    Int = 1 << 3,
    Float = 1 << 4,
    String = 1 << 5,
    Memory = 1 << 6,
    Object = 1 << 7,
    Pointer = 1 << 8,
    Indirect = 1 << 9,
    Exception = 1 << 10,
    Hard = 1 << 11
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
    void swapKind(VariantKind& other) {
      std::swap(this->bits, other.bits);
    }
  };

  class IVariantSoft : public ICollectable {
  public:
    virtual Variant& getVariant() = 0;
  };

  class Variant final : public VariantKind {
    // Stop type promotion for implicit constructors
    template<typename T> Variant(T rhs) = delete;
    friend class VariantFactory;
  private:
    union {
      Bool b; // Bool
      Int i; // Int
      Float f; // Float
      const IMemory* s; // String|Memory
      IObject* o; // Object
      IVariantSoft* p; // Pointer|Indirect
      uintptr_t x; // others
    } u;
  public:
    // Construction/destruction
    Variant() : VariantKind(VariantBits::Void) {
      this->u.x = 0; // keep valgrind happy
    }
    Variant(const Variant& rhs) : VariantKind(rhs.getKind()) {
      Variant::copyInternals(*this, rhs);
    }
    Variant(Variant&& rhs) : VariantKind(rhs.getKind()) {
      Variant::moveInternals(*this, rhs);
      rhs.setKind(VariantBits::Void);
    }
    ~Variant() {
      Variant::destroyInternals(*this);
    }
    // Assignment
    Variant& operator=(const Variant& rhs) {
      if (this != &rhs) {
        // The resources of 'before' will be cleaned up after the assignment
        Variant before{ std::move(*this) };
        this->setKind(rhs.getKind());
        Variant::copyInternals(*this, rhs);
      }
      return *this;
    }
    Variant& operator=(Variant&& rhs) {
      // See https://stackoverflow.com/a/9322542
      if (this != &rhs) {
        // Need to make sure the resource of the original 'this' are cleaned up last
        this->swap(rhs);
        Variant::destroyInternals(rhs);
        rhs.setKind(VariantBits::Void);
      }
      return *this;
    }
    // Null
    Variant(nullptr_t) : VariantKind(VariantBits::Null) {
      this->u.x = 0;
    }
    // Bool
    Variant(Bool value) : VariantKind(VariantBits::Bool) {
      this->u.b = value;
    }
    Bool getBool() const {
      assert(this->hasAny(VariantBits::Bool));
      return this->u.b;
    }
    // Int (support automatic promotion of 32-bit integers)
    Variant(int32_t value) : VariantKind(VariantBits::Int) {
      this->u.i = value;
    }
    Variant(int64_t value) : VariantKind(VariantBits::Int) {
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
    Variant(const String& value) : VariantKind(VariantBits::String | VariantBits::Hard) {
      this->u.s = String::hardAcquire(value.get());
    }
    Variant(const std::string& value) : VariantKind(VariantBits::String | VariantBits::Hard) {
      // We've got to create a string without an allocator
      this->u.s = Variant::acquireFallbackString(value.data(), value.size());
    }
    Variant(const char* value) : VariantKind(VariantBits::String | VariantBits::Hard) {
      if (value == nullptr) {
        this->setKind(VariantBits::Null);
        this->u.x = 0;
      } else {
        // We've got to create a string without an allocator
        this->u.s = Variant::acquireFallbackString(value, std::strlen(value));
      }
    }
    String getString() const {
      assert(this->hasAny(VariantBits::String));
      return String(this->u.s);
    }
    // Memory
    Variant(const Memory& value) : VariantKind(VariantBits::Memory | VariantBits::Hard) {
      this->u.s = String::hardAcquire(value.get());
      assert(this->u.s != nullptr);
    }
    Memory getMemory() const {
      assert(this->hasAny(VariantBits::Memory));
      assert(this->u.s != nullptr);
      return Memory(this->u.s);
    }
    // Object
    Variant(const Object& value) : VariantKind(VariantBits::Object | VariantBits::Hard) {
      this->u.o = Object::hardAcquire(value.get());
      assert(this->u.o != nullptr);
    }
    Object getObject() const {
      assert(this->hasAny(VariantBits::Object));
      assert(this->u.o != nullptr);
      return Object(*this->u.o);
    }
    // Pointer/Indirect
    Variant(VariantBits flavour, IVariantSoft& value) : VariantKind(flavour) {
      assert((flavour == VariantBits::Pointer) || (flavour == VariantBits::Indirect));
      this->u.p = &value;
      assert(this->u.p != nullptr);
    }
    Variant& getPointee() const {
      assert(this->hasAny(VariantBits::Pointer | VariantBits::Indirect));
      assert(this->u.p != nullptr);
      return this->u.p->getVariant();
    }
  private:
    void swap(Variant& other) {
      this->swapKind(other);
      std::swap(this->u, other.u);
    }
    static void copyInternals(Variant& dst, const Variant& src) {
      // dst:INVALID,src:VALID => dst:VALID,src:VALID
      assert(dst.getKind() == src.getKind());
      if (src.hasAny(VariantBits::Hard)) {
        if (src.hasAny(VariantBits::Object)) {
          dst.u.o = Object::hardAcquire(src.u.o);
          return;
        }
        if (src.hasAny(VariantBits::String | VariantBits::Memory)) {
          dst.u.s = String::hardAcquire(src.u.s);
          return;
        }
      }
      dst.u = src.u;
    }
    static void moveInternals(Variant& dst, const Variant& src) {
      // dst:INVALID,src:VALID => dst:VALID,src:INVALID
      assert(dst.getKind() == src.getKind());
      dst.u = src.u;
    }
    static void destroyInternals(Variant& dst) {
      // dst:VALID => dst:INVALID
      if (dst.hasAny(VariantBits::Hard)) {
        if (dst.hasAny(VariantBits::Object)) {
          assert(dst.u.o != nullptr);
          dst.u.o->hardRelease();
        } else if (dst.hasAny(VariantBits::String | VariantBits::Memory)) {
          if (dst.u.s != nullptr) {
            dst.u.s->hardRelease();
          }
        }
      }
    }
    static const IMemory* acquireFallbackString(const char* utf8, size_t bytes);
  };
}
