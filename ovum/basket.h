namespace egg::ovum {
  using Basket = HardPtr<IBasket>;

  class BasketFactory {
  public:
    static Basket createBasket(IAllocator& allocator);
  };

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
