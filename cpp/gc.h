namespace egg::gc {
  class Collectable;
  template<class T> class SoftRef;

  template<typename T>
  class Atomic {
    Atomic(const Atomic&) = delete;
    Atomic& operator=(const Atomic&) = delete;
  private:
    std::atomic<T> atom;
  public:
    explicit Atomic(T init) : atom(init) {
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
    explicit ReferenceCount(int64_t init) : atomic(init) {}
    size_t acquire() const {
      auto after = this->atomic.add(1) + 1;
      assert(after > 0);
      return static_cast<size_t>(after);
    }
    size_t release() const {
      auto after = this->atomic.add(-1) - 1;
      assert(after >= 0);
      return static_cast<size_t>(after);
    }
  };

  template<class T>
  class NotReferenceCounted : public T {
  public:
    NotReferenceCounted() : T() {}
    template<typename... ARGS>
    explicit NotReferenceCounted(ARGS&&... args) : T(std::forward<ARGS>(args)...) {}
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
    template<typename... ARGS>
    explicit HardReferenceCounted(int64_t rc, ARGS&&... args) : T(std::forward<ARGS>(args)...), hard(rc) {}
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
    explicit HardRef(const T* rhs) : ptr(HardRef::acquireHard(rhs)) {
      assert(this->ptr != nullptr);
    }
    HardRef(HardRef&& rhs) : ptr(rhs.ptr) {
      assert(this->ptr != nullptr);
    }
    HardRef(const HardRef& rhs) : ptr(rhs.acquireHard()) {
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
      return HardRef::acquireHard(this->ptr);
    }
    void set(T* rhs) {
      auto* old = this->ptr;
      assert(old != nullptr);
      this->ptr = HardRef::acquireHard(rhs);
      old->releaseHard();
    }
    T& operator*() const {
      return *this->ptr;
    }
    T* operator->() const {
      return this->ptr;
    }
    static T* acquireHard(const T* ptr) {
      assert(ptr != nullptr);
      return static_cast<T*>(ptr->acquireHard());
    }
    template<typename U, typename... ARGS>
    static HardRef<T> make(ARGS&&... args) {
      // Use perfect forwarding to the constructor
      return HardRef<T>(new U(std::forward<ARGS>(args)...));
    }
  };

  class Basket {
    Basket(const Basket&) = delete;
    Basket& operator=(const Basket&) = delete;
  public:
    class Link {
      friend class Basket;
      Link(const Link&) = delete;
      Link& operator=(const Link&) = delete;
    private:
      Collectable* from;
      Collectable* to;
      Link* next; // chain of links belonging to 'from'
    public:
      Link(Basket& basket, Collectable& from, Collectable& to);
      Link(Link&& rhs) = default;
      ~Link();
      Link** findOrigin() const;
      Collectable* get() const;
      void set(Collectable& pointee);
    };
    class IVisitor {
    public:
      virtual void visit(Collectable& collectable) = 0;
    };
    class Visitor : public IVisitor {
      typedef std::function<void(Collectable&)> Action;
    private:
      Action action;
    public:
      explicit Visitor(Action action) : action(action) {
        assert(action != nullptr);
      }
      virtual ~Visitor() {}
      virtual void visit(Collectable& collectable) override {
        this->action(collectable);
      }
    };
  private:
    class Head;
    Head* head;
  public:
    Basket();
    ~Basket();
    void add(Collectable& collectable, bool root);
    void remove(Collectable& collectable);
    void visitCollectables(IVisitor& visitor);
    void visitRoots(IVisitor& visitor);
    void visitGarbage(IVisitor& visitor);
    void visitPurge(IVisitor& visitor);
  };

  class Collectable {
    Collectable(const Collectable&) = delete;
    Collectable& operator=(const Collectable&) = delete;
    friend class Basket;
  private:
    Basket* basket;
    Collectable* prevInBasket;
    Collectable* nextInBasket;
    Basket::Link* ownedLinks;
  protected:
    Collectable()
      : basket(nullptr),
        prevInBasket(nullptr),
        nextInBasket(nullptr),
        ownedLinks(nullptr) {
    }
  public:
    virtual ~Collectable() {
      // Make sure we don't own any active links by the time we're destroyed
      assert(this->ownedLinks == nullptr);
    }
    Basket* getCollectableBasket() const {
      return this->basket;
    }
  };

  template<class T>
  class SoftRef {
    SoftRef() = delete;
    SoftRef(const SoftRef&) = delete;
    SoftRef& operator=(const SoftRef&) = delete;
  private:
    Basket::Link link;
  public:
    SoftRef(Basket& basket, Collectable& from, T& to)
      : link(basket, from, to) {
    }
    SoftRef(SoftRef&& rhs) = default;
    T* get() const {
      auto* to = this->link.get();
      assert(to != nullptr);
      return static_cast<T*>(to);
    }
    void set(T& to) {
      this->link.set(to);
    }
  };

  class BasketFactory {
  public:
    static std::shared_ptr<Basket> createBasket();
  };
}
