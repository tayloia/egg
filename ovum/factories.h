namespace egg::ovum {
  class AllocatorDefault : public IAllocator {
  public:
    virtual void* allocate(size_t bytes, size_t alignment) {
      return ::operator new(bytes, std::align_val_t(alignment));
    }
    virtual void deallocate(void* allocated, size_t alignment) {
      ::operator delete(allocated, std::align_val_t(alignment));
    }
  };
}
