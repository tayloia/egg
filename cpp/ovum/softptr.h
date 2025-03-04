namespace egg::ovum {
  template<typename T>
  class SoftReferenceCounted : public HardReferenceCounted<T> {
    SoftReferenceCounted(const SoftReferenceCounted&) = delete;
    SoftReferenceCounted& operator=(const SoftReferenceCounted&) = delete;
  protected:
    mutable IBasket* basket;
  public:
    template<typename... ARGS>
    explicit SoftReferenceCounted(ARGS&&... args)
      : HardReferenceCounted<T>(std::forward<ARGS>(args)...),
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
  class SoftReferenceCountedAllocator : public SoftReferenceCounted<T> {
    SoftReferenceCountedAllocator(const SoftReferenceCountedAllocator&) = delete;
    SoftReferenceCountedAllocator& operator=(const SoftReferenceCountedAllocator&) = delete;
  protected:
    IAllocator& allocator;
  public:
    template<typename... ARGS>
    explicit SoftReferenceCountedAllocator(IAllocator& allocator, ARGS&&... args)
      : SoftReferenceCounted<T>(std::forward<ARGS>(args)...),
        allocator(allocator) {
    }
  protected:
    virtual void hardDestroy() const override {
      assert(this->atomic.get() == 0);
      this->allocator.destroy(this);
    }
  };

  template<typename T>
  class SoftReferenceCountedNone : public T {
    SoftReferenceCountedNone(const SoftReferenceCountedNone&) = delete;
    SoftReferenceCountedNone& operator=(const SoftReferenceCountedNone&) = delete;
  public:
    template<typename... ARGS>
    SoftReferenceCountedNone(ARGS&&... args)
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
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // Nothing to do
    }
  };

  template<typename T, typename U = ICollectable>
  class SoftPtr {
    friend class IVM;
    friend class SoftValue;
    SoftPtr(const SoftPtr&) = delete;
    SoftPtr& operator=(const SoftPtr&) = delete;
  protected:
    U* ptr;
    explicit SoftPtr(T* created)
      : ptr(created) {
      // Used by SoftValue construction
    }
  public:
    SoftPtr()
      : ptr(nullptr) {
    }
    T* get() const {
      return static_cast<T*>(this->ptr);
    }
    void visit(ICollectable::IVisitor& visitor) const {
      if (this->ptr != nullptr) {
        visitor.visit(*this->ptr);
      }
    }
    T& operator*() const {
      assert(this->ptr != nullptr);
      return *static_cast<T*>(this->ptr);
    }
    T* operator->() const {
      assert(this->ptr != nullptr);
      return static_cast<T*>(this->ptr);
    }
    bool operator==(std::nullptr_t) const {
      return this->ptr == nullptr;
    }
    bool operator!=(std::nullptr_t) const {
      return this->ptr != nullptr;
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
