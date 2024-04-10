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
    embedded.compressed = egg::ovum::EggboxFactory::createSandwichFromFile(actualExePath, actualZipPath, false);
  }
  static void TearDownTestCase() {
    // Temporary directory remove at exit
  }
  void assertFileEntry(const std::string& expected_name, const std::string& expected_subpath, const std::shared_ptr<egg::ovum::IEggboxFileEntry>& entry) {
    ASSERT_NE(nullptr, entry);
    ASSERT_EQ(expected_name, entry->getName());
    ASSERT_EQ(expected_subpath, entry->getSubpath());
  }
  void assertFileEntries(egg::ovum::IEggbox& eggbox) {
    this->assertFileEntry("egg.png", "egg.png", eggbox.findFileEntryByIndex(0));
    this->assertFileEntry("empty.dat", "empty.dat", eggbox.findFileEntryByIndex(1));
    this->assertFileEntry("empty.dat", "folder/empty.dat", eggbox.findFileEntryByIndex(2));
    this->assertFileEntry("greeting.txt", "folder/greeting.txt", eggbox.findFileEntryByIndex(3));
    this->assertFileEntry("jabberwocky.txt", "jabberwocky.txt", eggbox.findFileEntryByIndex(4));
    ASSERT_EQ(nullptr, eggbox.findFileEntryByIndex(5));
    this->assertFileEntry("greeting.txt", "folder/greeting.txt", eggbox.findFileEntryBySubpath("folder/greeting.txt"));
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
  inline static struct {
    uint64_t compressed;
  } embedded;
};

TEST_F(TestEggbox, CreateFileFromDirectory) {
  ASSERT_EQ(5u, zip.entries);
  ASSERT_EQ(28190u, zip.compressed);
  ASSERT_EQ(28270u, zip.uncompressed);
}

TEST_F(TestEggbox, OpenDirectory) {
  auto eggbox = egg::ovum::EggboxFactory::openDirectory(actualDirPath);
  ASSERT_NE(nullptr, eggbox);
  auto expectedResourcePath = actualDirPath;
  ASSERT_EQ(expectedResourcePath, eggbox->getResourcePath(nullptr));
  std::string example = "example/path";
  ASSERT_EQ(expectedResourcePath + "/example/path", eggbox->getResourcePath(&example));
  assertFileEntries(*eggbox);
}

TEST_F(TestEggbox, OpenDirectoryInvalid) {
  auto path = actualDirPath + "/egg.png";
  ASSERT_THROW_E(egg::ovum::EggboxFactory::openDirectory(path), egg::ovum::Exception, {
    ASSERT_STARTSWITH(e.what(), "Eggbox path is not a directory");
    ASSERT_EQ(path, e.get("path"));
    ASSERT_EQ(egg::ovum::os::file::denormalizePath(path, false), e.get("native"));
  });
}

TEST_F(TestEggbox, OpenDirectoryMissing) {
  auto path = actualDirPath + "/missing";
  ASSERT_THROW_E(egg::ovum::EggboxFactory::openDirectory(path), egg::ovum::Exception, {
    ASSERT_STARTSWITH(e.what(), "Eggbox directory does not exist");
    ASSERT_EQ(path, e.get("path"));
    ASSERT_EQ(egg::ovum::os::file::denormalizePath(path, false), e.get("native"));
  });
}

TEST_F(TestEggbox, OpenEmbedded) {
  auto eggbox = egg::ovum::EggboxFactory::openEmbedded(actualExePath);
  ASSERT_NE(nullptr, eggbox);
  auto expectedResourcePath = actualExePath + "//~EGGBOX";
  ASSERT_EQ(expectedResourcePath, eggbox->getResourcePath(nullptr));
  std::string example = "example/path";
  ASSERT_EQ(expectedResourcePath + "//example/path", eggbox->getResourcePath(&example));
  assertFileEntries(*eggbox);
}

TEST_F(TestEggbox, OpenEmbeddedInvalidExecutable) {
  auto path = actualDirPath + "/egg.png";
  ASSERT_THROW_E(egg::ovum::EggboxFactory::openEmbedded(path), egg::ovum::Exception, {
    ASSERT_STARTSWITH(e.what(), "Unable to find eggbox resource in executable");
    ASSERT_EQ(path, e.get("executable"));
    ASSERT_EQ(egg::ovum::os::file::denormalizePath(path, false), e.get("native"));
  });
}

TEST_F(TestEggbox, OpenEmbeddedMissingExecutable) {
  auto path = actualDirPath + "/missing";
  ASSERT_THROW_E(egg::ovum::EggboxFactory::openEmbedded(path), egg::ovum::Exception, {
    ASSERT_STARTSWITH(e.what(), "Eggbox executable does not exist");
    ASSERT_EQ(path, e.get("executable"));
    ASSERT_EQ(egg::ovum::os::file::denormalizePath(path, false), e.get("native"));
  });
}

TEST_F(TestEggbox, OpenEmbeddedMissingResource) {
  ASSERT_THROW_E(egg::ovum::EggboxFactory::openEmbedded(actualExePath, "MISSING"), egg::ovum::Exception, {
    ASSERT_STARTSWITH(e.what(), "Unable to find eggbox resource in executable");
    ASSERT_EQ(actualExePath, e.get("executable"));
    ASSERT_EQ(egg::ovum::os::file::denormalizePath(actualExePath, false), e.get("native"));
  });
}

TEST_F(TestEggbox, OpenZipFile) {
  auto eggbox = egg::ovum::EggboxFactory::openZipFile(actualZipPath);
  ASSERT_NE(nullptr, eggbox);
  auto expectedResourcePath = actualZipPath;
  ASSERT_EQ(expectedResourcePath, eggbox->getResourcePath(nullptr));
  std::string example = "example/path";
  ASSERT_EQ(expectedResourcePath + "//example/path", eggbox->getResourcePath(&example));
  assertFileEntries(*eggbox);
}

TEST_F(TestEggbox, OpenZipFileInvalid) {
  auto path = actualDirPath + "/egg.png";
  ASSERT_THROW_E(egg::ovum::EggboxFactory::openZipFile(path), egg::ovum::Exception, {
    ASSERT_STARTSWITH(e.what(), "Invalid zip file");
    ASSERT_EQ(path, e.get("path"));
  });
}

TEST_F(TestEggbox, OpenZipFileMissing) {
  auto path = actualDirPath + "/missing";
  ASSERT_THROW_E(egg::ovum::EggboxFactory::openZipFile(path), egg::ovum::Exception, {
    ASSERT_STARTSWITH(e.what(), "Eggbox zip file does not exist");
    ASSERT_EQ(path, e.get("path"));
    ASSERT_EQ(egg::ovum::os::file::denormalizePath(path, false), e.get("native"));
  });
}
