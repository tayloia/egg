#include "ovum/test.h"

TEST(TestTesting, Empty) {
}

TEST(TestTesting, Truth) {
  ASSERT_EQ(6 * 7, 42);
}

TEST(TestTesting, Contains) {
  // These are our own additions to the assert macros
  ASSERT_CONTAINS("haystack", "ta");
  ASSERT_NOTCONTAINS("haystack", "needle");
}

TEST(TestTesting, StartsWith) {
  // This are our own addition to the assert macros
  ASSERT_STARTSWITH("haystack", "hay");
}

TEST(TestTesting, EndsWith) {
  // This are our own addition to the assert macros
  ASSERT_ENDSWITH("haystack", "stack");
}

TEST(TestTesting, Throws) {
  // This are our own addition to the assert macros
  ASSERT_THROW_E(throw std::runtime_error("reason"), std::runtime_error, ASSERT_STREQ("reason", e.what()));
}
