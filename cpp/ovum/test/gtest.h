// GoogleTest needs a reduction in MSVC's warning levels
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(push, 3)
#pragma warning(disable: 26812 26495)
#endif
#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(pop)
// warning C4625 : copy constructor was implicitly defined as deleted
// warning C4626 : assignment operator was implicitly defined as deleted
// warning C5026 : move constructor was implicitly defined as deleted
// warning C5027 : move assignment operator was implicitly defined as deleted
#pragma warning(disable: 4625 4626 5026 5027)
// warning C26495 : This kind of function should not throw. Declare it 'noexcept' (PREfast)
#pragma warning(disable: 26439)
#endif

#include <regex>

namespace egg::test {
  // This is the default entry-point for Google Test runners
  int main(int argc, char** argv);

  // Readability wrappers
  inline bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
  }
  inline bool startsWith(const std::string& haystack, const std::string& needle) {
    return (haystack.size() >= needle.size()) && std::equal(needle.begin(), needle.end(), haystack.begin());
  }
  inline bool endsWith(const std::string& haystack, const std::string& needle) {
    return (haystack.size() >= needle.size()) && std::equal(needle.rbegin(), needle.rend(), haystack.rbegin());
  }

  // Used by ASSERT_CONTAINS, ASSERT_STARTSWITH, ASSERT_ENDSWITH, et al
  ::testing::AssertionResult assertContains(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle);
  ::testing::AssertionResult assertNotContains(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle);
  ::testing::AssertionResult assertStartsWith(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle);
  ::testing::AssertionResult assertEndsWith(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle);

  // Used by ASSERT_PRINT
  template<typename T>
  ::testing::AssertionResult assertPrint(const char* expected_expr, const char* value_expr, const std::string& expected, const T& value) {
    egg::ovum::StringBuilder sb;
    auto actual = sb.add(value).toUTF8();
    return ::testing::internal::CmpHelperEQ(expected_expr, value_expr, expected, actual);
  }

  // Used by EGG_INSTANTIATE_TEST_CASE_P
  template<typename T>
  auto registerTestCase(const char* name, const char* file, int line) {
    // See https://github.com/google/googletest/blob/main/docs/advanced.md#how-to-write-value-parameterized-tests
    auto& registry = ::testing::UnitTest::GetInstance()->parameterized_test_registry();
    auto* holder = registry.GetTestCasePatternHolder<T>(name, ::testing::internal::CodeLocation("C:\\Project\\egg\\build.sh", 7));
    return holder->AddTestSuiteInstantiation("", &T::generator, &T::name, file, line);
  }

  std::filesystem::path resolvePath(const std::string& devpath);
}

// The following is almost the same as
//  INSTANTIATE_TEST_CASE_P(Examples, TestExamples, ::testing::ValuesIn(TestExamples::find()), TestExamples::name);
// but produces prettier lists in MSVC Test Explorer
#define EGG_INSTANTIATE_TEST_CASE_P(test_case_name) \
  static int test_case_name##Registration GTEST_ATTRIBUTE_UNUSED_ = egg::test::registerTestCase<test_case_name>(#test_case_name, __FILE__, __LINE__);

// Add some useful extra macros
#define ASSERT_CONTAINS(haystack, needle) ASSERT_PRED_FORMAT2(egg::test::assertContains, haystack, needle)
#define ASSERT_NOTCONTAINS(haystack, needle) ASSERT_PRED_FORMAT2(egg::test::assertNotContains, haystack, needle)
#define ASSERT_STARTSWITH(haystack, needle) ASSERT_PRED_FORMAT2(egg::test::assertStartsWith, haystack, needle)
#define ASSERT_ENDSWITH(haystack, needle) ASSERT_PRED_FORMAT2(egg::test::assertEndsWith, haystack, needle)
#define ASSERT_PRINT(expected, value) ASSERT_PRED_FORMAT2(egg::test::assertPrint, expected, value)

// See https://stackoverflow.com/a/43569017
#define ASSERT_THROW_E(statement, expected_exception, caught) \
  try \
  { \
    GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);\
    FAIL() << "Expected: " #statement " throws an exception of type " #expected_exception ".\n  Actual: it throws nothing."; \
  } \
  catch (const expected_exception& e) \
  { \
    GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(caught); \
  } \
  catch (...) \
  { \
    FAIL() << "Expected: " #statement " throws an exception of type " #expected_exception ".\n  Actual: it throws a different type."; \
  }

#define ASSERT_FAILS(statement) EXPECT_FATAL_FAILURE(statement, "")
