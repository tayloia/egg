namespace egg::ovum {
  class BasketFactory {
  public:
    static HardPtr<IBasket> createBasket(IAllocator& allocator);
  };

  struct AllocatorDefaultPolicy {
    static void* memalloc(size_t bytes, size_t alignment) {
      return os::memory::alloc(bytes, alignment);
    }
    static size_t memsize(void* allocated, size_t alignment) {
      return os::memory::size(allocated, alignment);
    }
    static void memfree(void* allocated, size_t alignment) {
      os::memory::free(allocated, alignment);
    }
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
      auto bytes = POLICY::memsize(allocated, alignment);
      this->deallocatedBlocks.add(1);
      this->deallocatedBytes.add(bytes);
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
}
