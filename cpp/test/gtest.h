#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(push, 3)
#pragma warning(disable: 4365 4623 4625 4626 5026 5027)
#endif
#include "gtest/gtest.h"
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#pragma warning(pop)
#endif

// Add some useful extra macros
#define ASSERT_STARTSWITH(haystack, needle) ASSERT_TRUE(egg::yolk::String::startsWith(haystack, needle))
#define ASSERT_ENDSWITH(haystack, needle)   ASSERT_TRUE(egg::yolk::String::endsWith(haystack, needle))

namespace egg::yolk::test {
  // This is the default entry-point for Google Test runners
  int main(int argc, char** argv);
}
