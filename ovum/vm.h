namespace egg::ovum {
  using Bool = bool;
  using Byte = uint8_t;
  using Int = uint64_t;
  using Float = double;

  class IHardAcquireRelease {
  public:
    virtual ~IHardAcquireRelease() {}
    virtual void hardAcquire() const = 0;
    virtual void hardRelease() const = 0;
  };

  template<class T>
  class HardPtr {
  private:
    T* ptr;
  public:
    HardPtr() : ptr(nullptr) {
    }
    explicit HardPtr(const T* rhs) : ptr(HardPtr::hardAcquire(rhs)) {
    }
    HardPtr(const HardPtr& rhs) : ptr(rhs.hardAcquire()) {
    }
    template<typename U>
    HardPtr(const HardPtr<U>& rhs) : ptr(rhs.hardAcquire()) {
    }
    HardPtr& operator=(const HardPtr& rhs) {
      this->set(rhs.get());
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
    static T* hardAcquire(const T* ptr) {
      if (ptr != nullptr) {
        return static_cast<T*>(ptr->hardAcquire());
      }
      return nullptr;
    }
    template<typename U = T, typename... ARGS>
    static HardPtr<T> make(ARGS&&... args) {
      // Use perfect forwarding to the constructor
      return HardPtr<T>(new U(std::forward<ARGS>(args)...));
    }
  };

  class IAllocator {
  public:
    virtual void* allocate(size_t bytes, size_t alignment) = 0;
    virtual void deallocate(void* allocated, size_t alignment) = 0;

    template<typename T, typename... ARGS>
    T* create(size_t extra, ARGS&&... args) {
      // Use perfect forwarding to in-place new
      size_t bytes = sizeof(T) + extra;
      void* memory = static_cast<T*>(this->allocate(bytes, alignof(T)));
      assert(memory != nullptr);
      return new(memory) T(std::forward<ARGS>(args)...);
    }
    template<typename T>
    void destroy(T* allocated) {
      this->deallocate(allocated, alignof(T));
    }
  };

  class IMemory : public IHardAcquireRelease {
  public:
    virtual const Byte* begin() const = 0;
    virtual const Byte* end() const = 0;

    size_t bytes() const {
      return size_t(this->end() - this->begin());
    }
  };

  class IString : public IHardAcquireRelease {
  public:
    virtual size_t length() const = 0;
    virtual int32_t codePointAt(size_t index) const = 0;
    virtual size_t bytesUTF8(size_t codePointOffset = 0, size_t codePointLength = SIZE_MAX) const = 0;
    virtual HardPtr<IMemory> memoryUTF8(size_t codePointOffset = 0, size_t codePointLength = SIZE_MAX) const = 0;
  };

  class ICollectable : public IHardAcquireRelease {
  public:
    using Visitor = std::function<void(const ICollectable& from, const ICollectable& to)>;
    virtual void visitSoftLinks(const ICollectable& from, const ICollectable& to) const = 0;
  };

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

  class VariantKind {
  public:
    using Underlying = std::underlying_type_t<VariantBits>;
  private:
    Underlying bits;
  public:
    explicit VariantKind(VariantBits bits) : bits(static_cast<Underlying>(bits)) {
    }
    bool hasAny(VariantBits mask) const {
      auto underlying = static_cast<Underlying>(mask);
      return (this->bits & underlying) != 0;
    }
    bool hasAll(VariantBits mask) const {
      auto underlying = static_cast<Underlying>(mask);
      return (this->bits & underlying) == underlying;
    }
  };
  inline VariantBits operator|(VariantBits lhs, VariantBits rhs) {
    return static_cast<VariantBits>(static_cast<VariantKind::Underlying>(lhs) | static_cast<VariantKind::Underlying>(rhs));
  }

  class Variant {
  private:
    VariantKind kind;
    union {
      Bool b;
      Int i;
      Float f;
    };
  public:
    Variant() : kind(VariantBits::Void) {
    }
  };
}
