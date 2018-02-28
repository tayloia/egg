#pragma warning(push, 3)
#pragma warning(disable: 4365 4623 4625 4626 5026 5027)
#include "gtest/gtest.h"
#pragma warning(pop)

// Add some useful extra macros
#define ASSERT_STARTSWITH(haystack, needle) ASSERT_TRUE(egg::yolk::String::startsWith(haystack, needle))
#define ASSERT_ENDSWITH(haystack, needle)   ASSERT_TRUE(egg::yolk::String::endsWith(haystack, needle))

namespace egg::yolk::test {
  int main(int argc, char** argv);
}
