#include "gtest.h"

#pragma warning(push, 3)
//#pragma warning(disable: 4365 4514 4571 4623 4625 4626 4710 4774 4820 5026 5027)
#pragma warning(disable: 4571)
#include "src/gtest-all.cc"
#pragma warning(pop)


GTEST_API_ int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
