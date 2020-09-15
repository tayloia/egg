namespace egg::ovum {
  class Memory : public HardPtr<const IMemory> {
  public:
    Memory() = default;
    explicit Memory(const IMemory* rhs) : HardPtr(rhs) {
    }
    static bool equals(const IMemory* lhs, const IMemory* rhs);
    bool validate() const;
  };

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
    uint8_t* begin() {
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

  class MemoryFactory {
  public:
    static Memory createEmpty();
    static Memory createImmutable(IAllocator& allocator, const void* src, size_t bytes, IMemory::Tag tag = IMemory::Tag{ 0 });
    static MemoryMutable createMutable(IAllocator& allocator, size_t bytes, IMemory::Tag tag = IMemory::Tag{ 0 });
  };
}
