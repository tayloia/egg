#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(push, 3)
#pragma warning(disable: 4365 4623 4625 4626 5026 5027)
#endif
#include "gtest/gtest.h"
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(pop)
#endif


namespace egg::yolk::test {
  // This is the default entry-point for Google Test runners
  int main(int argc, char** argv);

  // Use by ASSERT_CONTAINS, ASSERT_STARTSWITH, ASSERT_ENDSWITH, et al
  ::testing::AssertionResult assertContains(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle);
  ::testing::AssertionResult assertNotContains(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle);
  ::testing::AssertionResult assertStartsWith(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle);
  ::testing::AssertionResult assertEndsWith(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle);
}

// Add some useful extra macros
#define ASSERT_CONTAINS(haystack, needle) ASSERT_PRED_FORMAT2(egg::yolk::test::assertContains, haystack, needle)
#define ASSERT_NOTCONTAINS(haystack, needle) ASSERT_PRED_FORMAT2(egg::yolk::test::assertNotContains, haystack, needle)
#define ASSERT_STARTSWITH(haystack, needle) ASSERT_PRED_FORMAT2(egg::yolk::test::assertStartsWith, haystack, needle)
#define ASSERT_ENDSWITH(haystack, needle)   ASSERT_PRED_FORMAT2(egg::yolk::test::assertEndsWith, haystack, needle)
