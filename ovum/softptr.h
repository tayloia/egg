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
    virtual IBasket* softSetBasket(IBasket* value) const override {
      // Make sure we're not transferred directly between baskets
      auto* old = this->basket;
      assert((old == nullptr) || (value == nullptr) || (old == value));
      this->basket = value;
      return old;
    }
    virtual bool softLink(const ICollectable& target) const override {
      // Make sure we're not transferring directly between baskets
      auto targetBasket = target.softGetBasket();
      if (targetBasket == nullptr) {
        if (this->basket == nullptr) {
          return false;
        }
        this->basket->take(target);
        return true;
      }
      if (this->basket == nullptr) {
        targetBasket->take(*this);
        return true;
      }
      return this->basket == targetBasket;
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
    virtual IBasket* softSetBasket(IBasket* basket) const override {
      // We cannot be added to a basket
      // WUBBLE assert(basket == nullptr);
      (void)basket;
      return nullptr;
    }
    virtual bool softLink(const ICollectable&) const override {
      // Should never be called
      return true; // WUBBLE false?
    }
    virtual void softVisitLinks(const ICollectable::Visitor&) const override {
    }
  };

  template<typename T>
  class SoftPtr {
    SoftPtr(const SoftPtr&) = delete;
    SoftPtr& operator=(const SoftPtr&) = delete;
  private:
    T* ptr;
  public:
    SoftPtr() : ptr(nullptr) {
    }
    T* get() const {
      return this->ptr;
    }
    void set(ICollectable& container, const T* target) {
      if (target != nullptr) {
        if (!container.softLink(*target)) {
          throw std::logic_error("Soft link basket condition violation");
        }
      }
      this->ptr = const_cast<T*>(target);
    }
    void visit(const ICollectable::Visitor& visitor) const {
      if (this->ptr != nullptr) {
        visitor(*this->ptr);
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
    static T* hardAcquire(const T* ptr) {
      if (ptr != nullptr) {
        // See https://stackoverflow.com/a/15572442
        return ptr->template hardAcquire<T>();
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
