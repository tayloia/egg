namespace egg::ovum {
  using Bool = bool;
  using Int = int64_t;
  using Float = double;
  class Variant;
  class VariantFactory;
  class VariantSoft;

  template<class T>
  class HardPtr {
  private:
    T * ptr;
  public:
    HardPtr() : ptr(nullptr) {
    }
    explicit HardPtr(const T* rhs) : ptr(HardPtr::hardAcquire(rhs)) {
    }
    HardPtr(const HardPtr& rhs) : ptr(rhs.hardAcquire()) {
    }
    HardPtr(HardPtr&& rhs) : ptr(rhs.get()) {
      rhs.ptr = nullptr;
    }
    template<typename U>
    HardPtr(const HardPtr<U>& rhs) : ptr(rhs.hardAcquire()) {
    }
    HardPtr& operator=(nullptr_t) {
      this->ptr->hardRelease();
      this->ptr = nullptr;
      return *this;
    }
    HardPtr& operator=(const HardPtr& rhs) {
      this->set(rhs.get());
      return *this;
    }
    HardPtr& operator=(HardPtr&& rhs) {
      if (this->ptr != nullptr) {
        this->ptr->hardRelease();
      }
      this->ptr = rhs.get();
      rhs.ptr = nullptr;
      return *this;
    }
    template<typename U>
    HardPtr& operator=(const HardPtr<U>& rhs) {
      this->set(rhs.get());
      return *this;
    }
    ~HardPtr() {
      if (this->ptr != nullptr) {
        this->ptr->hardRelease();
      }
    }
    T* hardAcquire() const {
      return HardPtr::hardAcquire(this->ptr);
    }
    T* get() const {
      return this->ptr;
    }
    void set(T* rhs) {
      auto* old = this->ptr;
      this->ptr = HardPtr::hardAcquire(rhs);
      if (old != nullptr) {
        old->hardRelease();
      }
    }
    T& operator*() const {
      assert(this->ptr != nullptr);
      return *this->ptr;
    }
    T* operator->() const {
      assert(this->ptr != nullptr);
      return this->ptr;
    }
    bool operator==(nullptr_t) const {
      return this->ptr == nullptr;
    }
    bool operator!=(nullptr_t) const {
      return this->ptr != nullptr;
    }
    static T* hardAcquire(const T* ptr) {
      if (ptr != nullptr) {
        return static_cast<T*>(ptr->hardAcquire());
      }
      return nullptr;
    }
  };
  template<typename T>
  bool operator==(nullptr_t, const HardPtr<T>& ptr) {
    // Yoda equality comparison used by GoogleTest
    return ptr == nullptr;
  }
  template<typename T>
  bool operator!=(nullptr_t, const HardPtr<T>& ptr) {
    // Yoda inequality comparison used by GoogleTest
    return ptr != nullptr;
  }

  using Memory = HardPtr<const IMemory>;

  class String : public Memory {
  public:
    explicit String(const IMemory* rhs = nullptr) : Memory(rhs) {
    }
    String(const char* rhs); // implicit; fallback to factory
    String(const std::string& rhs); // implicit; fallback to factory
    String(const std::string& rhs, size_t codepoints); // fallback to factory
    size_t length() const {
      auto* memory = this->get();
      if (memory == nullptr) {
        return 0;
      }
      return size_t(memory->tag().u);
    }
    std::string toUTF8() const {
      auto* memory = this->get();
      if (memory == nullptr) {
        return std::string();
      }
      return std::string(memory->begin(), memory->end());
    }
    int compare(const String& rhs) const {
      auto* rmemory = rhs.get();
      if (rmemory == nullptr) {
        // If the right side is an empty string, it all boils down to our length
        return (this->length() == 0) ? 0 : 1;
      }
      return this->compare(rmemory->begin(), rmemory->end());
    }
    int compare(const uint8_t* rbegin, const uint8_t* rend) const {
      assert(rbegin != nullptr);
      assert(rend >= rbegin);
      auto* lmemory = this->get();
      if (lmemory == nullptr) {
        // If we're empty, the result depends solely on the length of the right side
        return (rbegin == rend) ? 0 : -1;
      }
      return String::compare(lmemory->begin(), lmemory->end(), rbegin, rend);
    }
    bool operator<(const String& rhs) const {
      return this->compare(rhs) < 0;
    }
    static int compare(const uint8_t* lbegin, const uint8_t* lend, const uint8_t* rbegin, const uint8_t* rend) {
      assert(lbegin != nullptr);
      assert(lend >= lbegin);
      assert(rbegin != nullptr);
      assert(rend >= rbegin);
      auto lsize = size_t(lend - lbegin);
      auto rsize = size_t(rend - rbegin);
      if (lsize < rsize) {
        auto cmp = std::memcmp(lbegin, rbegin, lsize);
        return (cmp == 0) ? -1 : cmp;
      }
      if (lsize > rsize) {
        auto cmp = std::memcmp(lbegin, rbegin, rsize);
        return (cmp == 0) ? 1 : cmp;
      }
      return std::memcmp(lbegin, rbegin, lsize);
    }
  };

  class Object : public HardPtr<IObject> {
  public:
    explicit Object(const IObject& rhs) : HardPtr(&rhs) {
      assert(this->get() != nullptr);
    }
  };

  using Module = HardPtr<IModule>;

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
    bool hasOne(VariantBits mask) const {
      auto underlying = static_cast<Underlying>(mask);
      auto masked = this->bits & underlying;
      return ((masked & (masked - 1)) == 0) && (masked != 0);
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
