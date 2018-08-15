namespace egg::ovum {
  using Bool = bool;
  using Int = int64_t;
  using Float = double;

  template<typename T>
  class Atomic {
    Atomic(Atomic&) = delete;
    Atomic& operator=(Atomic&) = delete;
  public:
    using Underlying = T;
  private:
    std::atomic<Underlying> atomic;
  public:
    explicit Atomic(Underlying value) : atomic(value) {
    }
    Underlying get() const {
      // Get the current value
      return std::atomic_load(&this->atomic);
    }
    Underlying add(Underlying value) {
      // Return the value BEFORE the addition
      return std::atomic_fetch_add(&this->atomic, value);
    }
    Underlying increment() {
      // The result should be strictly positive
      auto result = this->add(1) + 1;
      assert(result > 0);
      return result;
    }
    Underlying decrement() {
      // The result should not be negative
      auto result = this->add(-1) - 1;
      assert(result >= 0);
      return result;
    }
  };

  using ReadWriteMutex = std::shared_mutex;
  using WriteLock = std::unique_lock<ReadWriteMutex>;
  using ReadLock = std::shared_lock<ReadWriteMutex>;

  template<typename T>
  class HardReferenceCounted : public T {
    HardReferenceCounted(const HardReferenceCounted&) = delete;
    HardReferenceCounted& operator=(const HardReferenceCounted&) = delete;
  protected:
    IAllocator& allocator;
    mutable Atomic<int64_t> atomic; // signed so we can detect underflows
  public:
    template<typename... ARGS>
    HardReferenceCounted(IAllocator& allocator, int64_t atomic, ARGS&&... args)
      : T(std::forward<ARGS>(args)...), allocator(allocator), atomic(atomic) {
    }
    virtual ~HardReferenceCounted() {
      // Make sure our reference count reached zero
      assert(this->atomic.get() == 0);
    }
    virtual T* hardAcquireBase() const override {
      this->atomic.increment();
      return const_cast<T*>(static_cast<const T*>(this));
    }
    virtual void hardRelease() const override {
      if (this->atomic.decrement() <= 0) {
        this->allocator.destroy(this);
      }
    }
  };

  template<typename T>
  class SoftReferenceCounted : public HardReferenceCounted<T> {
    SoftReferenceCounted(const SoftReferenceCounted&) = delete;
    SoftReferenceCounted& operator=(const SoftReferenceCounted&) = delete;
  private:
    IBasket* basket;
  public:
    explicit SoftReferenceCounted(IAllocator& allocator)
      : HardReferenceCounted<T>(allocator, 0),
        basket(nullptr) {
    }
    virtual ~SoftReferenceCounted() override {
      // Make sure we're no longer a member of a basket
      assert(this->basket == nullptr);
    }
    virtual bool softIsRoot() const override {
      // We're a root if there's a hard reference in addition to ours
      return this->atomic.get() > 1;
    }
    virtual IBasket* softSetBasket(IBasket* value) override {
      auto* old = this->basket;
      this->basket = value;
      return old;
    }
  };

  template<typename T>
  class NotReferenceCounted : public T {
  public:
    template<typename... ARGS>
    NotReferenceCounted(ARGS&&... args)
      : T(std::forward<ARGS>(args)...) {
    }
    virtual T* hardAcquireBase() const override {
      return const_cast<T*>(static_cast<const T*>(this));
    }
    virtual void hardRelease() const override {
      // Do nothing
    }
  };

  template<typename T>
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
        // See https://stackoverflow.com/a/15572442
        return ptr->template hardAcquire<T>();
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

  template<typename T, typename... ARGS>
  HardPtr<T> IAllocator::make(ARGS&&... args) {
    // Use perfect forwarding to in-place new
    void* allocated = this->allocate(sizeof(T), alignof(T));
    assert(allocated != nullptr);
    return HardPtr<T>(new(allocated) T(*this, std::forward<ARGS>(args)...));
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
  };

  class Object : public HardPtr<IObject> {
  public:
    explicit Object(const IObject& rhs) : HardPtr(&rhs) {
      assert(this->get() != nullptr);
    }
  };

  // Helper for converting IEEE to/from mantissa/exponents
  struct MantissaExponent {
    static constexpr int64_t ExponentNaN = 1;
    static constexpr int64_t ExponentPositiveInfinity = 2;
    static constexpr int64_t ExponentNegativeInfinity = -2;
    int64_t mantissa;
    int64_t exponent;
    void fromFloat(Float f);
    Float toFloat() const;
  };

  struct UTF8 {
    inline static size_t sizeFromLeadByte(uint8_t lead) {
      // Fetch the size in bytes of a codepoint given just the lead byte
      if (lead < 0x80) {
        return 1;
      } else if (lead < 0xC0) {
        return SIZE_MAX; // Continuation byte
      } else if (lead < 0xE0) {
        return 2;
      } else if (lead < 0xF0) {
        return 3;
      } else if (lead < 0xF8) {
        return 4;
      }
      return SIZE_MAX;
    }
    inline static size_t measure(const uint8_t* p, const uint8_t* q) {
      // Return SIZE_MAX if this is not valid UTF-8, otherwise the codepoint count
      size_t count = 0;
      while (p < q) {
        if (*p < 0x80) {
          // Fast code path for ASCII
          p++;
        } else {
          auto remaining = size_t(q - p);
          auto length = UTF8::sizeFromLeadByte(*p);
          if (length > remaining) {
            // Truncated
            return SIZE_MAX;
          }
          assert(length > 1);
          for (size_t i = 1; i < length; ++i) {
            if ((p[i] & 0xC0) != 0x80) {
              // Bad continuation byte
              return SIZE_MAX;
            }
          }
          p += length;
        }
        count++;
      }
      return count;
    }
  };
}
