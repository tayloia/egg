#include "ovum/ovum.h"
#include "ovum/utf.h"

namespace {
  using namespace egg::ovum;

  const uint8_t emptyByte = 0;
  const UTF8 emptyReader(&emptyByte, &emptyByte, 0);

  UTF8 utf8ReaderBegin(const String& s) {
    auto* p = s.get();
    if (p == nullptr) {
      return emptyReader;
    }
    return UTF8(p->begin(), p->end(), 0);
  }

  UTF8 utf8ReaderEnd(const String& s) {
    auto* p = s.get();
    if (p == nullptr) {
      return emptyReader;
    }
    return UTF8(p->begin(), p->end(), p->bytes());
  }

  UTF8 utf8ReaderIndex(const String& s, size_t index) {
    auto length = s.length();
    if (index >= length) {
      return emptyReader;
    }
    if (index > (length >> 1)) {
      // We're closer to the end of the string
      auto reader = utf8ReaderEnd(s);
      reader.skipBackward(length - index);
      return reader;
    }
    auto reader = utf8ReaderBegin(s);
    reader.skipForward(index);
    return reader;
  }

  const IMemory* createContiguous(IAllocator& allocator, const uint8_t* utf8, size_t bytes, size_t codepoints = SIZE_MAX) {
    // TODO detect malformed/overlong/etc
    if ((utf8 == nullptr) || (bytes == 0)) {
      return nullptr;
    }
    if (codepoints == SIZE_MAX) {
      codepoints = egg::ovum::UTF8::measure(utf8, utf8 + bytes);
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
    static const IMemory* createUTF8(const char* utf8, size_t bytes, size_t codepoints = SIZE_MAX) {
      static StringFallbackAllocator allocator;
      return createContiguous(allocator, reinterpret_cast<const uint8_t*>(utf8), bytes, codepoints);
    }
    static const IMemory* createUTF8(const char* utf8) {
      return (utf8 == nullptr) ? nullptr : StringFallbackAllocator::createUTF8(utf8, std::strlen(utf8));
    }
  };
}

egg::ovum::String::String(const char* utf8)
  : HardPtr(StringFallbackAllocator::createUTF8(utf8)) {
  // We've got to create this string without an allocator, so use a fallback
}

egg::ovum::String::String(const std::string& utf8, size_t codepoints)
  : HardPtr(StringFallbackAllocator::createUTF8(utf8.data(), utf8.size(), codepoints)) {
  // We've got to create this string without an allocator, so use a fallback
}

int64_t egg::ovum::String::hashCode() const {
  // See https://docs.oracle.com/javase/6/docs/api/java/lang/String.html#hashCode()
  int64_t hash = 0;
  auto reader = utf8ReaderBegin(*this);
  char32_t codepoint = 0;
  while (reader.forward(codepoint)) {
    hash = hash * 31 + int64_t(uint32_t(codepoint));
  }
  return hash;
}
int32_t egg::ovum::String::codePointAt(size_t index) const {
  auto reader = utf8ReaderIndex(*this, index);
  char32_t codepoint = 0;
  return reader.forward(codepoint) ? int32_t(codepoint) : -1;
}

bool egg::ovum::String::equals(const String& other) const {
  // Ordinal comparison
  return IMemory::equals(this->get(), other.get());
}

bool egg::ovum::String::lessThan(const String& other) const {
  // Ordinal comparison via UTF8
  auto* lhs = this->get();
  if (lhs == nullptr) {
    return !other.empty();
  }
  auto* rhs = other.get();
  if (rhs == nullptr) {
    return false;
  }
  auto lsize = lhs->bytes();
  auto rsize = rhs->bytes();
  if (lsize < rsize) {
    return std::memcmp(lhs->begin(), rhs->begin(), lsize) <= 0;
  }
  return std::memcmp(lhs->begin(), rhs->begin(), rsize) < 0;
}

int64_t egg::ovum::String::compareTo(const String& other) const {
  // Ordinal comparison via UTF8
  auto* lhs = this->get();
  if (lhs == nullptr) {
    return other.empty() ? 0 : -1;
  }
  auto* rhs = other.get();
  if (rhs == nullptr) {
    return this->empty() ? 0 : 1;
  }
  auto lsize = lhs->bytes();
  auto rsize = rhs->bytes();
  int cmp;
  if (lsize < rsize) {
    cmp = std::memcmp(lhs->begin(), rhs->begin(), lsize);
    if (cmp == 0) {
      return -1;
    }
  } else if (lsize > rsize) {
    cmp = std::memcmp(lhs->begin(), rhs->begin(), rsize);
    if (cmp == 0) {
      return 1;
    }
  } else {
    cmp = std::memcmp(lhs->begin(), rhs->begin(), rsize);
  }
  return (cmp > 0) - (cmp < 0); // ensure {-1,0,+1}
}

egg::ovum::String egg::ovum::String::fromCodePoint(char32_t codepoint) {
  // OPTIMIZE
  assert(codepoint <= 0x10FFFF);
  auto utf8 = egg::ovum::UTF32::toUTF8(codepoint);
  return String(StringFallbackAllocator::createUTF8(utf8.data(), utf8.size(), 1));
}

egg::ovum::String egg::ovum::String::fromUTF8(const std::string& utf8, size_t codepoints) {
  return String(StringFallbackAllocator::createUTF8(utf8.data(), utf8.size(), codepoints));
}

egg::ovum::String egg::ovum::String::fromUTF32(const std::u32string& utf32) {
  auto utf8 = egg::ovum::UTF32::toUTF8(utf32);
  return String(StringFallbackAllocator::createUTF8(utf8.data(), utf8.size(), utf32.size()));
}

egg::ovum::String egg::ovum::StringFactory::fromCodePoint(IAllocator& allocator, char32_t codepoint) {
  // OPTIMIZE
  assert(codepoint <= 0x10FFFF);
  auto utf8 = egg::ovum::UTF32::toUTF8(codepoint);
  return String(createContiguous(allocator, reinterpret_cast<const uint8_t*>(utf8.data()), utf8.size(), 1));
}

egg::ovum::String egg::ovum::StringFactory::fromUTF8(IAllocator& allocator, const uint8_t* begin, const uint8_t* end, size_t codepoints) {
  assert(begin != nullptr);
  assert(end >= begin);
  auto bytes = size_t(end - begin);
  return String(createContiguous(allocator, begin, bytes, codepoints));
}

const egg::ovum::IMemory* egg::ovum::Variant::acquireFallbackString(const char* utf8, size_t bytes) {
  // We've got to create this string without an allocator, so use a fallback
  return HardPtr<IMemory>::hardAcquire(StringFallbackAllocator::createUTF8(utf8, bytes));
}

std::ostream& operator<<(std::ostream& os, const egg::ovum::String& text) {
  return os << text.toUTF8(); // WIBBLE
}
