#include "ovum/test.h"
#include "ovum/os-file.h"
#include "ovum/zip.h"

TEST(TestZip, CreateFileFromDirectory) {
  auto target = egg::ovum::os::file::createTemporaryFile("actual.", ".zip", 100);
  auto directory = egg::ovum::os::file::getDevelopmentDirectory() + "cpp/data/zip/actual";
  uint64_t compressed, uncompressed;
  auto entries = egg::ovum::Zip::createFileFromDirectory(target, directory, compressed, uncompressed);
  ASSERT_EQ(5u, entries);
  ASSERT_EQ(28190u, compressed);
  ASSERT_EQ(28270u, uncompressed);
}
