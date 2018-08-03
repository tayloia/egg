#include "ovum/ovum.h"
#include "ovum/utf8.h"

namespace {
  const egg::ovum::IMemory* createContiguous(egg::ovum::IAllocator& allocator, const egg::ovum::Byte* utf8, size_t bytes) {
    // TODO detect malformed/overlong/etc
    if ((utf8 == nullptr) || (bytes == 0)) {
      return nullptr;
    }
    auto length = egg::ovum::utf8::measure(utf8, utf8 + bytes);
    if (length == SIZE_MAX) {
      throw std::invalid_argument("egg::ovum::String: Invalid UTF-8 input data");
    }
    auto* memory = allocator.create<egg::ovum::MemoryContiguous>(bytes, allocator, bytes, egg::ovum::IMemory::Tag{ length });
    assert(memory != nullptr);
    std::memcpy(memory->base(), utf8, bytes);
    return memory;
  }

  class StringFallbackAllocator final : public egg::ovum::IAllocator {
    StringFallbackAllocator(const StringFallbackAllocator&) = delete;
    StringFallbackAllocator& operator=(const StringFallbackAllocator&) = delete;
  private:
    egg::ovum::Atomic<int64_t> atomic;
  public:
    StringFallbackAllocator() : atomic(0) {
    }
    ~StringFallbackAllocator() {
      // Make sure all our strings have been destroyed when the process exits
      assert(this->atomic.get() == 0);
    }
    virtual void* allocate(size_t bytes, size_t alignment) {
      this->atomic.increment();
      return egg::ovum::AllocatorDefaultPolicy::memalloc(bytes, alignment);
    }
    virtual void deallocate(void* allocated, size_t alignment) {
      assert(allocated != nullptr);
      this->atomic.decrement();
      egg::ovum::AllocatorDefaultPolicy::memfree(allocated, alignment);
    }
    virtual bool statistics(Statistics&) const {
      return false;
    }
    static const egg::ovum::IMemory* createString(const char* utf8, size_t bytes) {
      static StringFallbackAllocator allocator;
      return createContiguous(allocator, reinterpret_cast<const egg::ovum::Byte*>(utf8), bytes);
    }
    static const egg::ovum::IMemory* createString(const char* utf8) {
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

egg::ovum::String egg::ovum::StringFactory::fromUTF8(IAllocator& allocator, const Byte* begin, const Byte* end) {
  assert(begin != nullptr);
  assert(end >= begin);
  auto bytes = size_t(end - begin);
  if (bytes == 0) {
    return String();
  }
  return String(createContiguous(allocator, begin, bytes));
}

const egg::ovum::IMemory* egg::ovum::Variant::acquireFallbackString(const char* utf8, size_t bytes) {
  // We've got to create this string without an allocator, so use a fallback
  return HardPtr<IMemory>::hardAcquire(StringFallbackAllocator::createString(utf8, bytes));
}
