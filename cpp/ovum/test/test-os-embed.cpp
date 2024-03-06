#include "ovum/test.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"
#include "ovum/file.h"

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

TEST(TestOS_Embed, CloneExecutable) {
  auto tmpdir = egg::ovum::os::file::createTemporaryDirectory("egg-test-embed-", 100);
  auto clone = tmpdir + "cloned.exe";
  ASSERT_EQ(egg::ovum::File::Kind::Unknown, egg::ovum::File::getKind(clone));
  egg::ovum::os::embed::cloneExecutable(clone);
  ASSERT_EQ(egg::ovum::File::Kind::File, egg::ovum::File::getKind(clone));
}

TEST(TestOS_Embed, FindResources) {
  // WIBBLE std::string path = egg::ovum::os::file::getExecutablePath();
  std::string path = "c:/program files/microsoft visual studio/2022/community/common7/ide/devenv.exe";
  auto resources = egg::ovum::os::embed::findResources(path);
  ASSERT_GT(resources.size(), 0);
}

TEST(TestOS_Embed, AddResource) {
  auto tmpdir = egg::ovum::os::file::createTemporaryDirectory("egg-test-embed-", 100);
  auto cloned = tmpdir + "cloned.exe";
  egg::ovum::os::embed::cloneExecutable(cloned);
  egg::ovum::os::embed::addResource(cloned, "WIBBLE", "Hello world!", 12);
}
