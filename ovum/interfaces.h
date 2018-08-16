namespace egg::ovum {
  class IBasket;
  template<typename T> class HardPtr;

  class IHardAcquireRelease {
  public:
    virtual ~IHardAcquireRelease() {}
    virtual IHardAcquireRelease* hardAcquireBase() const = 0;
    virtual void hardRelease() const = 0;
    template<typename T>
    T* hardAcquire() const {
      return static_cast<T*>(this->hardAcquireBase());
    }
  };

  class IAllocator {
  public:
    struct Statistics {
      uint64_t totalBlocksAllocated;
      uint64_t totalBytesAllocated;
      uint64_t currentBlocksAllocated;
      uint64_t currentBytesAllocated;
    };

    virtual void* allocate(size_t bytes, size_t alignment) = 0;
    virtual void deallocate(void* allocated, size_t alignment) = 0;
    virtual bool statistics(Statistics& out) const = 0;

    template<typename T, typename... ARGS>
    T* create(size_t extra, ARGS&&... args) {
      // Use perfect forwarding to in-place new
      size_t bytes = sizeof(T) + extra;
      void* allocated = this->allocate(bytes, alignof(T));
      assert(allocated != nullptr);
      return new(allocated) T(std::forward<ARGS>(args)...);
    }
    template<typename T>
    void destroy(const T* allocated) {
      assert(allocated != nullptr);
      allocated->~T();
      this->deallocate(const_cast<T*>(allocated), alignof(T));
    }

    template<typename T, typename... ARGS>
    inline HardPtr<T> make(ARGS&&... args);
  };

  class IMemory : public IHardAcquireRelease {
  public:
    union Tag {
      uintptr_t u;
      void* p;
    };
    virtual const uint8_t* begin() const = 0;
    virtual const uint8_t* end() const = 0;
    virtual Tag tag() const = 0;

    size_t bytes() const {
      return size_t(this->end() - this->begin());
    }
  };

  class ICollectable : public IHardAcquireRelease {
  public:
    using Visitor = std::function<void(ICollectable& target)>;
    virtual bool softIsRoot() const = 0;
    virtual IBasket* softGetBasket() const = 0;
    virtual IBasket* softSetBasket(IBasket* basket) = 0;
    virtual bool softLink(ICollectable& target) = 0;
    virtual void softVisitLinks(const Visitor& visitor) const = 0;
  };

  class IBasket : public IHardAcquireRelease {
  public:
    struct Statistics {
      uint64_t currentBlocksOwned;
      uint64_t currentBytesOwned;
    };

    virtual void take(ICollectable& collectable) = 0;
    virtual void drop(ICollectable& collectable) = 0;
    virtual size_t collect() = 0;
    virtual size_t purge() = 0;
    virtual bool statistics(Statistics& out) const = 0;
  };

  class IObject : public ICollectable {
  };
}
