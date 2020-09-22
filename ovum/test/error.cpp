#include "ovum/test.h"

TEST(TestError, ErrorMessage) {
  egg::ovum::Error error("message");
  ASSERT_STRING("message", error.toString());
}

TEST(TestError, ErrorFormat) {
  egg::ovum::Error error("b=", true, ' ', "i=", 123, ' ', "f=", 1.23);
  ASSERT_STRING("b=true i=123 f=1.23", error.toString());
}

TEST(TestError, ErraticIntGood) {
  egg::ovum::Erratic<int> erratic(123);
  ASSERT_FALSE(erratic.failed());
  ASSERT_EQ(123, erratic.success());
}

TEST(TestError, ErraticIntBad) {
  auto erratic = egg::ovum::Erratic<int>::fail("something", ' ', "failed");
  ASSERT_TRUE(erratic.failed());
  ASSERT_STRING("something failed", erratic.failure().toString());
}

TEST(TestError, ErraticVoidGood) {
  egg::ovum::Erratic<void> erratic;
  ASSERT_FALSE(erratic.failed());
}

TEST(TestError, ErraticVoidBad) {
  auto erratic = egg::ovum::Erratic<void>::fail("something", ' ', "failed");
  ASSERT_TRUE(erratic.failed());
  ASSERT_STRING("something failed", erratic.failure().toString());
}
