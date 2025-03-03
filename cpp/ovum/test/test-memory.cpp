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
    const uint8_t* begin;
    const uint8_t* end;
    explicit Literal(const char* text) {
      assert(text != nullptr);
      this->begin = reinterpret_cast<const uint8_t*>(text);
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
  egg::test::Allocator allocator;
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
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  auto empty = egg::ovum::MemoryFactory::createEmpty();
  ASSERT_NE(nullptr, empty);
  ASSERT_NE(nullptr, empty->begin());
  ASSERT_EQ(empty->begin(), empty->end());
  ASSERT_EQ(0u, empty->bytes());
  auto memory = egg::ovum::MemoryFactory::createMutable(allocator, 0);
  auto* ptr = memory.begin();
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ(memory.end(), ptr);
  ASSERT_EQ(0u, memory.bytes());
  auto another = egg::ovum::MemoryFactory::createMutable(allocator, 0);
  ASSERT_EQ(another.begin(), ptr);
}

TEST(TestMemory, MemoryImmutable) {
  egg::test::Allocator allocator;
  const char* buffer = "hello world";
  const size_t bufsize = strlen(buffer) + 1;
  auto memory = egg::ovum::MemoryFactory::createImmutable(allocator, buffer, bufsize);
  ASSERT_NE(nullptr, memory);
  auto* ptr = memory->begin();
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ(memory->end(), ptr + bufsize);
  ASSERT_EQ(bufsize, memory->bytes());
  ASSERT_STREQ(buffer, reinterpret_cast<const char*>(ptr));
}

TEST(TestMemory, MemoryMutable) {
  egg::test::Allocator allocator;
  const size_t bufsize = 128;
  auto memory = egg::ovum::MemoryFactory::createMutable(allocator, bufsize);
  auto* ptr = memory.begin();
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ(memory.end(), ptr + bufsize);
  ASSERT_EQ(bufsize, memory.bytes());
  ASSERT_TRUE(readWriteTest(ptr));
  auto built = memory.build();
  ASSERT_NE(nullptr, built);
  ASSERT_EQ(bufsize, built->bytes());
  ASSERT_EQ(*ptr, *built->begin());
}

TEST(TestMemory, MemoryTag) {
  egg::test::Allocator allocator;
  egg::ovum::IMemory::Tag tag;
  tag.u = 123456789;
  auto memory = egg::ovum::MemoryFactory::createImmutable(allocator, nullptr, 0, tag);
  ASSERT_EQ(123456789u, memory->tag().u);
  tag.p = &allocator;
  memory = egg::ovum::MemoryFactory::createImmutable(allocator, nullptr, 0, tag);
  ASSERT_EQ(&allocator, memory->tag().p);
}

TEST(TestMemory, MemoryBuilder) {
  egg::test::Allocator allocator;
  egg::ovum::MemoryBuilder builder(allocator);
  Literal hello("hello world");
  builder.add(hello.begin, hello.end);
  auto memory = builder.build();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(11u, memory->bytes());
  ASSERT_EQ(0, std::memcmp(memory->begin(), hello.begin, memory->bytes()));
  // The build should have reset the builder
  memory = builder.build();
  ASSERT_EQ(0u, memory->bytes());
  // Explicit reset
  builder.add(hello.begin, hello.end);
  builder.reset();
  Literal goodbye("goodbye");
  builder.add(goodbye.begin, goodbye.end);
  memory = builder.build();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(7u, memory->bytes());
  ASSERT_EQ(0, std::memcmp(memory->begin(), goodbye.begin, memory->bytes()));
  // Concatenation
  builder.add(hello.begin, hello.end);
  builder.add(goodbye.begin, goodbye.end);
  memory = builder.build();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(18u, memory->bytes());
  ASSERT_EQ(0, std::memcmp(memory->begin(), "hello worldgoodbye", memory->bytes()));
}

TEST(TestMemory, MemoryShared) {
  egg::test::Allocator allocator;
  auto memory = egg::ovum::MemoryFactory::createMutable(allocator, 11);
  ASSERT_EQ(11u, memory.bytes());
  std::memcpy(memory.begin(), "hello world", memory.bytes());
  auto shared = memory.build();
  ASSERT_EQ(11u, shared->bytes());
  ASSERT_EQ(0, std::memcmp(shared->begin(), "hello world", shared->bytes()));
  // Test that a builder just returns the chunk if there's only one
  egg::ovum::MemoryBuilder builder(allocator);
  builder.add(*shared);
  auto result = builder.build();
  ASSERT_EQ(shared->bytes(), result->bytes());
  ASSERT_EQ(shared->begin(), result->begin());
  ASSERT_EQ(shared->end(), result->end());
  // Check that two chunks result in concatenation
  builder.add(*shared);
  builder.add(*shared);
  result = builder.build();
  ASSERT_EQ(shared->bytes() * 2, result->bytes());
  ASSERT_NE(shared->begin(), result->begin());
  ASSERT_NE(shared->end(), result->end());
  ASSERT_EQ(0, std::memcmp(result->begin(), "hello worldhello world", result->bytes()));
}
