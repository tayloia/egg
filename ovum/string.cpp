#include "ovum/ovum.h"
#include "ovum/utf8.h"

namespace {
  using namespace egg::ovum;

  const IMemory* createContiguous(IAllocator& allocator, const uint8_t* utf8, size_t bytes) {
    // TODO detect malformed/overlong/etc
    if ((utf8 == nullptr) || (bytes == 0)) {
      return nullptr;
    }
    auto length = utf8::measure(utf8, utf8 + bytes);
    if (length == SIZE_MAX) {
      throw std::invalid_argument("String: Invalid UTF-8 input data");
    }
    auto* memory = allocator.create<MemoryContiguous>(bytes, allocator, bytes, IMemory::Tag{ length });
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
    static const IMemory* createString(const char* utf8, size_t bytes) {
      static StringFallbackAllocator allocator;
      return createContiguous(allocator, reinterpret_cast<const uint8_t*>(utf8), bytes);
    }
    static const IMemory* createString(const char* utf8) {
      return (utf8 == nullptr) ? nullptr : StringFallbackAllocator::createString(utf8, std::strlen(utf8));
    }
  };
}

egg::ovum::String::String(const char* utf8)
  : HardPtr(StringFallbackAllocator::createString(utf8)) {
  // We've got to create this string without an allocator, so use a fallback
}

egg::ovum::String::String(const std::string& utf8)
  : HardPtr(StringFallbackAllocator::createString(utf8.data(), utf8.size())) {
  // We've got to create this string without an allocator, so use a fallback
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
