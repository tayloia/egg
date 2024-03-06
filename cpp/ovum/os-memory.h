namespace egg::ovum::os::memory {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  // Microsoft-style run-time
  inline void* alloc(size_t bytes, size_t alignment) {
    return _aligned_malloc(bytes, alignment);
  }
  inline size_t size(void* allocated, size_t alignment) {
    return _aligned_msize(allocated, alignment, 0);
  }
  inline void free(void* allocated, size_t) {
    return _aligned_free(allocated);
  }
#else
  // Platform-independent
  inline void* alloc(size_t bytes, size_t alignment) {
    auto total = bytes + std::max(alignment, sizeof(size_t) * 2);
    auto allocated = static_cast<char*>(std::malloc(total));
    auto unaligned = allocated + total - bytes;
    auto aligned = unaligned - reinterpret_cast<uintptr_t>(unaligned) % uintptr_t(alignment);
    auto preamble = reinterpret_cast<size_t*>(aligned);
    preamble[-2] = size_t(aligned - allocated);
    preamble[-1] = bytes;
    return aligned;
  }
  inline size_t size(void* allocated, size_t) {
    return reinterpret_cast<size_t*>(allocated)[-1];
  }
  inline void free(void* allocated, size_t) {
    auto padding = reinterpret_cast<size_t*>(allocated)[-2];
    return std::free(reinterpret_cast<char*>(allocated) - padding);
  }
#endif
}
