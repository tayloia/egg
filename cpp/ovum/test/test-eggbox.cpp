#include "ovum/test.h"
#include "ovum/eggbox.h"
#include "ovum/os-file.h"

struct TestEggbox : public testing::Test {
  // See https://github.com/google/googletest/blob/main/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-suite
  static void SetUpTestCase() {
    temporaryDirPath = egg::ovum::os::file::createTemporaryDirectory("TestEggbox.", 100);
    actualDirPath = egg::ovum::os::file::getDevelopmentDirectory() + "cpp/data/zip/actual/";
    actualZipPath = temporaryDirPath + "actual.zip";
    actualExePath = temporaryDirPath + "actual.exe";
    zip.entries = egg::ovum::EggboxFactory::createZipFileFromDirectory(actualZipPath, actualDirPath, zip.compressed, zip.uncompressed);
  }
  static void TearDownTestCase() {
    // Temporary directory remove at exit
  }
  inline static std::string temporaryDirPath;
  inline static std::string actualDirPath;
  inline static std::string actualZipPath;
  inline static std::string actualExePath;
  inline static struct {
    size_t entries;
    uint64_t compressed;
    uint64_t uncompressed;
  } zip = {};
};

TEST_F(TestEggbox, CreateFileFromDirectory) {
  ASSERT_EQ(5u, zip.entries);
  ASSERT_EQ(28190u, zip.compressed);
  ASSERT_EQ(28270u, zip.uncompressed);
}

/* WIBBLE
TEST_F(TestEggbox, OpenDirectory) {
  auto eggbox = egg::ovum::EggboxFactory::openDirectory(actualDirPath);
  assert(eggbox != nullptr);
}
*/