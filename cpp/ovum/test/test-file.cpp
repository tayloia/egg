#include "ovum/test.h"
#include "ovum/file.h"
#include "ovum/os-file.h"

TEST(TestFile, NormalizePath) {
  ASSERT_EQ("/path/to/file", egg::ovum::File::normalizePath("/path/to/file"));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::normalizePath("/path/to/file/"));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::normalizePath("/path/to/file", true));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::normalizePath("/path/to/file/", true));
  if (egg::ovum::os::file::slash() == '\\') {
    ASSERT_EQ("c:/path/to/file", egg::ovum::File::normalizePath("C:\\Path\\to\\file"));
    ASSERT_EQ("c:/path/to/file/", egg::ovum::File::normalizePath("C:\\Path\\to\\file\\"));
    ASSERT_EQ("c:/path/to/file/", egg::ovum::File::normalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("c:/path/to/file/", egg::ovum::File::normalizePath("C:\\Path\\to\\file\\", true));
  }
}

TEST(TestFile, DenormalizePath) {
  ASSERT_EQ("C:\\Path\\to\\file", egg::ovum::File::denormalizePath("C:\\Path\\to\\file"));
  ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\"));
  if (egg::ovum::os::file::slash() == '\\') {
    ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\", true));
    ASSERT_EQ("\\path\\to\\file", egg::ovum::File::denormalizePath("/path/to/file"));
    ASSERT_EQ("\\path\\to\\file\\", egg::ovum::File::denormalizePath("/path/to/file/"));
    ASSERT_EQ("\\path\\to\\file\\", egg::ovum::File::denormalizePath("/path/to/file", true));
    ASSERT_EQ("\\path\\to\\file\\", egg::ovum::File::denormalizePath("/path/to/file/", true));
  } else {
    ASSERT_EQ("C:\\Path\\to\\file/", egg::ovum::File::denormalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("C:\\Path\\to\\file\\/", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\", true));
    ASSERT_EQ("/path/to/file", egg::ovum::File::denormalizePath("/path/to/file"));
    ASSERT_EQ("/path/to/file/", egg::ovum::File::denormalizePath("/path/to/file/"));
    ASSERT_EQ("/path/to/file/", egg::ovum::File::denormalizePath("/path/to/file", true));
    ASSERT_EQ("/path/to/file/", egg::ovum::File::denormalizePath("/path/to/file/", true));
  }
}

TEST(TestFile, ResolvePath) {
  auto resolved = egg::ovum::File::resolvePath("~/path/to/file");
  ASSERT_NE('~', resolved[0]);
  if (egg::ovum::os::file::slash() == '\\') {
    ASSERT_EQ("\\path\\to\\file", egg::ovum::File::resolvePath("/path/to/file"));
    ASSERT_ENDSWITH(resolved, "\\path\\to\\file");
  } else {
    ASSERT_EQ("/path/to/file", egg::ovum::File::resolvePath("/path/to/file"));
    ASSERT_ENDSWITH(resolved, "/path/to/file");
  }
}

TEST(TestFile, ReadDirectory) {
  auto filenames = egg::ovum::File::readDirectory("~/data");
  ASSERT_FALSE(filenames.empty());
  filenames = egg::ovum::File::readDirectory("~/missing-in-action");
  ASSERT_TRUE(filenames.empty());
}

TEST(TestFile, KindUnknown) {
  ASSERT_EQ(egg::ovum::File::Kind::Unknown, egg::ovum::File::getKind("~/missing-in-action"));
}

TEST(TestFile, KindDirectory) {
  ASSERT_EQ(egg::ovum::File::Kind::Directory, egg::ovum::File::getKind("~/data"));
}

TEST(TestFile, KindFile) {
  ASSERT_EQ(egg::ovum::File::Kind::File, egg::ovum::File::getKind("~/data/egg.png"));
}
