namespace egg::ovum {
  class Variant;
  class VariantFactory;
  class VariantSoft;

#define EGG_VM_BASAL_ENUM(name, value) name = 1 << value,
  enum class Basal {
    None = 0,
    EGG_VM_BASAL(EGG_VM_BASAL_ENUM)
    Arithmetic = Int | Float,
    Any = Bool | Int | Float | String | Object
  };
#undef EGG_VM_BASAL_ENUM
  inline Basal operator|(Basal lhs, Basal rhs) {
    return egg::ovum::Bits::set(lhs, rhs);
  }

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
    return egg::ovum::Bits::set(lhs, rhs);
  }

  class VariantKind {
  private:
    VariantBits kind;
  public:
    explicit VariantKind(VariantBits bits) : kind(bits) {
    }
    bool hasOne(VariantBits mask) const {
      return Bits::hasOneSet(this->kind, mask);
    }
    bool hasAny(VariantBits mask) const {
      return Bits::hasAnySet(this->kind, mask);
    }
    bool hasAll(VariantBits mask) const {
      return Bits::hasAllSet(this->kind, mask);
    }
    bool is(VariantBits value) const {
      return this->kind == value;
    }
    VariantBits getKind() const {
      return this->kind;
    }
  protected:
    void setKind(VariantBits bits) {
      this->kind = bits;
    }
    void swapKind(VariantKind& other) {
      std::swap(this->kind, other.kind);
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
    friend class VariantSoft;
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
    Variant(Variant&& rhs) : VariantKind(rhs.getKind()) { // WIBBLE noexcept
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
    Variant(bool value) : VariantKind(VariantBits::Bool) {
      this->u.b = value;
    }
    bool getBool() const {
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
    // Float (support automatic promotion of 32-bit IEEE)
    Variant(float value) : VariantKind(VariantBits::Float) {
      this->u.f = value;
    }
    Variant(double value) : VariantKind(VariantBits::Float) {
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
      assert(this->hasOne(VariantBits::Pointer | VariantBits::Indirect));
      if (this->hasAny(VariantBits::Hard)) {
        this->u.p = HardPtr<IVariantSoft>::hardAcquire(&value);
      } else {
        this->u.p = &value;
      }
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
        if (src.hasAny(VariantBits::Pointer | VariantBits::Indirect)) {
          dst.u.p = HardPtr<IVariantSoft>::hardAcquire(src.u.p);
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
        } else {
          assert(dst.u.o != nullptr);
          dst.u.p->hardRelease();
        }
      }
    }
    static const IMemory* acquireFallbackString(const char* utf8, size_t bytes);
  };
}
