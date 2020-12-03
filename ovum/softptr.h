namespace egg::ovum {
  template<typename T>
  class SoftReferenceCounted : public HardReferenceCounted<T> {
    SoftReferenceCounted(const SoftReferenceCounted&) = delete;
    SoftReferenceCounted& operator=(const SoftReferenceCounted&) = delete;
  protected:
    mutable IBasket* basket;
  public:
    explicit SoftReferenceCounted(IAllocator& allocator)
      : HardReferenceCounted<T>(allocator, 0),
        basket(nullptr) {
    }
    virtual ~SoftReferenceCounted() override {
      // Make sure we're no longer a member of a basket
      assert(this->basket == nullptr);
    }
    virtual bool validate() const override {
      return this->atomic.get() >= 0;
    }
    virtual bool softIsRoot() const override {
      // We're a root if there's a hard reference in addition to ours
      assert(this->basket != nullptr);
      return this->atomic.get() > 1;
    }
    virtual IBasket* softGetBasket() const override {
      // Fetch our current basket, if any
      return this->basket;
    }
    virtual ICollectable::SetBasketResult softSetBasket(IBasket* desired) const override {
      // Make sure we're not transferred directly between baskets
      auto* before = this->basket;
      if (desired == before) {
        return ICollectable::SetBasketResult::Unaltered;
      }
      if ((desired == nullptr) || (before == nullptr)) {
        this->basket = desired;
        return ICollectable::SetBasketResult::Altered;
      }
      return ICollectable::SetBasketResult::Failed;
    }
  };

  template<typename T>
  class NotSoftReferenceCounted : public T {
    NotSoftReferenceCounted(const NotSoftReferenceCounted&) = delete;
    NotSoftReferenceCounted& operator=(const NotSoftReferenceCounted&) = delete;
  public:
    template<typename... ARGS>
    NotSoftReferenceCounted(ARGS&&... args)
      : T(std::forward<ARGS>(args)...) {
    }
    virtual T* hardAcquire() const override {
      return const_cast<T*>(static_cast<const T*>(this));
    }
    virtual void hardRelease() const override {
      // Do nothing
    }
    virtual bool validate() const override {
      // Nothing to validate
      return true;
    }
    virtual bool softIsRoot() const override {
      // We cannot be destroyed so we must be a root
      return true;
    }
    virtual IBasket* softGetBasket() const override {
      // We belong to no basket
      return nullptr;
    }
    virtual ICollectable::SetBasketResult softSetBasket(IBasket*) const override {
      // We cannot be added to a basket
      return ICollectable::SetBasketResult::Exempt;
    }
    virtual void softVisit(const ICollectable::Visitor&) const override {
      // Nothing to do
    }
  };

  template<typename T>
  class SoftPtr {
    SoftPtr(const SoftPtr&) = delete;
    SoftPtr& operator=(const SoftPtr&) = delete;
  private:
    T* ptr;
  public:
    SoftPtr(std::nullptr_t = nullptr) : ptr(nullptr) { // implicit
    }
    SoftPtr(IBasket& basket, const T* target) : ptr(SoftPtr::softAcquire(basket, target)) {
    }
    SoftPtr(SoftPtr&& rhs) noexcept : ptr(rhs.ptr) {
      rhs.ptr = nullptr;
    }
    T* get() const {
      return this->ptr;
    }
    void set(IBasket& basket, const T* target) {
      this->ptr = SoftPtr::softAcquire(basket, target);
    }
    void visit(const ICollectable::Visitor& visitor) const {
      if (this->ptr != nullptr) {
        this->ptr->softVisit(visitor);
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
    bool operator==(std::nullptr_t) const {
      return this->ptr == nullptr;
    }
    bool operator!=(std::nullptr_t) const {
      return this->ptr != nullptr;
    }
    static T* hardAcquire(const T* target) {
      if (target != nullptr) {
        // See https://stackoverflow.com/a/15572442
        return target->template hardAcquire<T>();
      }
      return nullptr;
    }
    static T* softAcquire(IBasket& basket, const T* target) {
      if (target != nullptr) {
        return static_cast<T*>(basket.take(*target));
      }
      return nullptr;
    }
  };
  template<typename T>
  bool operator==(std::nullptr_t, const SoftPtr<T>& ptr) {
    // Yoda equality comparison used by GoogleTest
    return ptr == nullptr;
  }
  template<typename T>
  bool operator!=(std::nullptr_t, const SoftPtr<T>& ptr) {
    // Yoda inequality comparison used by GoogleTest
    return ptr != nullptr;
  }
}
