#include "ovum/test.h"

TEST(TestVM, CreateDefaultInstance) {
  egg::test::Allocator allocator;
  auto vm = egg::ovum::VMFactory::createDefault(allocator);
  ASSERT_NE(nullptr, vm);
}

TEST(TestVM, CreateTestInstance) {
  egg::test::Allocator allocator;
  auto vm = egg::ovum::VMFactory::createTest(allocator);
  ASSERT_NE(nullptr, vm);
}

TEST(TestVM, CreateUTF8) {
  egg::test::VM vm;
  auto s = vm->createUTF8(u8"hello");
  ASSERT_STRING("hello", s);
}

TEST(TestVM, CreateUTF32) {
  egg::test::VM vm;
  auto s = vm->createUTF32(U"hello");
  ASSERT_STRING("hello", s);
}
