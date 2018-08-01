#include "ovum/test.h"

namespace {
  struct Header {
    void* memory;
    Header() {
      this->memory = this + 1;
    }
  };

  bool readWriteTest(volatile void* memory) {
    auto* p = static_cast<volatile uint32_t*>(memory);
    uint32_t expected = 0;
    for (size_t i = 0; i < 100; ++i) {
      *p = expected;
      if (*p != expected) {
        return false;
      }
      expected += 0x07050301;
    }
    return true;
  }
}

TEST(TestMemory, AllocatorDefault) {
  egg::ovum::AllocatorDefault allocator;
  const size_t bufsize = 128;
  const size_t align = alignof(std::max_align_t);
  // Perform a raw allocation/deallocation
  auto* memory = allocator.allocate(bufsize, align);
  ASSERT_NE(nullptr, memory);
  ASSERT_TRUE(readWriteTest(memory));
  allocator.deallocate(memory, align);
  // Perform a header allocation with extra space
  auto header = allocator.create<Header>(bufsize);
  ASSERT_NE(nullptr, header);
  ASSERT_NE(nullptr, header->memory);
  ASSERT_TRUE(readWriteTest(header->memory));
  allocator.destroy(header);
}
