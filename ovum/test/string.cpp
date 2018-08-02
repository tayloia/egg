#include "ovum/test.h"

TEST(TestString, StringFromBytes) {
  egg::test::Allocator allocator;
  const size_t bufsize = 11;
  auto* buffer = reinterpret_cast<const egg::ovum::Byte*>("hello world");
  auto str = egg::ovum::StringFactory::fromUTF8(allocator, buffer, buffer + bufsize);
  ASSERT_EQ(bufsize, str.length());
  auto memory = str.memoryUTF8();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(bufsize, memory->bytes());
}

TEST(TestString, StringFromUTF8) {
  egg::test::Allocator allocator;
  auto str = egg::ovum::StringFactory::fromUTF8(allocator, u8"egg \U0001F95A");
  ASSERT_EQ(5u, str.length());
  auto memory = str.memoryUTF8();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(8u, memory->bytes());
}

TEST(TestString, StringToUTF8) {
  egg::test::Allocator allocator;
  auto input = egg::ovum::StringFactory::fromUTF8(allocator, u8"egg \U0001F95A");
  ASSERT_EQ(5u, input.length());
  auto output = input.toUTF8();
  ASSERT_STREQ(u8"egg \U0001F95A", output.c_str());
}

TEST(TestString, StringFallback) {
  // These are strings that are allocated on the fallback allocator because an explicit one is not specified
  egg::ovum::String empty;
  ASSERT_EQ(0u, empty.length());
  egg::ovum::String nil{ nullptr };
  ASSERT_EQ(0u, nil.length());
  egg::ovum::String hello{ "hello world" };
  ASSERT_EQ(11u, hello.length());
  ASSERT_STREQ("hello world", hello.toUTF8().c_str());
  egg::ovum::String goodbye{ std::string("goodbye") };
  ASSERT_EQ(7u, goodbye.length());
  ASSERT_STREQ("goodbye", goodbye.toUTF8().c_str());
}