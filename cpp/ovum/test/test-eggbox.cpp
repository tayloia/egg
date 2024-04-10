#include "ovum/test.h"
#include "ovum/eggbox.h"
#include "ovum/os-file.h"

struct TestEggbox : public testing::Test {
  // See https://github.com/google/googletest/blob/main/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-suite
  static void SetUpTestCase() {
    temporaryDirPath = egg::ovum::os::file::createTemporaryDirectory("TestEggbox.", 100);
    actualDirPath = egg::ovum::os::file::getDevelopmentDirectory() + "cpp/data/zip/actual";
    actualZipPath = temporaryDirPath + "actual.zip";
    actualExePath = temporaryDirPath + "actual.exe";
    zip.entries = egg::ovum::EggboxFactory::createZipFileFromDirectory(actualZipPath, actualDirPath, zip.compressed, zip.uncompressed);
  }
  static void TearDownTestCase() {
    // Temporary directory remove at exit
  }
  void assertFileEntry(const std::string& expected_name, const std::string& expected_subpath, egg::ovum::IEggboxFileEntry::Kind expected_kind, const std::shared_ptr<egg::ovum::IEggboxFileEntry>& entry) {
    ASSERT_NE(nullptr, entry);
    ASSERT_EQ(expected_name, entry->getName());
    ASSERT_EQ(expected_subpath, entry->getSubpath());
    ASSERT_EQ(expected_kind, entry->getKind());
  }
  void assertFileEntries(egg::ovum::IEggbox& eggbox) {
    this->assertFileEntry("egg.png", "egg.png", egg::ovum::IEggboxFileEntry::Kind::File, eggbox.findFileEntryByIndex(0));
    this->assertFileEntry("empty.dat", "empty.dat", egg::ovum::IEggboxFileEntry::Kind::File, eggbox.findFileEntryByIndex(1));
    this->assertFileEntry("folder", "folder", egg::ovum::IEggboxFileEntry::Kind::Directory, eggbox.findFileEntryByIndex(2));
    this->assertFileEntry("jabberwocky.txt", "jabberwocky.txt", egg::ovum::IEggboxFileEntry::Kind::File, eggbox.findFileEntryByIndex(3));
    ASSERT_EQ(nullptr, eggbox.findFileEntryByIndex(4));
    this->assertFileEntry("greeting.txt", "folder/greeting.txt", egg::ovum::IEggboxFileEntry::Kind::File, eggbox.findFileEntryBySubpath("folder/greeting.txt"));
    ASSERT_EQ(nullptr, eggbox.findFileEntryBySubpath("folder/missing/unknown.dat"));
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

TEST_F(TestEggbox, OpenDirectory) {
  auto eggbox = egg::ovum::EggboxFactory::openDirectory(actualDirPath);
  ASSERT_NE(nullptr, eggbox);
  ASSERT_EQ(actualDirPath, eggbox->getResourcePath(nullptr));
  std::string example = "example/path";
  ASSERT_EQ(actualDirPath + "/example/path", eggbox->getResourcePath(&example));
  assertFileEntries(*eggbox);
}
