#include "ovum/test.h"
#include "ovum/eggbox.h"
#include "ovum/os-file.h"

// Reduce the warning level in MSVC
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(push, 3)
#pragma warning(disable: 5039 6011 6031 6387 26812 26495)
#endif

// Fix GCC GoogleTest issue 1521
#if EGG_PLATFORM == EGG_PLATFORM_GCC
// See https://github.com/google/googletest/issues/1521
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

// For this source file only: make sure the following is in our search path, re-enable language extensions and turn off precompiled headers
#include "src/gtest-all.cc"

// Restore warning level after GCC GoogleTest issue 1521
#if EGG_PLATFORM == EGG_PLATFORM_GCC
#pragma GCC diagnostic pop
#endif

// Restore the warning level in MSVC
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(pop)
// Google test for MSVC uses old POSIX names
// See https://docs.microsoft.com/en-gb/cpp/c-runtime-library/backward-compatibility
#pragma comment(lib, "oldnames.lib")
#endif

std::shared_ptr<egg::ovum::IEggbox> egg::test::Eggbox::createTest(const std::filesystem::path& subdir) {
  // Create an eggbox that just looks at a directory, but is a chain like egg::ovum::EggboxFactory::createDefault()
  auto chain = egg::ovum::EggboxFactory::createChain();
  chain->with(egg::ovum::EggboxFactory::openDirectory(egg::test::resolvePath(subdir)));
  return chain;
}

int egg::test::main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  // If you see '!! This test has probably CRASHED !!' when not debugging set the following to 'false'
  testing::GTEST_FLAG(break_on_failure) = false;
  return RUN_ALL_TESTS();
}

std::filesystem::path egg::test::resolvePath(const std::filesystem::path& path) {
  // Resolve a file path in normalized form
  if (path.is_absolute()) {
    throw std::runtime_error("Absolute path passed to egg::test::resolvePath()");
  }
  return egg::ovum::os::file::denormalizePath(egg::ovum::os::file::getDevelopmentDirectory() + path.string(), false);
}

::testing::AssertionResult egg::test::assertContains(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle) {
  if (egg::test::contains(haystack, needle)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
    << "haystack (" << haystack_expr << ") does not contain needle (" << needle_expr << ")\n"
    << "haystack is\n  " << ::testing::PrintToString(haystack) << "\n"
    << "needle is\n  " << ::testing::PrintToString(needle);
}

::testing::AssertionResult egg::test::assertNotContains(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle) {
  if (!egg::test::contains(haystack, needle)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
    << "haystack (" << haystack_expr << ") does contain needle (" << needle_expr << ")\n"
    << "haystack is\n  " << ::testing::PrintToString(haystack) << "\n"
    << "needle is\n  " << ::testing::PrintToString(needle);
}

::testing::AssertionResult egg::test::assertStartsWith(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle) {
  if (egg::test::startsWith(haystack, needle)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
    << "haystack (" << haystack_expr << ") does not start with needle (" << needle_expr << ")\n"
    << "haystack is\n  " << ::testing::PrintToString(haystack) << "\n"
    << "needle is\n  " << ::testing::PrintToString(needle);
}

::testing::AssertionResult egg::test::assertEndsWith(const char* haystack_expr, const char* needle_expr, const std::string& haystack, const std::string& needle) {
  if (egg::test::endsWith(haystack, needle)) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
    << "haystack (" << haystack_expr << ") does not end with needle (" << needle_expr << ")\n"
    << "haystack is\n  " << ::testing::PrintToString(haystack) << "\n"
    << "needle is\n  " << ::testing::PrintToString(needle);
}

int main(int argc, char** argv) {
  return egg::test::main(argc, argv);
}
