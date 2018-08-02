#include "ovum/ovum.h"
#include "ovum/utf8.h"

namespace {
  egg::ovum::IMemoryPtr slice(const egg::ovum::IMemory& memory, size_t length, size_t codePointOffset, size_t codePointLength) {
    assert(codePointOffset == 0);
    assert(codePointLength >= length);
    return egg::ovum::IMemoryPtr(&memory);
  }

  class StringEmpty : public egg::ovum::NotReferenceCounted<egg::ovum::IString> {
    StringEmpty(const StringEmpty&) = delete;
    StringEmpty& operator=(const StringEmpty&) = delete;
  private:
    StringEmpty() {}
  public:
    virtual size_t length() const override {
      return 0;
    }
    virtual egg::ovum::IMemoryPtr memoryUTF8(size_t, size_t) const override {
      return egg::ovum::MemoryFactory::createEmpty();
    }
    static const StringEmpty instance;
  };
  const StringEmpty StringEmpty::instance{};

  class StringContiguous : public egg::ovum::HardReferenceCounted<egg::ovum::IString> {
    StringContiguous(const StringContiguous&) = delete;
    StringContiguous& operator=(const StringContiguous&) = delete;
  private:
    egg::ovum::IMemoryPtr memory;
    size_t codepoints;
  public:
    StringContiguous(egg::ovum::IAllocator& allocator, egg::ovum::IMemoryPtr&& memory, size_t codepoints)
      : HardReferenceCounted(allocator),
        memory(std::move(memory)),
        codepoints(codepoints) {
      assert(this->memory->bytes() >= codepoints);
      assert(this->codepoints > 0);
    }
    virtual size_t length() const override {
      return this->codepoints;
    }
    virtual egg::ovum::IMemoryPtr memoryUTF8(size_t codePointOffset, size_t codePointLength) const override {
      return slice(*this->memory, this->codepoints, codePointOffset, codePointLength);
    }
    static const IString* create(egg::ovum::IAllocator& allocator, const egg::ovum::Byte* utf8, size_t bytes) {
      assert(utf8 != nullptr);
      if (bytes == 0) {
        return &StringEmpty::instance;
      }
      auto length = egg::ovum::utf8::measure(utf8, utf8 + bytes);
      if (length == SIZE_MAX) {
        throw std::invalid_argument("egg::ovum::String: Invalid UTF-8 input data");
      }
      auto memory = egg::ovum::MemoryFactory::createMutable(allocator, bytes);
      std::memcpy(memory.begin(), utf8, bytes);
      return allocator.create<StringContiguous>(0, allocator, memory.bake(), length);
    }
  };

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
    static const egg::ovum::IString* createString(const char* utf8, size_t bytes) {
      static StringFallbackAllocator allocator;
      auto* begin = reinterpret_cast<const egg::ovum::Byte*>(utf8);
      assert(begin != nullptr);
      auto* created = StringContiguous::create(allocator, begin, bytes);
      assert(created != nullptr);
      return created;
    }
    static const egg::ovum::IString* createString(const char* utf8) {
      if (utf8 == nullptr) {
        // Map null to the empty string
        return &StringEmpty::instance;
      }
      return StringFallbackAllocator::createString(utf8, std::strlen(utf8));
    }
  };
}

egg::ovum::String::String(const char* utf8)
  : HardRef(*StringFallbackAllocator::createString(utf8)) {
  // We've got to create this string without an allocator, so use a fallback
}

egg::ovum::String::String(const std::string& utf8)
  : HardRef(*StringFallbackAllocator::createString(utf8.data(), utf8.size())) {
  // We've got to create this string without an allocator, so use a fallback
}

const egg::ovum::IString* egg::ovum::Variant::acquireFallbackString(const char* utf8, size_t bytes) {
  // We've got to create this string without an allocator, so use a fallback
  auto* created = StringFallbackAllocator::createString(utf8, bytes);
  assert(created != nullptr);
  return String::hardAcquire(*created);
}

egg::ovum::String egg::ovum::StringFactory::fromUTF8(IAllocator& allocator, const egg::ovum::IMemory& memory, size_t length) {
  assert(memory.bytes() >= length);
  if (length == 0) {
    return String(StringEmpty::instance);
  }
  IMemoryPtr ptr(&memory);
  return String(*allocator.create<StringContiguous>(0, allocator, std::move(ptr), length));
}

egg::ovum::String egg::ovum::StringFactory::fromUTF8(IAllocator& allocator, const Byte* begin, const Byte* end) {
  assert(begin != nullptr);
  assert(end >= begin);
  auto bytes = size_t(end - begin);
  return String(*StringContiguous::create(allocator, begin, bytes));
}
