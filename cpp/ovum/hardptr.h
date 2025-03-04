namespace egg::ovum {
  template<typename T>
  class HardReferenceCounted : public T {
    HardReferenceCounted(const HardReferenceCounted&) = delete;
    HardReferenceCounted& operator=(const HardReferenceCounted&) = delete;
  protected:
    mutable Atomic<int64_t> atomic; // signed so we can detect underflows
  public:
    template<typename... ARGS>
    explicit HardReferenceCounted(ARGS&&... args)
      : T(std::forward<ARGS>(args)...),
        atomic(0) {
      assert(this->atomic.get() == 0);
    }
    virtual ~HardReferenceCounted() {
      // Make sure our reference count reached zero
      assert(this->atomic.get() == 0);
    }
    virtual T* hardAcquire() const override {
      auto count = this->atomic.increment();
      assert(count > 0);
      (void)count;
      return const_cast<T*>(static_cast<const T*>(this));
    }
    virtual void hardRelease() const override {
      auto count = this->atomic.decrement();
      assert(count >= 0);
      if (count == 0) {
        this->hardDestroy();
      }
    }
  protected:
    virtual void hardDestroy() const = 0;
  };

  template<typename T>
  class HardReferenceCountedAllocator : public HardReferenceCounted<T> {
    HardReferenceCountedAllocator(const HardReferenceCountedAllocator&) = delete;
    HardReferenceCountedAllocator& operator=(const HardReferenceCountedAllocator&) = delete;
  protected:
    IAllocator& allocator;
  public:
    template<typename... ARGS>
    explicit HardReferenceCountedAllocator(IAllocator& allocator, ARGS&&... args)
      : HardReferenceCounted<T>(std::forward<ARGS>(args)...), allocator(allocator) {
    }
  protected:
    virtual void hardDestroy() const override {
      assert(this->atomic.get() == 0);
      this->allocator.destroy(this);
    }
  };

  template<typename T>
  class HardReferenceCountedNone : public T {
    HardReferenceCountedNone(const HardReferenceCountedNone&) = delete;
    HardReferenceCountedNone& operator=(const HardReferenceCountedNone&) = delete;
  public:
    template<typename... ARGS>
    HardReferenceCountedNone(ARGS&&... args)
      : T(std::forward<ARGS>(args)...) {
    }
    virtual T* hardAcquire() const override {
      return const_cast<T*>(static_cast<const T*>(this));
    }
    virtual void hardRelease() const override {
      // Do nothing
    }
  };

  template<typename T>
  class HardPtr {
  private:
    Atomic<T*> ptr;
  public:
    HardPtr(std::nullptr_t = nullptr) : ptr(nullptr) {
    }
    explicit HardPtr(const T* rhs) : ptr(HardPtr::hardAcquire(rhs)) {
    }
    HardPtr(const HardPtr& rhs) : ptr(rhs.hardAcquire()) {
    }
    HardPtr(HardPtr&& rhs) noexcept : ptr(rhs.ptr.exchange(nullptr)) {
    }
    template<typename U>
    HardPtr(const HardPtr<U>& rhs) : ptr(rhs.hardAcquire()) {
    }
    HardPtr& operator=(std::nullptr_t) {
      HardPtr::hardRelease(this->ptr.exchange(nullptr));
      return *this;
    }
    HardPtr& operator=(const HardPtr& rhs) {
      this->set(rhs.get());
      return *this;
    }
    HardPtr& operator=(HardPtr&& rhs) noexcept {
      HardPtr::hardRelease(this->ptr.exchange(rhs.ptr.exchange(nullptr)));
      return *this;
    }
    template<typename U>
    HardPtr& operator=(const HardPtr<U>& rhs) {
      this->set(rhs.get());
      return *this;
    }
    ~HardPtr() {
      HardPtr::hardRelease(this->ptr.get());
    }
    T* hardAcquire() const {
      return HardPtr::hardAcquire(this->ptr.get());
    }
    T* get() const {
      return this->ptr.get();
    }
    void set(T* rhs) {
      HardPtr::hardRelease(this->ptr.exchange(HardPtr::hardAcquire(rhs)));
    }
    T& operator*() const {
      auto* p = this->ptr.get();
      assert(p != nullptr);
      return *p;
    }
    T* operator->() const {
      auto* p = this->ptr.get();
      assert(p != nullptr);
      return p;
    }
    bool operator==(std::nullptr_t) const {
      return this->ptr.get() == nullptr;
    }
    bool operator!=(std::nullptr_t) const {
      return this->ptr.get() != nullptr;
    }
    static T* hardAcquire(const T* ptr) {
      if (ptr != nullptr) {
        return static_cast<T*>(ptr->hardAcquire());
      }
      return nullptr;
    }
    static void hardRelease(const T* ptr) {
      if (ptr != nullptr) {
        ptr->hardRelease();
      }
    }
    static void hardSwap(HardPtr& lhs, HardPtr& rhs) {
      // TODO truly atomic?
      lhs.ptr.set(rhs.ptr.exchange(lhs.ptr.get()));
    }
  };
  template<typename T>
  bool operator==(std::nullptr_t, const HardPtr<T>& ptr) {
    // Yoda equality comparison used by GoogleTest
    return ptr == nullptr;
  }
  template<typename T>
  bool operator!=(std::nullptr_t, const HardPtr<T>& ptr) {
    // Yoda inequality comparison used by GoogleTest
    return ptr != nullptr;
  }
}
