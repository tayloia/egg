#include "yolk.h"

TEST(TestFiles, NormalizePath) {
  EXPECT_EQ("/path/to/file", egg::yolk::File::normalizePath("/path/to/file"));
  EXPECT_EQ("/path/to/file/", egg::yolk::File::normalizePath("/path/to/file/"));
  EXPECT_EQ("/path/to/file/", egg::yolk::File::normalizePath("/path/to/file", true));
  EXPECT_EQ("/path/to/file/", egg::yolk::File::normalizePath("/path/to/file/", true));
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  EXPECT_EQ("c:/path/to/file", egg::yolk::File::normalizePath("C:\\Path\\to\\file"));
  EXPECT_EQ("c:/path/to/file/", egg::yolk::File::normalizePath("C:\\Path\\to\\file\\"));
  EXPECT_EQ("c:/path/to/file/", egg::yolk::File::normalizePath("C:\\Path\\to\\file", true));
  EXPECT_EQ("c:/path/to/file/", egg::yolk::File::normalizePath("C:\\Path\\to\\file\\", true));
#endif
}

TEST(TestFiles, DenormalizePath) {
  EXPECT_EQ("C:\\Path\\to\\file", egg::yolk::File::denormalizePath("C:\\Path\\to\\file"));
  EXPECT_EQ("C:\\Path\\to\\file\\", egg::yolk::File::denormalizePath("C:\\Path\\to\\file\\"));
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  EXPECT_EQ("C:\\Path\\to\\file\\", egg::yolk::File::denormalizePath("C:\\Path\\to\\file", true));
  EXPECT_EQ("C:\\Path\\to\\file\\", egg::yolk::File::denormalizePath("C:\\Path\\to\\file\\", true));
  EXPECT_EQ("\\path\\to\\file", egg::yolk::File::denormalizePath("/path/to/file"));
  EXPECT_EQ("\\path\\to\\file\\", egg::yolk::File::denormalizePath("/path/to/file/"));
  EXPECT_EQ("\\path\\to\\file\\", egg::yolk::File::denormalizePath("/path/to/file", true));
  EXPECT_EQ("\\path\\to\\file\\", egg::yolk::File::denormalizePath("/path/to/file/", true));
#else
  EXPECT_EQ("C:\\Path\\to\\file/", egg::yolk::File::denormalizePath("C:\\Path\\to\\file", true));
  EXPECT_EQ("C:\\Path\\to\\file\\/", egg::yolk::File::denormalizePath("C:\\Path\\to\\file\\", true));
  EXPECT_EQ("/path/to/file", egg::yolk::File::denormalizePath("/path/to/file"));
  EXPECT_EQ("/path/to/file/", egg::yolk::File::denormalizePath("/path/to/file/"));
  EXPECT_EQ("/path/to/file/", egg::yolk::File::denormalizePath("/path/to/file", true));
  EXPECT_EQ("/path/to/file/", egg::yolk::File::denormalizePath("/path/to/file/", true));
#endif
}

TEST(TestFiles, GetTildeDirectory) {
  auto tilde = egg::yolk::File::getTildeDirectory();
  EXPECT_GT(tilde.size(), 0u);
  EXPECT_EQ('/', tilde.back());
}

TEST(TestFiles, ResolvePath) {
  auto resolved = egg::yolk::File::resolvePath("~/path/to/file");
  EXPECT_NE('~', resolved[0]);
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  EXPECT_EQ("\\path\\to\\file", egg::yolk::File::resolvePath("/path/to/file"));
  EXPECT_TRUE(egg::yolk::String::endsWith(resolved, "\\path\\to\\file"));
#else
  EXPECT_EQ("/path/to/file", egg::yolk::File::resolvePath("/path/to/file"));
  EXPECT_TRUE(egg:yolk::String::endsWith(resolved, "/path/to/file"));
#endif
}
