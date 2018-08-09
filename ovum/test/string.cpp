#include "ovum/test.h"

TEST(TestString, Empty) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  egg::ovum::String str;
  ASSERT_EQ(nullptr, str);
  ASSERT_EQ(0u, str.length());
  uint8_t nul = 0;
  str = egg::ovum::StringFactory::fromUTF8(allocator, &nul, &nul);
  ASSERT_EQ(nullptr, str);
  ASSERT_EQ(0u, str.length());
}

TEST(TestString, FromBytes) {
  egg::test::Allocator allocator;
  const size_t bufsize = 11;
  auto* buffer = reinterpret_cast<const uint8_t*>("hello world");
  auto str = egg::ovum::StringFactory::fromUTF8(allocator, buffer, buffer + bufsize);
  ASSERT_NE(nullptr, str);
  ASSERT_EQ(bufsize, str.length());
  ASSERT_EQ(bufsize, str->bytes());
}

TEST(TestString, FromUTF8) {
  egg::test::Allocator allocator;
  auto str = egg::ovum::StringFactory::fromUTF8(allocator, u8"egg \U0001F95A");
  ASSERT_NE(nullptr, str);
  ASSERT_EQ(5u, str.length());
  ASSERT_EQ(8u, str->bytes());
}

TEST(TestString, ToUTF8) {
  egg::test::Allocator allocator;
  auto input = egg::ovum::StringFactory::fromUTF8(allocator, u8"egg \U0001F95A");
  ASSERT_EQ(5u, input.length());
  auto output = input.toUTF8();
  ASSERT_STREQ(u8"egg \U0001F95A", output.c_str());
}

TEST(TestString, Fallback) {
  // These are strings that are allocated on the fallback allocator because an explicit one is not specified
  egg::ovum::String empty;
  ASSERT_EQ(0u, empty.length());
  const char* null = nullptr;
  egg::ovum::String nil{ null };
  ASSERT_EQ(0u, nil.length());
  egg::ovum::String hello{ "hello world" };
  ASSERT_EQ(11u, hello.length());
  ASSERT_STREQ("hello world", hello.toUTF8().c_str());
  egg::ovum::String goodbye{ std::string("goodbye") };
  ASSERT_EQ(7u, goodbye.length());
  ASSERT_STRING("goodbye", goodbye);
}

TEST(TestString, Assignment) {
  egg::ovum::String a{ "hello world" };
  ASSERT_STRING("hello world", a);
  egg::ovum::String b{ "goodbye" };
  ASSERT_STRING("goodbye", b);
  a = b;
  ASSERT_STRING("goodbye", a);
  ASSERT_STRING("goodbye", b);
  a = std::move(b);
  ASSERT_STRING("goodbye", a);
  ASSERT_STRING("", b);
}
