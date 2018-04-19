namespace egg::gc {
  template<typename T>
  class Atomic {
    Atomic(const Atomic&) = delete;
    Atomic& operator=(const Atomic&) = delete;
  private:
    std::atomic<T> atom;
  public:
    Atomic() : atom(0) {
    }
    T get() {
      return this->atom;
    }
    T add(T offset) {
      return this->atom.fetch_add(offset);
    }
  };

  class ReferenceCount {
    ReferenceCount(const ReferenceCount&) = delete;
    ReferenceCount& operator=(const ReferenceCount&) = delete;
  protected:
    mutable Atomic<int64_t> atomic;
  public:
    inline ReferenceCount() {}
    inline size_t acquire() const {
      auto after = this->atomic.add(1) + 1;
      assert(after > 0);
      return static_cast<size_t>(after);
    }
    inline size_t release() const {
      auto after = this->atomic.add(-1) - 1;
      assert(after >= 0);
      return static_cast<size_t>(after);
    }
  };

  template<class T>
  class NotReferenceCounted : public T {
  public:
    virtual T* acquireHard() const override {
      return const_cast<NotReferenceCounted*>(this);
    }
    virtual void releaseHard() const override {
    }
  };

  template<class T>
  class HardReferenceCounted : public T {
    HardReferenceCounted(const HardReferenceCounted&) = delete;
    HardReferenceCounted& operator=(const HardReferenceCounted&) = delete;
  private:
    ReferenceCount hard;
  public:
    inline HardReferenceCounted() {}
    virtual T* acquireHard() const override {
      this->hard.acquire();
      return const_cast<HardReferenceCounted*>(this);
    }
    virtual void releaseHard() const override {
      if (this->hard.release() == 0) {
        delete this;
      }
    }
  };

  template<class T>
  class HardRef {
    HardRef() = delete;
  private:
    T* ptr;
  public:
    inline explicit HardRef(const T* rhs) : ptr(rhs->acquireHard()) {
      assert(this->ptr != nullptr);
    }
    inline HardRef(const HardRef& rhs) : ptr(rhs.acquireHard()) {
      assert(this->ptr != nullptr);
    }
    HardRef& operator=(const HardRef& rhs) {
      assert(this->ptr != nullptr);
      this->set(rhs.get());
      return *this;
    }
    ~HardRef() {
      assert(this->ptr != nullptr);
      this->ptr->releaseHard();
    }
    T* get() const {
      assert(this->ptr != nullptr);
      return this->ptr;
    }
    T* acquireHard() const {
      assert(this->ptr != nullptr);
      return this->ptr->acquireHard();
    }
    void set(T* rhs) {
      assert(rhs != nullptr);
      auto* old = this->ptr;
      assert(old != nullptr);
      this->ptr = rhs->acquireHard();
      old->releaseHard();
    }
    T& operator*() const {
      return *this->ptr;
    }
    T* operator->() const {
      return this->ptr;
    }

    template<typename U, typename... ARGS>
    static HardRef<T> make(ARGS&&... args) {
      // Use perfect forwarding to the constructor
      return HardRef<T>(new U(std::forward<ARGS>(args)...));
    }
  };
}
