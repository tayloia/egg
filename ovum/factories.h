namespace egg::ovum {
  class MemoryFactory;

  struct AllocatorDefaultPolicy {
    inline static void* memalloc(size_t bytes, size_t alignment);
    inline static size_t memsize(void* allocated, size_t alignment);
    inline static void memfree(void* allocated, size_t);
  };

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
    virtual void* allocate(size_t bytes, size_t alignment) override {
      auto* allocated = POLICY::memalloc(bytes, alignment);
      assert(allocated != nullptr);
      this->allocatedBlocks.add(1);
      this->allocatedBytes.add(POLICY::memsize(allocated, alignment));
      return allocated;
    }
    virtual void deallocate(void* allocated, size_t alignment) override {
      assert(allocated != nullptr);
      this->deallocatedBlocks.add(1);
      this->deallocatedBytes.add(POLICY::memsize(allocated, alignment));
      POLICY::memfree(allocated, alignment);
    }
    virtual bool statistics(Statistics& out) const override {
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
      : HardReferenceCounted(allocator, 0), size(size), usertag(usertag) {
    }
    virtual const uint8_t* begin() const override {
      return this->base();
    }
    virtual const uint8_t* end() const override {
      return this->base() + this->size;
    }
    virtual IMemory::Tag tag() const override {
      return this->usertag;
    }
    uint8_t* base() const {
      return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(this + 1));
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
    uint8_t * begin() {
      assert(this->memory != nullptr);
      return const_cast<uint8_t*>(this->memory->begin());
    }
    uint8_t* end() {
      assert(this->memory != nullptr);
      return const_cast<uint8_t*>(this->memory->end());
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
      const uint8_t* base;
      size_t bytes;
      Chunk(const IMemory* memory, const uint8_t* base, size_t bytes) : memory(memory), base(base), bytes(bytes) {
        assert(base != nullptr);
        assert(bytes > 0);
      }
    };
    IAllocator& allocator;
    std::list<Chunk> chunks;
    size_t bytes;
  public:
    explicit MemoryBuilder(IAllocator& allocator);
    void add(const uint8_t* begin, const uint8_t* end);
    void add(const IMemory& memory);
    Memory build();
    void reset();
  };

  class StringBuilder {
    StringBuilder(const StringBuilder&) = delete;
    StringBuilder& operator=(const StringBuilder&) = delete;
  private:
    std::stringstream ss;
  public:
    StringBuilder() {
    }
    template<typename T>
    StringBuilder& add(T value) {
      this->ss << value;
      return *this;
    }
    template<typename T, typename... ARGS>
    StringBuilder& add(T value, ARGS&&... args) {
      return this->add(value).add(std::forward<ARGS>(args)...);
    }
    bool empty() const {
      return this->ss.rdbuf()->in_avail() == 0;
    }
    std::string toUTF8() const {
      return this->ss.str();
    }
    String str() const;

    template<typename... ARGS>
    static String concat(ARGS&&... args) {
      return StringBuilder().add(std::forward<ARGS>(args)...).str();
    }
  };

  class StringFactory {
  public:
    static String fromCodePoint(IAllocator& allocator, char32_t codepoint);
    static String fromUTF8(IAllocator& allocator, const uint8_t* begin, const uint8_t* end, size_t codepoints = SIZE_MAX);
    static String fromUTF8(IAllocator& allocator, const void* utf8, size_t bytes) {
      auto begin = static_cast<const uint8_t*>(utf8);
      assert(begin != nullptr);
      return fromUTF8(allocator, begin, begin + bytes);
    }
    static String fromUTF8(IAllocator& allocator, const std::string& utf8) {
      return fromUTF8(allocator, utf8.data(), utf8.size());
    }
  };

  class ObjectFactory {
  public:
    static Object createVanillaArray(IAllocator& allocator, size_t size = 0);
    static Object createVanillaException(IAllocator& allocator, const LocationSource& location, const String& message);
    static Object createVanillaKeyValue(IAllocator& allocator, IBasket& basket, const Variant& key, const Variant& value);
    static Object createVanillaObject(IAllocator& allocator);
    template<typename T, typename... ARGS>
    static Object create(IAllocator& allocator, ARGS&&... args) {
      // Use perfect forwarding
      return Object(*allocator.make<T>(std::forward<ARGS>(args)...));
    }
  };

  class VariantFactory {
  public:
    static HardPtr<IVariantSoft> createVariantSoft(IAllocator& allocator, IBasket& basket, Variant&& value);
    static Variant createBuiltinAssert(IAllocator& allocator);
    static Variant createBuiltinPrint(IAllocator& allocator);
    static Variant createBuiltinString(IAllocator& allocator);
    static Variant createBuiltinType(IAllocator& allocator);
    static Variant createStringProperty(IAllocator& allocator, const String& string, const String& property);
    static Variant createException(Variant&& value);
    template<typename... ARGS>
    static Variant createException(IAllocator& allocator, const LocationSource& location, ARGS&&... args) {
      // Use perfect forwarding
      return VariantFactory::createException(ObjectFactory::createVanillaException(allocator, location, StringBuilder::concat(std::forward<ARGS>(args)...)));
    }
    template<typename T, typename... ARGS>
    static Variant createObject(IAllocator& allocator, ARGS&&... args) {
      // Use perfect forwarding
      return Variant(ObjectFactory::create<T>(allocator, std::forward<ARGS>(args)...));
    }
  };

  class BasketFactory {
  public:
    static Basket createBasket(IAllocator& allocator);
  };
}

// We want this code generated from the header
#if defined(_MSC_VER)
  // Microsoft's run-time
  inline void* egg::ovum::AllocatorDefaultPolicy::memalloc(size_t bytes, size_t alignment) {
    return _aligned_malloc(bytes, alignment);
  }
  inline size_t egg::ovum::AllocatorDefaultPolicy::memsize(void* allocated, size_t alignment) {
    return _aligned_msize(allocated, alignment, 0);
  }
  inline void egg::ovum::AllocatorDefaultPolicy::memfree(void* allocated, size_t) {
    return _aligned_free(allocated);
  }
#else
  // For malloc_usable_size()
  #include <malloc.h>

  // Linux run-time
  inline void* egg::ovum::AllocatorDefaultPolicy::memalloc(size_t bytes, size_t alignment) {
    return aligned_alloc(alignment, bytes); // note switched order
  }
  inline size_t egg::ovum::AllocatorDefaultPolicy::memsize(void* allocated, size_t) {
    return malloc_usable_size(allocated);
  }
  inline void egg::ovum::AllocatorDefaultPolicy::memfree(void* allocated, size_t) {
    return free(allocated);
  }
#endif
