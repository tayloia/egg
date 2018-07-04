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
    T get() const {
      return this->atom.load();
    }
    T add(T arg) {
      return this->atom.fetch_add(arg);
    }
  };

  class ReferenceCount {
    ReferenceCount(const ReferenceCount&) = delete;
    ReferenceCount& operator=(const ReferenceCount&) = delete;
  protected:
    mutable Atomic<int64_t> atomic;
  public:
    explicit ReferenceCount(int64_t init) : atomic(init) {}
    uint64_t acquire() const {
      auto after = this->atomic.add(1) + 1;
      assert(after > 0);
      return static_cast<uint64_t>(after);
    }
    uint64_t release() const {
      auto after = this->atomic.add(-1) - 1;
      assert(after >= 0);
      return static_cast<uint64_t>(after);
    }
    uint64_t get() const {
      return static_cast<uint64_t>(this->atomic.get());
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
    explicit HardReferenceCounted(int64_t rc, ARGS&&... args) : T(std::forward<ARGS>(args)...), hard(rc) {
    }
    ~HardReferenceCounted() {
      assert(this->hard.get() == 0);
    }
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
    HardRef(const HardRef& rhs) : ptr(rhs.acquireHard()) {
      assert(this->ptr != nullptr);
    }
    template<typename U>
    HardRef(const HardRef<U>& rhs) : ptr(rhs.acquireHard()) {
      assert(this->ptr != nullptr);
    }
    HardRef& operator=(const HardRef& rhs) {
      assert(this->ptr != nullptr);
      this->set(rhs.get());
      return *this;
    }
    template<typename U>
    HardRef& operator=(const HardRef<U>& rhs) {
      assert(this->ptr != nullptr);
      this->set(rhs.get());
      return *this;
    }
    ~HardRef() {
      assert(this->ptr != nullptr);
      this->ptr->releaseHard();
    }
    T* acquireHard() const {
      return HardRef::acquireHard(this->ptr);
    }
    T* get() const {
      assert(this->ptr != nullptr);
      return this->ptr;
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
    template<typename U = T, typename... ARGS>
    static HardRef<T> make(ARGS&&... args) {
      // Use perfect forwarding to the constructor
      return HardRef<T>(new U(std::forward<ARGS>(args)...));
    }
  };

  class Basket {
    Basket(const Basket&) = delete;
    Basket& operator=(const Basket&) = delete;
    class Head;
  public:
    class Link {
      friend class Basket;
      template<class T> friend class SoftRef; // WIBBLE REMOVE REMOVE REMOVE
      Link(const Link&) = delete;
      Link& operator=(const Link&) = delete;
    private:
      Collectable* from;
      Collectable* to;
      Link* next; // chain of links belonging to 'from'
    public:
      Link()
        : from(nullptr), to(nullptr), next(nullptr) {
      }
      Link(Link&& rhs) = default;
      Link(Collectable& from, Collectable* to);
      ~Link() {
        this->reset();
      }
      Link** findOrigin() const;
      Collectable* get() const;
      void set(Collectable& from, Collectable& to);
      void reset();
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
    Head* head;
  public:
    Basket();
    ~Basket();
    void add(Collectable& collectable); // Must have a hard reference already
    bool validate() const; // Debugging only
    void visitCollectables(IVisitor& visitor);
    void visitRoots(IVisitor& visitor);
    void visitGarbage(IVisitor& visitor);
    void visitPurge(IVisitor& visitor);
    template<typename T, typename... ARGS>
    HardRef<T> make(ARGS&&... args) {
      // Use perfect forwarding to the constructor and then add to the basket
      HardRef<T> ref{ new T(std::forward<ARGS>(args)...) };
      this->add(*ref);
      return ref;
    }
  };

  class Collectable {
    Collectable(const Collectable&) = delete;
    Collectable& operator=(const Collectable&) = delete;
    friend class Basket;
  private:
    ReferenceCount hard;
    Basket* basket;
    Collectable* prevInBasket;
    Collectable* nextInBasket;
    Basket::Link* ownedLinks;
  protected:
    Collectable()
      : hard(0),
        basket(nullptr),
        prevInBasket(nullptr),
        nextInBasket(nullptr),
        ownedLinks(nullptr) {
    }
  public:
    virtual ~Collectable() {
      // Make sure we don't own any active links by the time we're destroyed
      assert(this->ownedLinks == nullptr);
    }
    virtual Collectable* acquireHard() const {
      this->hard.acquire();
      return const_cast<Collectable*>(this);
    }
    virtual void releaseHard() const {
      if (this->hard.release() == 0) {
        delete this;
      }
    }
    template<class T>
    void softLink(SoftRef<T>& link, T* pointee) { // WIBBLE still needed?
      // Type-safe link setting
      // OPTIMIZE
      if (pointee == nullptr) {
        link.reset();
      } else {
        HardRef<Collectable> ref{ this }; // WIBBLE
        link.set(*ref, *pointee);
      }
    }
    Basket* softBasket() const { // WIBBLE remove?
      // This may be null
      return this->basket;
    }
  };

  template<class T>
  class SoftRef {
    SoftRef(const SoftRef&) = delete;
    SoftRef& operator=(const SoftRef&) = delete;
  private:
    Basket::Link link;
  public:
    SoftRef() = default;
    SoftRef(SoftRef&& rhs) = default;
    SoftRef(Collectable& from, T* to)
      : link(from, to) {
    }
    T* get() const {
      auto* to = this->link.get();
      return static_cast<T*>(to);
    }
    void set(Collectable& from, T& to) {
      this->link.set(from, to);
    }
    void reset() {
      this->link.reset();
    }
    T& operator*() const {
      return *this->get();
    }
    T* operator->() const {
      return this->get();
    }
  };

  class BasketFactory {
  public:
    static std::shared_ptr<Basket> createBasket();
  };
}
