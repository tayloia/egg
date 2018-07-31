#include "yolk/test.h"

// Reduce the warning level in MSVC
#if defined(_MSC_VER)
#pragma warning(push, 3)
#pragma warning(disable: 4571 5045)
#endif

// For this source file only: make sure the following is in our search path, disable language extensions and turn off precompiled headers
#include "src/gtest-all.cc"

// Restore the warning level in MSVC
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(pop)
// Google test for MSVC uses old POSIX names
// See https://docs.microsoft.com/en-gb/cpp/c-runtime-library/backward-compatibility
#pragma comment(lib, "oldnames.lib")
#endif

int egg::yolk::test::main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

::testing::AssertionResult egg::yolk::test::assertContains(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle) {
  if (String::contains(haystack, needle)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
    << "haystack (" << haystack_expr << ") does not contain needle (" << needle_expr << ")\n"
    << "haystack is\n  " << ::testing::PrintToString(haystack) << "\n"
    << "needle is\n  " << ::testing::PrintToString(needle);
}

::testing::AssertionResult egg::yolk::test::assertNotContains(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle) {
  if (!String::contains(haystack, needle)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
    << "haystack (" << haystack_expr << ") does contain needle (" << needle_expr << ")\n"
    << "haystack is\n  " << ::testing::PrintToString(haystack) << "\n"
    << "needle is\n  " << ::testing::PrintToString(needle);
}

::testing::AssertionResult egg::yolk::test::assertStartsWith(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle) {
  if (String::startsWith(haystack, needle)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
    << "haystack (" << haystack_expr << ") does not start with needle (" << needle_expr << ")\n"
    << "haystack is\n  " << ::testing::PrintToString(haystack) << "\n"
    << "needle is\n  " << ::testing::PrintToString(needle);
}

::testing::AssertionResult egg::yolk::test::assertEndsWith(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle) {
  if (String::endsWith(haystack, needle)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
    << "haystack (" << haystack_expr << ") does not end with needle (" << needle_expr << ")\n"
    << "haystack is\n  " << ::testing::PrintToString(haystack) << "\n"
    << "needle is\n  " << ::testing::PrintToString(needle);
}

int main(int argc, char** argv) {
  return egg::yolk::test::main(argc, argv);
}
