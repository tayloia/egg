#include "ovum/test.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"

namespace {
  std::string expectedStub() {
    if (egg::ovum::os::file::slash() == '/') {
      return "egg-testsuite";
    }
    return "ovum-test";
  }
}
TEST(TestOS_Embed, GetExecutableFilename) {
  ASSERT_EQ(expectedStub() + ".exe", egg::ovum::os::embed::getExecutableFilename());
}

TEST(TestOS_Embed, GetExecutableStub) {
  ASSERT_EQ(expectedStub(), egg::ovum::os::embed::getExecutableStub());
}
