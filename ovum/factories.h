namespace egg::ovum {
  class MemoryFactory;

  template<typename T>
  class HardReferenceCounted : public T {
    HardReferenceCounted(const HardReferenceCounted&) = delete;
    HardReferenceCounted& operator=(const HardReferenceCounted&) = delete;
  private:
    IAllocator& allocator;
    mutable Atomic<int64_t> atomic; // signed so we can detect underflows
  public:
    explicit HardReferenceCounted(IAllocator& allocator, int64_t atomic = 0) : allocator(allocator), atomic(atomic) {
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

  // We want this code generated from the header
#if defined(_MSC_VER)
  // Microsoft's run-time
  struct AllocatorDefaultPolicy {
    inline static void* memalloc(size_t bytes, size_t alignment) {
      return _aligned_malloc(bytes, alignment);
    }
    inline static size_t memsize(void* allocated, size_t alignment) {
      return _aligned_msize(allocated, alignment, 0);
    }
    inline static void memfree(void* allocated, size_t) {
      return _aligned_free(allocated);
    }
  };
#else
  // Linux run-time
  struct AllocatorDefaultPolicy {
    inline static void* memalloc(size_t bytes, size_t alignment) {
      return aligned_alloc(alignment, bytes); // note switched order
    }
    inline static size_t memsize(void* allocated, size_t) {
      return malloc_usable_size(allocated);
    }
    inline static void memfree(void* allocated, size_t) {
      return free(allocated);
    }
  };
#endif

  // This often lives high up on the machine stack, so we need to know the class layout
  template<typename POLICY>
  class AllocatorWithPolicy : public IAllocator {
    AllocatorWithPolicy(const AllocatorWithPolicy&) = delete;
    AllocatorWithPolicy& operator=(const AllocatorWithPolicy&) = delete;
  private:
    Atomic<uint64_t> allocatedBlocks;
    Atomic<uint64_t> allocatedBytes;
    Atomic<uint64_t> deallocatedBlocks;
    Atomic<uint64_t> deallocatedBytes;
  public:
    AllocatorWithPolicy() : allocatedBlocks(0), allocatedBytes(0), deallocatedBlocks(0), deallocatedBytes(0) {}
    virtual void* allocate(size_t bytes, size_t alignment) {
      auto* allocated = POLICY::memalloc(bytes, alignment);
      assert(allocated != nullptr);
      this->allocatedBlocks.add(1);
      this->allocatedBytes.add(POLICY::memsize(allocated, alignment));
      return allocated;
    }
    virtual void deallocate(void* allocated, size_t alignment) {
      assert(allocated != nullptr);
      this->deallocatedBlocks.add(1);
      this->deallocatedBytes.add(POLICY::memsize(allocated, alignment));
      POLICY::memfree(allocated, alignment);
    }
    virtual bool statistics(Statistics& out) const {
      out.totalBlocksAllocated = this->allocatedBlocks.get();
      out.totalBytesAllocated = this->allocatedBytes.get();
      out.currentBlocksAllocated = diff(out.totalBlocksAllocated, this->deallocatedBlocks.get());
      out.currentBytesAllocated = diff(out.totalBytesAllocated, this->deallocatedBytes.get());
      return true;
    }
  private:
    static uint64_t diff(uint64_t a, uint64_t b) {
      // Disallow negative differences due to concurrency timing issues
      assert(a >= b);
      return (a < b) ? 0 : (a - b);
    }
  };
  using AllocatorDefault = AllocatorWithPolicy<AllocatorDefaultPolicy>;

  class MemoryContiguous : public HardReferenceCounted<IMemory> {
    MemoryContiguous(const MemoryContiguous&) = delete;
    MemoryContiguous& operator=(const MemoryContiguous&) = delete;
  private:
    size_t size;
    IMemory::Tag usertag;
  public:
    MemoryContiguous(IAllocator& allocator, size_t size, IMemory::Tag usertag)
      : HardReferenceCounted(allocator), size(size), usertag(usertag) {
    }
    virtual const Byte* begin() const override {
      return this->base();
    }
    virtual const Byte* end() const override {
      return this->base() + this->size;
    }
    virtual IMemory::Tag tag() const override {
      return this->usertag;
    }
    Byte* base() const {
      return const_cast<Byte*>(reinterpret_cast<const Byte*>(this + 1));
    }
  };

  class MemoryMutable {
    friend class MemoryFactory;
  private:
    Memory memory; // null only after being built
    explicit MemoryMutable(const IMemory* memory) : memory(memory) {
      // Only constructed by MemoryFactory
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
    Memory build() {
      assert(this->memory != nullptr);
      return std::move(this->memory);
    }
  };

  class MemoryFactory {
  public:
    static Memory createEmpty();
    static Memory createImmutable(IAllocator& allocator, const void* src, size_t bytes, IMemory::Tag tag = IMemory::Tag{ 0 });
    static MemoryMutable createMutable(IAllocator& allocator, size_t bytes, IMemory::Tag tag = IMemory::Tag{ 0 });
  };

  class MemoryBuilder {
    MemoryBuilder(const MemoryBuilder&) = delete;
    MemoryBuilder& operator=(const MemoryBuilder&) = delete;
  private:
    struct Chunk {
      Memory memory;
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
    Memory build();
    void reset();
  };

  class StringFactory {
  public:
    static String fromUTF8(IAllocator& allocator, const Byte* begin, const Byte* end);
    static String fromUTF8(IAllocator& allocator, const void* utf8, size_t bytes) {
      auto begin = static_cast<const Byte*>(utf8);
      assert(begin != nullptr);
      return fromUTF8(allocator, begin, begin + bytes);
    }
    static String fromUTF8(IAllocator& allocator, const std::string& utf8) {
      return fromUTF8(allocator, utf8.data(), utf8.size());
    }
    template<size_t N>
    static String fromUTF8(IAllocator& allocator, const char (&utf8)[N]) {
      return fromUTF8(allocator, utf8, N - 1);
    }
  };
}
