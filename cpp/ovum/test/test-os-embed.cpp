#include "ovum/test.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"
#include "ovum/file.h"

namespace {
  std::string expectedStub() {
    if (egg::ovum::os::file::slash() == '/') {
      return "egg-test";
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
  // The first time should succeed
  egg::ovum::os::embed::cloneExecutable(clone, false);
  ASSERT_EQ(egg::ovum::File::Kind::File, egg::ovum::File::getKind(clone));
  // The second time should fail
  ASSERT_THROW_E(egg::ovum::os::embed::cloneExecutable(clone, false), egg::ovum::Exception, ASSERT_CONTAINS(e.what(), "exists"));
  // The third time should overwrite
  egg::ovum::os::embed::cloneExecutable(clone, true);
}

TEST(TestOS_Embed, FindResources) {
  std::string path = egg::ovum::os::file::getExecutablePath();
  auto resources = egg::ovum::os::embed::findResources(path);
  ASSERT_GT(resources.size(), 0u);
}

TEST(TestOS_Embed, UpdateResourceFromMemory) {
  auto tmpdir = egg::ovum::os::file::createTemporaryDirectory("egg-test-embed-", 100);
  auto cloned = tmpdir + "cloned.exe";
  egg::ovum::os::embed::cloneExecutable(cloned, false);

  auto resources = egg::ovum::os::embed::findResources(cloned);
  auto before = resources.size();
  ASSERT_LT(0u, before);

  egg::ovum::os::embed::updateResourceFromMemory(cloned, "PROGBITS", "GREETING", "Hello, world!", 13);

  resources = egg::ovum::os::embed::findResources(cloned);
  auto after = resources.size();
  ASSERT_EQ(before + 1, after);
  auto found = std::find_if(resources.begin(), resources.end(), [](const egg::ovum::os::embed::Resource& candidate) {
    return candidate.label == "GREETING";
  });
  ASSERT_NE(resources.end(), found);
  ASSERT_EQ("PROGBITS", found->type);
  ASSERT_EQ("GREETING", found->label);
  ASSERT_EQ(13u, found->bytes);

  resources = egg::ovum::os::embed::findResourcesByType(cloned, "PROGBITS");
  ASSERT_FALSE(resources.empty());
  auto count = 0;
  for (auto& entry : resources) {
    ASSERT_EQ("PROGBITS", entry.type);
    if (entry.label == "GREETING") {
      ASSERT_EQ(13u, entry.bytes);
      count++;
    }
  }
  ASSERT_EQ(1, count);

  auto resource = egg::ovum::os::embed::findResourceByName(cloned, "PROGBITS", "GREETING");
  ASSERT_NE(nullptr, resource);
  ASSERT_EQ("PROGBITS", resource->type);
  ASSERT_EQ("GREETING", resource->label);
  ASSERT_EQ(13u, resource->bytes);
  auto* memory = resource->lock();
  ASSERT_NE(nullptr, memory);
  ASSERT_EQ(0, std::memcmp(memory, "Hello, world!", 13));
  resource->unlock();

  egg::ovum::os::embed::updateResourceFromMemory(cloned, "PROGBITS", "GREETING", nullptr, 0);
  resource = egg::ovum::os::embed::findResourceByName(cloned, "PROGBITS", "GREETING");
  ASSERT_EQ(nullptr, resource);
}

TEST(TestOS_Embed, UpdateResourceFromFile) {
  auto tmpdir = egg::ovum::os::file::createTemporaryDirectory("egg-test-embed-", 100);
  auto cloned = tmpdir + "cloned.exe";
  egg::ovum::os::embed::cloneExecutable(cloned, false);

  auto resource = egg::ovum::os::embed::findResourceByName(cloned, "PROGBITS", "JABBERWOCKY");
  ASSERT_EQ(nullptr, resource);

  auto datapath = egg::ovum::File::resolvePath("~/cpp/data/jabberwocky.txt");
  auto datasize = egg::ovum::os::embed::updateResourceFromFile(cloned, "PROGBITS", "JABBERWOCKY", datapath);
  ASSERT_EQ(1008u, datasize);

  resource = egg::ovum::os::embed::findResourceByName(cloned, "PROGBITS", "JABBERWOCKY");
  ASSERT_NE(nullptr, resource);
  ASSERT_EQ("PROGBITS", resource->type);
  ASSERT_EQ("JABBERWOCKY", resource->label);
  ASSERT_EQ(1008u, resource->bytes);
  auto* memory = resource->lock();
  ASSERT_NE(nullptr, memory);
  std::string data(static_cast<const char*>(memory) + 3, 34);
  ASSERT_EQ("Twas brillig, and the slithy toves", data);
  resource->unlock();
}
