#include "ovum/test.h"

namespace {
  struct Literal {
    const egg::ovum::Byte* begin;
    const egg::ovum::Byte* end;
    explicit Literal(const char* text) {
      assert(text != nullptr);
      this->begin = reinterpret_cast<const egg::ovum::Byte*>(text);
      this->end = this->begin + strlen(text);
    }
  };

  void allocatorStatisticsTest(egg::ovum::IAllocator& allocator) {
    egg::ovum::IAllocator::Statistics statistics;
    ASSERT_TRUE(allocator.statistics(statistics));
    ASSERT_EQ(statistics.currentBlocksAllocated, 0u);
    ASSERT_EQ(statistics.currentBytesAllocated, 0u);
    ASSERT_GT(statistics.totalBlocksAllocated, 0u);
    ASSERT_GT(statistics.totalBytesAllocated, 0u);
  }
}

TEST(TestString, StringCreateBytes) {
  egg::ovum::AllocatorDefault allocator;
  {
    Literal hello("hello world");
    auto str = egg::ovum::StringFactory::fromUTF8(allocator, hello.begin, hello.end);
    ASSERT_EQ(11u, str->length());
    auto memory = str->memoryUTF8();
    ASSERT_NE(nullptr, memory);
    ASSERT_EQ(11u, memory->bytes());
  }
  allocatorStatisticsTest(allocator);
}

TEST(TestString, StringCreateBuffer) {
  egg::ovum::AllocatorDefault allocator;
  {
    auto str = egg::ovum::StringFactory::fromUTF8(allocator, u8"egg \U0001F95A");
    ASSERT_EQ(5u, str->length());
    auto memory = str->memoryUTF8();
    ASSERT_NE(nullptr, memory);
    ASSERT_EQ(8u, memory->bytes());
  }
  allocatorStatisticsTest(allocator);
}
