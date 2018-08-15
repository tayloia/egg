#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  int compareUTF8(const uint8_t* lbegin, size_t lsize, const uint8_t* rbegin, size_t rsize) {
    assert(lbegin != nullptr);
    assert(rbegin != nullptr);
    if (lsize < rsize) {
      auto cmp = std::memcmp(lbegin, rbegin, lsize);
      return (cmp == 0) ? -1 : cmp;
    }
    if (lsize > rsize) {
      auto cmp = std::memcmp(lbegin, rbegin, rsize);
      return (cmp == 0) ? 1 : cmp;
    }
    return std::memcmp(lbegin, rbegin, lsize);
  }

  const IMemory* createContiguous(IAllocator& allocator, const uint8_t* utf8, size_t bytes, size_t codepoints = SIZE_MAX) {
    // TODO detect malformed/overlong/etc
    if ((utf8 == nullptr) || (bytes == 0)) {
      return nullptr;
    }
    if (codepoints == SIZE_MAX) {
      codepoints = UTF8::measure(utf8, utf8 + bytes);
    }
    if (codepoints > bytes) {
      throw std::invalid_argument("String: Invalid UTF-8 input data");
    }
    auto* memory = allocator.create<MemoryContiguous>(bytes, allocator, bytes, IMemory::Tag{ codepoints });
    assert(memory != nullptr);
    std::memcpy(memory->base(), utf8, bytes);
    return memory;
  }

  class StringFallbackAllocator final : public IAllocator {
    StringFallbackAllocator(const StringFallbackAllocator&) = delete;
    StringFallbackAllocator& operator=(const StringFallbackAllocator&) = delete;
  private:
    Atomic<int64_t> atomic;
  public:
    StringFallbackAllocator() : atomic(0) {
    }
    ~StringFallbackAllocator() {
      // Make sure all our strings have been destroyed when the process exits
      assert(this->atomic.get() == 0);
    }
    virtual void* allocate(size_t bytes, size_t alignment) {
      this->atomic.increment();
      return AllocatorDefaultPolicy::memalloc(bytes, alignment);
    }
    virtual void deallocate(void* allocated, size_t alignment) {
      assert(allocated != nullptr);
      this->atomic.decrement();
      AllocatorDefaultPolicy::memfree(allocated, alignment);
    }
    virtual bool statistics(Statistics&) const {
      return false;
    }
    static const IMemory* createString(const char* utf8, size_t bytes, size_t codepoints = SIZE_MAX) {
      static StringFallbackAllocator allocator;
      return createContiguous(allocator, reinterpret_cast<const uint8_t*>(utf8), bytes, codepoints);
    }
    static const IMemory* createString(const char* utf8) {
      return (utf8 == nullptr) ? nullptr : StringFallbackAllocator::createString(utf8, std::strlen(utf8));
    }
  };
}

bool egg::ovum::StringLess::operator()(const egg::ovum::String& lhs, const egg::ovum::String& rhs) const {
  if (lhs == nullptr) {
    return false;
  }
  if (rhs == nullptr) {
    return true;
  }
  return compareUTF8(lhs->begin(), lhs->bytes(), rhs->begin(), rhs->bytes());
}

egg::ovum::String::String(const char* utf8)
  : HardPtr(StringFallbackAllocator::createString(utf8)) {
  // We've got to create this string without an allocator, so use a fallback
}

egg::ovum::String::String(const std::string& utf8)
  : HardPtr(StringFallbackAllocator::createString(utf8.data(), utf8.size())) {
  // We've got to create this string without an allocator, so use a fallback
}

egg::ovum::String::String(const std::string& utf8, size_t codepoints)
  : HardPtr(StringFallbackAllocator::createString(utf8.data(), utf8.size(), codepoints)) {
  // We've got to create this string without an allocator, so use a fallback
}

egg::ovum::String egg::ovum::StringFactory::fromUTF8(IAllocator& allocator, const uint8_t* begin, const uint8_t* end, size_t codepoints) {
  assert(begin != nullptr);
  assert(end >= begin);
  auto bytes = size_t(end - begin);
  return String(createContiguous(allocator, begin, bytes, codepoints));
}

egg::ovum::String egg::ovum::StringFactory::fromUTF8(IAllocator& allocator, const uint8_t* begin, const uint8_t* end) {
  assert(begin != nullptr);
  assert(end >= begin);
  auto bytes = size_t(end - begin);
  return String(createContiguous(allocator, begin, bytes));
}

const egg::ovum::IMemory* egg::ovum::Variant::acquireFallbackString(const char* utf8, size_t bytes) {
  // We've got to create this string without an allocator, so use a fallback
  return HardPtr<IMemory>::hardAcquire(StringFallbackAllocator::createString(utf8, bytes));
}
