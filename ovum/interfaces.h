namespace egg::ovum {
  using Bool = bool;
  using Int = int64_t;
  using Float = double;

  enum Opcode {
#define EGG_VM_OPCODES_ENUM(opcode, minbyte, minargs, maxargs, text) opcode = minbyte,
    EGG_VM_OPCODES(EGG_VM_OPCODES_ENUM)
#undef EGG_VM_OPCODES_ENUM
    OPCODE_reserved = -1
  };

  enum Section {
#define EGG_VM_SECTIONS_ENUM(section, value) section = value,
    EGG_VM_SECTIONS(EGG_VM_SECTIONS_ENUM)
#undef EGG_VM_SECTIONS_ENUM
  };

  class IHardAcquireRelease {
  public:
    virtual ~IHardAcquireRelease() {}
    virtual IHardAcquireRelease* hardAcquire() const = 0;
    virtual void hardRelease() const = 0;
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
    virtual class IBasket* softSetBasket(IBasket* basket) = 0;
    virtual void softVisitLinks(const Visitor& visitor) const = 0;
  };

  class IBasket {
  public:
    virtual ~IBasket() {}
    virtual void take(ICollectable& collectable) = 0;
    virtual void drop(ICollectable& collectable) = 0;
    virtual size_t collect() = 0;
    virtual size_t purge() = 0;
  };

  class IObject : public ICollectable {
  };
}
