#include "ovum/test.h"
#include "ovum/file.h"
#include "ovum/os-zip.h"

#include <iostream>

namespace {
  std::string slurp(std::istream& stream) {
    std::stringstream ss;
    ss << stream.rdbuf();
    return ss.str();
  }
}

TEST(TestOS_Zip, CreateFactory) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
}

TEST(TestOS_Zip, GetFactoryVersion) {
  // TODO keep up-to-date
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  ASSERT_EQ("9.1.15", factory->getVersion());
}

TEST(TestOS_Zip, OpenFile) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
}

TEST(TestOS_Zip, OpenFileMissing) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto missing = egg::ovum::File::resolvePath("~/cpp/data/missing.zip");
  ASSERT_THROW_E(factory->openFile(missing), std::runtime_error, ASSERT_STARTSWITH(e.what(), "Zip file not found: "));
}

TEST(TestOS_Zip, OpenFileInvalid) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto invalid = egg::ovum::File::resolvePath("~/cpp/data/jabberwocky.txt");
  ASSERT_THROW_E(factory->openFile(invalid), std::runtime_error, ASSERT_STARTSWITH(e.what(), "Invalid zip file: "));
}

TEST(TestOS_Zip, GetComment) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
  ASSERT_EQ("Twas brillig, and the slithy toves", zip->getComment());
}

TEST(TestOS_Zip, GetFileEntryByIndex) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
  auto count = zip->getFileEntryCount();
  ASSERT_EQ(4u, count);
  size_t jabberwockies = 0;
  for (size_t index = 0; index < count; ++index) {
    auto entry = zip->getFileEntryByIndex(index);
    ASSERT_NE(nullptr, entry);
    std::cout << entry->getName() << std::endl;
    ASSERT_FALSE(entry->getName().empty());
    if (entry->getName() == "poem/jabberwocky.txt") {
      ++jabberwockies;
    }
    ASSERT_LE(entry->getCompressedBytes(), entry->getUncompressedBytes());
  }
  ASSERT_EQ(1u, jabberwockies);
}

TEST(TestOS_Zip, GetFileEntryByName) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
  auto entry = zip->getFileEntryByName("poem/jabberwocky.txt");
  ASSERT_NE(nullptr, entry);
  ASSERT_EQ("poem/jabberwocky.txt", entry->getName());
  ASSERT_EQ(493u, entry->getCompressedBytes());
  ASSERT_EQ(1008u, entry->getUncompressedBytes());
  ASSERT_EQ(0xC4458520u, entry->getCRC32());
}

TEST(TestOS_Zip, GetFileEntryByNameMissing) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
  auto entry = zip->getFileEntryByName("missing.txt");
  ASSERT_EQ(nullptr, entry);
}

TEST(TestOS_Zip, GetReadStream) {
  auto factory = egg::ovum::os::zip::createFactory();
  ASSERT_NE(nullptr, factory);
  auto zip = factory->openFile(egg::ovum::File::resolvePath("~/cpp/data/egg.zip"));
  ASSERT_NE(nullptr, zip);
  auto entry = zip->getFileEntryByName("poem/jabberwocky.txt");
  ASSERT_NE(nullptr, entry);
  auto& stream = entry->getReadStream();
  auto actual = slurp(stream);
  ASSERT_EQ(entry->getUncompressedBytes(), actual.size());
  auto expected = egg::ovum::File::slurp("~/cpp/data/jabberwocky.txt");
  ASSERT_EQ(expected, actual);
}
