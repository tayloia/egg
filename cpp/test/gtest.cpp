#include "yolk.h"
#include "gtest.h"

// Reduce the warning level in MSVC
#if defined(_MSC_VER)
#pragma warning(push, 3)
#pragma warning(disable: 4571)
#endif

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

int main(int argc, char** argv) {
  return egg::yolk::test::main(argc, argv);
}
