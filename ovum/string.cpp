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
    static const StringEmpty instance;
  };
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
  if (bytes == 0) {
    return String(StringEmpty::instance);
  }
  auto length = utf8::measure(begin, end);
  if (length == SIZE_MAX) {
    throw std::invalid_argument("egg::ovum::StringFactory: Invalid UTF-8 input data");
  }
  auto memory = MemoryFactory::createMutable(allocator, bytes);
  std::memcpy(memory.begin(), begin, bytes);
  return String(*allocator.create<StringContiguous>(0, allocator, memory.bake(), length));
}
