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

  class ReferenceCounted {
    ReferenceCounted(const ReferenceCounted&) = delete;
    ReferenceCounted& operator=(const ReferenceCounted&) = delete;
  protected:
    mutable Atomic<int64_t> atomic;
  public:
    inline ReferenceCounted() {}
    virtual ~ReferenceCounted() {}
    inline size_t acquireHard() const {
      auto after = this->atomic.add(1) + 1;
      assert(after > 0);
      return static_cast<size_t>(after);
    }
    inline size_t releaseHard() const {
      auto after = this->atomic.add(-1) -1;
      assert(after >= 0);
      if (after == 0) {
        delete this;
      }
      return static_cast<size_t>(after);
    }
  };

  template<class T>
  class HardRef {
    HardRef() = delete;
  private:
    T* ptr;
  public:
    explicit HardRef(T* ptr) : ptr(ptr) {
      assert(this->ptr != nullptr);
      this->ptr->acquireHard();
    }
    template<class U>
    explicit HardRef(HardRef<U> rhs) : ptr(rhs.acquire()) {
      assert(this->ptr != nullptr);
    }
    ~HardRef() {
      assert(this->ptr != nullptr);
      this->ptr->releaseHard();
    }
    T* get() const {
      assert(this->ptr != nullptr);
      return this->ptr;
    }
    T* acquire() const {
      assert(this->ptr != nullptr);
      this->ptr->acquireHard();
      return this->ptr;
    }
    void set(T* rhs) {
      assert(rhs != nullptr);
      rhs->acquireHard();
      assert(this->ptr != nullptr);
      this->ptr->releaseHard();
      this->ptr = rhs;
    }
    T& operator*() const {
      return *this->ptr;
    }
    T* operator->() const {
      return this->ptr;
    }
  };
}
