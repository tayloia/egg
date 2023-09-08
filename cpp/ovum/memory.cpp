#include "ovum/ovum.h"

namespace {
  class MemoryEmpty : public egg::ovum::HardReferenceCountedNone<egg::ovum::IMemory> {
    MemoryEmpty(const MemoryEmpty&) = delete;
    MemoryEmpty& operator=(const MemoryEmpty&) = delete;
  private:
    static const uint8_t empty;
    MemoryEmpty() {}
  public:
    virtual const uint8_t* begin() const override {
      return &empty;
    }
    virtual const uint8_t* end() const override {
      return &empty;
    }
    virtual egg::ovum::IMemory::Tag tag() const override {
      return egg::ovum::IMemory::Tag{ 0 };
    }
    static const MemoryEmpty instance;
  };
  const uint8_t MemoryEmpty::empty{ 0 };
  const MemoryEmpty MemoryEmpty::instance{};
}

bool egg::ovum::Memory::validate() const {
  auto* memory = this->get();
  if (memory == nullptr) {
    return true;
  }
  auto* p = memory->begin();
  auto* q = memory->end();
  return (p != nullptr) && (q >= p);

}

bool egg::ovum::Memory::equal(const IMemory* lhs, const IMemory* rhs) {
  // Don't look at the tag
  if (lhs == rhs) {
    return true;
  }
  if ((lhs == nullptr) || (rhs == nullptr)) {
    return false;
  }
  auto bytes = lhs->bytes();
  if (rhs->bytes() != bytes) {
    return false;
  }
  return std::memcmp(lhs->begin(), rhs->begin(), bytes) == 0;
}

egg::ovum::Memory egg::ovum::MemoryFactory::createEmpty() {
  return egg::ovum::Memory(&MemoryEmpty::instance);
}

egg::ovum::Memory egg::ovum::MemoryFactory::createImmutable(IAllocator& allocator, const void* src, size_t bytes, IMemory::Tag tag) {
  assert((src != nullptr) || (bytes == 0));
  if ((bytes == 0) && (tag.u == 0)) {
    return egg::ovum::Memory(&MemoryEmpty::instance);
  }
  auto* immutable = allocator.create<MemoryContiguous>(bytes, allocator, bytes, tag);
  assert(immutable != nullptr);
  if ((src != nullptr) && (bytes > 0)) {
    std::memcpy(immutable->base(), src, bytes);
  }
  return egg::ovum::Memory(immutable);
}

egg::ovum::MemoryMutable egg::ovum::MemoryFactory::createMutable(IAllocator& allocator, size_t bytes, IMemory::Tag tag) {
  if ((bytes == 0) && (tag.u == 0)) {
    return egg::ovum::MemoryMutable(&MemoryEmpty::instance);
  }
  return egg::ovum::MemoryMutable(allocator.create<MemoryContiguous>(bytes, allocator, bytes, tag));
}

egg::ovum::MemoryBuilder::MemoryBuilder(egg::ovum::IAllocator& allocator)
  : allocator(allocator),
    chunks(),
    bytes(0) {
}

void egg::ovum::MemoryBuilder::add(const uint8_t* begin, const uint8_t* end) {
  assert(begin != nullptr);
  assert(end >= begin);
  auto size = size_t(end - begin);
  if (size > 0) {
    this->chunks.emplace_back(nullptr, begin, size);
    this->bytes += size;
  }
}

void egg::ovum::MemoryBuilder::add(const IMemory& memory) {
  auto size = memory.bytes();
  if (size > 0) {
    this->chunks.emplace_back(&memory, memory.begin(), size);
    this->bytes += size;
  }
}

egg::ovum::Memory egg::ovum::MemoryBuilder::build() {
  if (this->chunks.size() == 1) {
    // There's only a single chunk in the list
    auto front = this->chunks.front().memory;
    if (front != nullptr) {
      // Simply re-use the memory block
      this->reset();
      return front;
    }
  }
  auto created = MemoryFactory::createMutable(this->allocator, this->bytes);
  auto* ptr = created.begin();
  for (const auto& chunk : this->chunks) {
    std::memcpy(ptr, chunk.base, chunk.bytes);
    ptr += chunk.bytes;
  }
  assert(ptr == created.end());
  this->reset();
  return created.build();
}

void egg::ovum::MemoryBuilder::reset() {
  this->chunks.clear();
  this->bytes = 0;
}
