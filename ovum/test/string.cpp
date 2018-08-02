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
}

TEST(TestString, StringCreateBytes) {
  egg::test::Allocator allocator;
  Literal hello("hello world");
  auto str = egg::ovum::StringFactory::fromUTF8(allocator, hello.begin, hello.end);
  ASSERT_EQ(11u, str->length());
  auto memory = str->memoryUTF8();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(11u, memory->bytes());
}

TEST(TestString, StringCreateBuffer) {
  egg::test::Allocator allocator;
  auto str = egg::ovum::StringFactory::fromUTF8(allocator, u8"egg \U0001F95A");
  ASSERT_EQ(5u, str->length());
  auto memory = str->memoryUTF8();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(8u, memory->bytes());
}
