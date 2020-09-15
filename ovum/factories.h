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
    static String fromASCIIZ(IAllocator& allocator, const char* asciiz);
  };

  class ValueFactory {
  public:
    static Value createVoid();
    static Value createNull();
    static Value createBool(Bool value);
    static Value createInt(IAllocator& allocator, Int value);
    static Value createFloat(IAllocator& allocator, Float value);
    static Value createString(IAllocator& allocator, const String& value);
    static Value createObject(IAllocator& allocator, const Object& value);
    static Value createMemory(IAllocator& allocator, const Memory& value);
    static Value createPointer(IAllocator& allocator, const Value& pointee);
    static Value createFlowControl(ValueFlags flags);
    static Value createFlowControl(IAllocator& allocator, ValueFlags flags, const Value& value);

    // Overloaded without implicit promotion
    template<typename T>
    static Value createValue(IAllocator& allocator, T value) = delete;
    static Value createValue(IAllocator&, std::nullptr_t) {
      return createNull();
    }
    static Value createValue(IAllocator&, bool value) {
      return createBool(value);
    }
    static Value createValue(IAllocator& allocator, int32_t value) {
      return createInt(allocator, value);
    }
    static Value createValue(IAllocator& allocator, int64_t value) {
      return createInt(allocator, value);
    }
    static Value createValue(IAllocator& allocator, float value) {
      return createFloat(allocator, value);
    }
    static Value createValue(IAllocator& allocator, double value) {
      return createFloat(allocator, value);
    }
    static Value createValue(IAllocator& allocator, const String& value) {
      return createString(allocator, value);
    }
    static Value createValue(IAllocator& allocator, const std::string& value) {
      return createString(allocator, StringFactory::fromUTF8(allocator, value));
    }
    static Value createValue(IAllocator& allocator, const char* value) {
      if (value == nullptr) {
        return createNull();
      }
      return createString(allocator, StringFactory::fromASCIIZ(allocator, value));
    }
    static Value createValue(IAllocator& allocator, const Object& value) {
      return createObject(allocator, value);
    }
  };

  class BasketFactory {
  public:
    static Basket createBasket(IAllocator& allocator);
  };
}

// We want this code generated from the header
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  // Microsoft-style run-time
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
  // Platform-independent
  inline void* egg::ovum::AllocatorDefaultPolicy::memalloc(size_t bytes, size_t alignment) {
    auto total = bytes + std::max(alignment, sizeof(size_t) * 2);
    auto allocated = static_cast<char*>(std::malloc(total));
    auto unaligned = allocated + total - bytes;
    auto aligned = unaligned - reinterpret_cast<uintptr_t>(unaligned) % uintptr_t(alignment);
    auto preamble = reinterpret_cast<size_t*>(aligned);
    preamble[-2] = size_t(aligned - allocated);
    preamble[-1] = bytes;
    return aligned;
  }
  inline size_t egg::ovum::AllocatorDefaultPolicy::memsize(void* allocated, size_t) {
    return reinterpret_cast<size_t*>(allocated)[-1];
  }
  inline void egg::ovum::AllocatorDefaultPolicy::memfree(void* allocated, size_t) {
    auto padding = reinterpret_cast<size_t*>(allocated)[-2];
    return free(reinterpret_cast<char*>(allocated) - padding);
  }
#endif
