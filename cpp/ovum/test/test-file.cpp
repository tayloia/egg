#include "ovum/test.h"
#include "ovum/file.h"
#include "ovum/os-file.h"

TEST(TestFile, NormalizePath) {
  ASSERT_EQ("/path/to/file", egg::ovum::File::normalizePath("/path/to/file"));
  ASSERT_EQ("/path/to/file", egg::ovum::File::normalizePath("/path/to/file/"));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::normalizePath("/path/to/file", true));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::normalizePath("/path/to/file/", true));
  if (egg::ovum::os::file::slash() == '\\') {
    ASSERT_EQ("c:/path/to/file", egg::ovum::File::normalizePath("C:\\Path\\to\\file"));
    ASSERT_EQ("c:/path/to/file", egg::ovum::File::normalizePath("C:\\Path\\to\\file\\"));
    ASSERT_EQ("c:/path/to/file/", egg::ovum::File::normalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("c:/path/to/file/", egg::ovum::File::normalizePath("C:\\Path\\to\\file\\", true));
  }
}

TEST(TestFile, DenormalizePath) {
  if (egg::ovum::os::file::slash() == '\\') {
    ASSERT_EQ("C:\\Path\\to\\file", egg::ovum::File::denormalizePath("C:\\Path\\to\\file"));
    ASSERT_EQ("C:\\Path\\to\\file", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\"));
    ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\", true));
    ASSERT_EQ("\\path\\to\\file", egg::ovum::File::denormalizePath("/path/to/file"));
    ASSERT_EQ("\\path\\to\\file", egg::ovum::File::denormalizePath("/path/to/file/"));
    ASSERT_EQ("\\path\\to\\file\\", egg::ovum::File::denormalizePath("/path/to/file", true));
    ASSERT_EQ("\\path\\to\\file\\", egg::ovum::File::denormalizePath("/path/to/file/", true));
  } else {
    ASSERT_EQ("C:\\Path\\to\\file", egg::ovum::File::denormalizePath("C:\\Path\\to\\file"));
    ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\"));
    ASSERT_EQ("C:\\Path\\to\\file/", egg::ovum::File::denormalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("C:\\Path\\to\\file\\/", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\", true));
    ASSERT_EQ("/path/to/file", egg::ovum::File::denormalizePath("/path/to/file"));
    ASSERT_EQ("/path/to/file", egg::ovum::File::denormalizePath("/path/to/file/"));
    ASSERT_EQ("/path/to/file/", egg::ovum::File::denormalizePath("/path/to/file", true));
    ASSERT_EQ("/path/to/file/", egg::ovum::File::denormalizePath("/path/to/file/", true));
  }
}

TEST(TestFile, ReadDirectory) {
  auto filenames = egg::ovum::File::readDirectory(egg::test::resolvePath("data"));
  ASSERT_FALSE(filenames.empty());
  filenames = egg::ovum::File::readDirectory(egg::test::resolvePath("missing-in-action"));
  ASSERT_TRUE(filenames.empty());
}

TEST(TestFile, KindUnknown) {
  ASSERT_EQ(egg::ovum::File::Kind::Unknown, egg::ovum::File::getKind(egg::test::resolvePath("missing-in-action").string()));
}

TEST(TestFile, KindDirectory) {
  ASSERT_EQ(egg::ovum::File::Kind::Directory, egg::ovum::File::getKind(egg::test::resolvePath("data").string()));
}

TEST(TestFile, KindFile) {
  ASSERT_EQ(egg::ovum::File::Kind::File, egg::ovum::File::getKind(egg::test::resolvePath("data/egg.png").string()));
}

TEST(TestFile, Slurp) {
  auto jabberwocky = egg::ovum::File::slurp(egg::test::resolvePath("cpp/data/jabberwocky.txt").string());
  ASSERT_EQ(1008u, jabberwocky.size());
  ASSERT_CONTAINS(jabberwocky, "Twas brillig, and the slithy toves");
}
