#include "ovum/test.h"

namespace {
  struct Header {
    void* memory;
    Header() {
      // Make the memory pointer point to the extra space beyond the instance
      this->memory = this + 1;
    }
  };
  struct Literal {
    const egg::ovum::Byte* begin;
    const egg::ovum::Byte* end;
    explicit Literal(const char* text) {
      assert(text != nullptr);
      this->begin = reinterpret_cast<const egg::ovum::Byte*>(text);
      this->end = this->begin + strlen(text);
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

TEST(TestMemory, MemoryEmpty) {
  egg::ovum::AllocatorDefault allocator;
  auto memory = egg::ovum::MemoryFactory::create(allocator, 0);
  auto* ptr = memory.begin();
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ(memory.end(), ptr);
  ASSERT_EQ(0u, memory.bytes());
  auto another = egg::ovum::MemoryFactory::create(allocator, 0);
  ASSERT_EQ(another.begin(), ptr);
}

TEST(TestMemory, MemoryFactory) {
  egg::ovum::AllocatorDefault allocator;
  const size_t bufsize = 128;
  auto memory = egg::ovum::MemoryFactory::create(allocator, bufsize);
  auto* ptr = memory.begin();
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ(memory.end(), ptr + bufsize);
  ASSERT_EQ(bufsize, memory.bytes());
  ASSERT_TRUE(readWriteTest(ptr));
}

TEST(TestMemory, MemoryBuilder) {
  egg::ovum::AllocatorDefault allocator;
  egg::ovum::MemoryBuilder builder(allocator);
  Literal hello("hello world");
  builder.add(hello.begin, hello.end);
  auto memory = builder.bake();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(11u, memory->bytes());
  ASSERT_EQ(0, std::memcmp(memory->begin(), hello.begin, memory->bytes()));
  // The bake should have reset the builder
  memory = builder.bake();
  ASSERT_EQ(0u, memory->bytes());
  // Explicit reset
  builder.add(hello.begin, hello.end);
  builder.reset();
  Literal goodbye("goodbye");
  builder.add(goodbye.begin, goodbye.end);
  memory = builder.bake();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(7u, memory->bytes());
  ASSERT_EQ(0, std::memcmp(memory->begin(), goodbye.begin, memory->bytes()));
  // Concatenation
  builder.add(hello.begin, hello.end);
  builder.add(goodbye.begin, goodbye.end);
  memory = builder.bake();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(18u, memory->bytes());
  ASSERT_EQ(0, std::memcmp(memory->begin(), "hello worldgoodbye", memory->bytes()));
}

TEST(TestMemory, MemoryShared) {
  egg::ovum::AllocatorDefault allocator;
  auto memory = egg::ovum::MemoryFactory::create(allocator, 11);
  ASSERT_EQ(11u, memory.bytes());
  std::memcpy(memory.begin(), "hello world", memory.bytes());
  auto shared = memory.bake();
  ASSERT_EQ(11u, shared->bytes());
  ASSERT_EQ(0, std::memcmp(shared->begin(), "hello world", shared->bytes()));
  // Test that a builder just returns the chunk if there's only one
  egg::ovum::MemoryBuilder builder(allocator);
  builder.add(*shared);
  auto result = builder.bake();
  ASSERT_EQ(shared->bytes(), result->bytes());
  ASSERT_EQ(shared->begin(), result->begin());
  ASSERT_EQ(shared->end(), result->end());
  // Check that two chunks result in concatenation
  builder.add(*shared);
  builder.add(*shared);
  result = builder.bake();
  ASSERT_EQ(shared->bytes() * 2, result->bytes());
  ASSERT_NE(shared->begin(), result->begin());
  ASSERT_NE(shared->end(), result->end());
  ASSERT_EQ(0, std::memcmp(result->begin(), "hello worldhello world", result->bytes()));
}
