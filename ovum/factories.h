namespace egg::ovum {
  class MemoryFactory;

  template<typename T>
  class HardReferenceCounted : public T {
    HardReferenceCounted(const HardReferenceCounted&) = delete;
    HardReferenceCounted& operator=(const HardReferenceCounted&) = delete;
  protected:
    IAllocator& allocator;
    mutable Atomic atomic;
  public:
    explicit HardReferenceCounted(IAllocator& allocator, Atomic::Underlying atomic = 0) : allocator(allocator), atomic(atomic) {
    }
    virtual T* hardAcquire() const override {
      this->atomic.increment();
      return const_cast<T*>(static_cast<const T*>(this));
    }
    virtual void hardRelease() const override {
      if (this->atomic.decrement() <= 0) {
        this->allocator.destroy(this);
      }
    }
  };

  template<typename T>
  class NotReferenceCounted : public T {
    NotReferenceCounted(const NotReferenceCounted&) = delete;
    NotReferenceCounted& operator=(const NotReferenceCounted&) = delete;
  public:
    NotReferenceCounted() {}
    virtual T* hardAcquire() const override {
      return const_cast<T*>(static_cast<const T*>(this));
    }
    virtual void hardRelease() const override {
      // Do nothing
    }
  };

  class AllocatorDefault : public IAllocator {
  public:
    virtual void* allocate(size_t bytes, size_t alignment) {
      return ::operator new(bytes, std::align_val_t(alignment));
    }
    virtual void deallocate(void* allocated, size_t alignment) {
      ::operator delete(allocated, std::align_val_t(alignment));
    }
  };

  class MemoryMutable {
    friend class MemoryFactory;
  private:
    IMemoryPtr memory; // null only after being baked
    explicit MemoryMutable(const IMemory* memory) : memory(memory) {
      // Only constructed by MemoryFactory;
    }
  public:
    Byte* begin() {
      assert(this->memory != nullptr);
      return const_cast<Byte*>(this->memory->begin());
    }
    Byte* end() {
      assert(this->memory != nullptr);
      return const_cast<Byte*>(this->memory->end());
    }
    size_t bytes() const {
      assert(this->memory != nullptr);
      return this->memory->bytes();
    }
    IMemoryPtr bake() {
      assert(this->memory != nullptr);
      IMemoryPtr detached;
      this->memory.swap(detached);
      assert(detached != nullptr);
      return detached;
    }
  };

  class MemoryFactory {
  public:
    static MemoryMutable create(IAllocator& allocator, size_t bytes);
  };

  class MemoryBuilder {
    MemoryBuilder(const MemoryBuilder&) = delete;
    MemoryBuilder& operator=(const MemoryBuilder&) = delete;
  private:
    struct Chunk {
      IMemoryPtr memory;
      const Byte* base;
      size_t bytes;
      Chunk(const IMemory* memory, const Byte* base, size_t bytes) : memory(memory), base(base), bytes(bytes) {
        assert(base != nullptr);
        assert(bytes > 0);
      }
    };
    IAllocator& allocator;
    std::list<Chunk> chunks;
    size_t bytes;
  public:
    explicit MemoryBuilder(IAllocator& allocator);
    void add(const Byte* begin, const Byte* end);
    void add(const IMemory& memory);
    IMemoryPtr bake();
    void reset();
  };
}
