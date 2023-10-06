#include "ovum/test.h"

TEST(TestString, Empty) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  egg::ovum::String str;
  ASSERT_EQ(nullptr, str);
  ASSERT_EQ(0u, str.length());
  str = egg::ovum::String::fromUTF8(allocator, nullptr);
  ASSERT_EQ(nullptr, str);
  ASSERT_EQ(0u, str.length());
}

TEST(TestString, FromBytes) {
  egg::test::Allocator allocator;
  auto str = egg::ovum::String::fromUTF8(allocator, "hello world");
  ASSERT_NE(nullptr, str);
  ASSERT_EQ(11u, str.length());
  ASSERT_EQ(11u, str->bytes());
}

TEST(TestString, FromUTF8) {
  egg::test::Allocator allocator;
  auto str = egg::ovum::String::fromUTF8(allocator, u8"egg \U0001F95A");
  ASSERT_NE(nullptr, str);
  ASSERT_EQ(5u, str.length());
  ASSERT_EQ(8u, str->bytes());
}

TEST(TestString, ToUTF8) {
  egg::test::Allocator allocator;
  auto input = egg::ovum::String::fromUTF8(allocator, u8"egg \U0001F95A");
  ASSERT_EQ(5u, input.length());
  auto output = input.toUTF8();
  ASSERT_STRING(u8"egg \U0001F95A", allocator.concat(output));
}

TEST(TestString, Assignment) {
  egg::test::Allocator allocator;
  auto a = allocator.concat("hello world");
  ASSERT_STRING("hello world", a);
  auto b = allocator.concat("goodbye");
  ASSERT_STRING("goodbye", b);
  a = b;
  ASSERT_STRING("goodbye", a);
  ASSERT_STRING("goodbye", b);
  a = std::move(b);
  ASSERT_STRING("goodbye", a);
  ASSERT_STRING("", b);
}
