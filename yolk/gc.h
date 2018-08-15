namespace egg::gc {
  class Collectable;
  template<class T> class SoftRef;

  template<class T> using HardReferenceCounted = egg::ovum::HardReferenceCounted<T>;
  template<class T> using HardRef = egg::ovum::HardPtr<T>;

  class Basket {
    Basket(const Basket&) = delete;
    Basket& operator=(const Basket&) = delete;
    class Head;
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
      Link()
        : from(nullptr), to(nullptr), next(nullptr) {
      }
      Link(Link&& rhs) noexcept = default;
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
  private:
    egg::ovum::IAllocator& allocator;
    Head* head;
  public:
    explicit Basket(egg::ovum::IAllocator& allocator);
    ~Basket();
    void add(Collectable& collectable); // Must have a hard reference already
    bool validate() const; // Debugging only
    void visitCollectables(IVisitor& visitor);
    void visitRoots(IVisitor& visitor);
    void visitGarbage(IVisitor& visitor);
    void visitPurge(IVisitor& visitor);
    size_t collectGarbage();
    size_t purgeAll();

    template<typename T, typename... ARGS>
    HardRef<T> make(ARGS&&... args) {
      // Use perfect forwarding to the constructor and then add to the basket
      auto hard{ this->allocator.make<T>(std::forward<ARGS>(args)...) };
      this->add(*hard);
      return hard;
    }
  };

  class Collectable : public HardReferenceCounted<egg::ovum::IHardAcquireRelease> {
    Collectable(const Collectable&) = delete;
    Collectable& operator=(const Collectable&) = delete;
    friend class Basket;
    template<class T> friend class SoftRef;
  private:
    Basket* basket;
    Collectable* prevInBasket;
    Collectable* nextInBasket;
    Basket::Link* ownedLinks;
  protected:
    explicit Collectable(egg::ovum::IAllocator& allocator)
      : HardReferenceCounted(allocator, 0),
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
    template<class T>
    void linkSoft(SoftRef<T>& link, T* pointee) {
      // Type-safe link setting
      if (pointee == nullptr) {
        link.reset();
      } else {
        // Take a temporary hard link to this container so garbage collection doesn't find a false positive
        HardRef<Collectable> ref{ this };
        link.set(*ref, *pointee);
      }
    }
    template<class T>
    void linkSoft(SoftRef<T>& link, const HardRef<T>& pointee) {
      // Take a temporary hard link to this container so garbage collection doesn't find a false positive
      HardRef<Collectable> ref{ this };
      link.set(*ref, *pointee);
    }
    int64_t hardGet() const {
      // WIBBLE
      return this->atomic.get();
    }
  };

  template<class T>
  class SoftRef {
    SoftRef(const SoftRef&) = delete;
    SoftRef& operator=(const SoftRef&) = delete;
  private:
    egg::ovum::IAllocator& allocator;
    Basket::Link link;
  public:
    explicit SoftRef(egg::ovum::IAllocator& allocator)
      : allocator(allocator), link() {
    }
    SoftRef(SoftRef&& rhs) noexcept = default;
    SoftRef(Collectable& from, T* to)
      : allocator(from.allocator), link(from, to) {
    }
    void destroy() {
      this->allocator.destroy(this);
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
    HardRef<T> harden() const {
      return HardRef<T>(this->get());
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
    static std::shared_ptr<Basket> createBasket(egg::ovum::IAllocator& allocator);
  };
}
