#include "ovum/test.h"

TEST(TestString, StringCreateBytes) {
  egg::test::Allocator allocator;
  const size_t bufsize = 11;
  auto* buffer = reinterpret_cast<const egg::ovum::Byte*>("hello world");
  auto str = egg::ovum::StringFactory::fromUTF8(allocator, buffer, buffer + bufsize);
  ASSERT_EQ(bufsize, str->length());
  auto memory = str->memoryUTF8();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(bufsize, memory->bytes());
}

TEST(TestString, StringCreateBuffer) {
  egg::test::Allocator allocator;
  auto str = egg::ovum::StringFactory::fromUTF8(allocator, u8"egg \U0001F95A");
  ASSERT_EQ(5u, str->length());
  auto memory = str->memoryUTF8();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(8u, memory->bytes());
}
