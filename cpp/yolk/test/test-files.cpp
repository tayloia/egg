#include "yolk/test.h"
#include "ovum/file.h"

TEST(TestFiles, NormalizePath) {
  ASSERT_EQ("/path/to/file", egg::ovum::File::normalizePath("/path/to/file"));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::normalizePath("/path/to/file/"));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::normalizePath("/path/to/file", true));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::normalizePath("/path/to/file/", true));
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  ASSERT_EQ("c:/path/to/file", egg::ovum::File::normalizePath("C:\\Path\\to\\file"));
  ASSERT_EQ("c:/path/to/file/", egg::ovum::File::normalizePath("C:\\Path\\to\\file\\"));
  ASSERT_EQ("c:/path/to/file/", egg::ovum::File::normalizePath("C:\\Path\\to\\file", true));
  ASSERT_EQ("c:/path/to/file/", egg::ovum::File::normalizePath("C:\\Path\\to\\file\\", true));
#endif
}

TEST(TestFiles, DenormalizePath) {
  ASSERT_EQ("C:\\Path\\to\\file", egg::ovum::File::denormalizePath("C:\\Path\\to\\file"));
  ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\"));
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file", true));
  ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\", true));
  ASSERT_EQ("\\path\\to\\file", egg::ovum::File::denormalizePath("/path/to/file"));
  ASSERT_EQ("\\path\\to\\file\\", egg::ovum::File::denormalizePath("/path/to/file/"));
  ASSERT_EQ("\\path\\to\\file\\", egg::ovum::File::denormalizePath("/path/to/file", true));
  ASSERT_EQ("\\path\\to\\file\\", egg::ovum::File::denormalizePath("/path/to/file/", true));
#else
  ASSERT_EQ("C:\\Path\\to\\file/", egg::ovum::File::denormalizePath("C:\\Path\\to\\file", true));
  ASSERT_EQ("C:\\Path\\to\\file\\/", egg::ovum::File::denormalizePath("C:\\Path\\to\\file\\", true));
  ASSERT_EQ("/path/to/file", egg::ovum::File::denormalizePath("/path/to/file"));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::denormalizePath("/path/to/file/"));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::denormalizePath("/path/to/file", true));
  ASSERT_EQ("/path/to/file/", egg::ovum::File::denormalizePath("/path/to/file/", true));
#endif
}

TEST(TestFiles, GetTildeDirectory) {
  auto tilde = egg::ovum::File::getTildeDirectory();
  ASSERT_GT(tilde.size(), 0u);
  ASSERT_EQ('/', tilde.back());
}

TEST(TestFiles, GetCurrentDirectory) {
  auto cwd = egg::ovum::File::getCurrentDirectory();
  ASSERT_GT(cwd.size(), 0u);
  ASSERT_EQ('/', cwd.back());
}

TEST(TestFiles, ResolvePath) {
  auto resolved = egg::ovum::File::resolvePath("~/path/to/file");
  ASSERT_NE('~', resolved[0]);
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  ASSERT_EQ("\\path\\to\\file", egg::ovum::File::resolvePath("/path/to/file"));
  ASSERT_ENDSWITH(resolved, "\\path\\to\\file");
#else
  ASSERT_EQ("/path/to/file", egg::ovum::File::resolvePath("/path/to/file"));
  ASSERT_ENDSWITH(resolved, "/path/to/file");
#endif
}

TEST(TestFiles, ReadDirectory) {
  auto filenames = egg::ovum::File::readDirectory("~/data");
  ASSERT_FALSE(filenames.empty());
  filenames = egg::ovum::File::readDirectory("~/missing-in-action");
  ASSERT_TRUE(filenames.empty());
}

TEST(TestFiles, KindUnknown) {
  ASSERT_EQ(egg::ovum::File::Kind::Unknown, egg::ovum::File::getKind("~/missing-in-action"));
}

TEST(TestFiles, KindDirectory) {
  ASSERT_EQ(egg::ovum::File::Kind::Directory, egg::ovum::File::getKind("~/data"));
}

TEST(TestFiles, KindFile) {
  ASSERT_EQ(egg::ovum::File::Kind::File, egg::ovum::File::getKind("~/data/egg.png"));
}
