#include "yolk.h"

TEST(TestFiles, NormalizePath) {
  ASSERT_EQ("/path/to/file", egg::yolk::File::normalizePath("/path/to/file"));
  ASSERT_EQ("/path/to/file/", egg::yolk::File::normalizePath("/path/to/file/"));
  ASSERT_EQ("/path/to/file/", egg::yolk::File::normalizePath("/path/to/file", true));
  ASSERT_EQ("/path/to/file/", egg::yolk::File::normalizePath("/path/to/file/", true));
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  ASSERT_EQ("c:/path/to/file", egg::yolk::File::normalizePath("C:\\Path\\to\\file"));
  ASSERT_EQ("c:/path/to/file/", egg::yolk::File::normalizePath("C:\\Path\\to\\file\\"));
  ASSERT_EQ("c:/path/to/file/", egg::yolk::File::normalizePath("C:\\Path\\to\\file", true));
  ASSERT_EQ("c:/path/to/file/", egg::yolk::File::normalizePath("C:\\Path\\to\\file\\", true));
#endif
}

TEST(TestFiles, DenormalizePath) {
  ASSERT_EQ("C:\\Path\\to\\file", egg::yolk::File::denormalizePath("C:\\Path\\to\\file"));
  ASSERT_EQ("C:\\Path\\to\\file\\", egg::yolk::File::denormalizePath("C:\\Path\\to\\file\\"));
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  ASSERT_EQ("C:\\Path\\to\\file\\", egg::yolk::File::denormalizePath("C:\\Path\\to\\file", true));
  ASSERT_EQ("C:\\Path\\to\\file\\", egg::yolk::File::denormalizePath("C:\\Path\\to\\file\\", true));
  ASSERT_EQ("\\path\\to\\file", egg::yolk::File::denormalizePath("/path/to/file"));
  ASSERT_EQ("\\path\\to\\file\\", egg::yolk::File::denormalizePath("/path/to/file/"));
  ASSERT_EQ("\\path\\to\\file\\", egg::yolk::File::denormalizePath("/path/to/file", true));
  ASSERT_EQ("\\path\\to\\file\\", egg::yolk::File::denormalizePath("/path/to/file/", true));
#else
  ASSERT_EQ("C:\\Path\\to\\file/", egg::yolk::File::denormalizePath("C:\\Path\\to\\file", true));
  ASSERT_EQ("C:\\Path\\to\\file\\/", egg::yolk::File::denormalizePath("C:\\Path\\to\\file\\", true));
  ASSERT_EQ("/path/to/file", egg::yolk::File::denormalizePath("/path/to/file"));
  ASSERT_EQ("/path/to/file/", egg::yolk::File::denormalizePath("/path/to/file/"));
  ASSERT_EQ("/path/to/file/", egg::yolk::File::denormalizePath("/path/to/file", true));
  ASSERT_EQ("/path/to/file/", egg::yolk::File::denormalizePath("/path/to/file/", true));
#endif
}

TEST(TestFiles, GetTildeDirectory) {
  auto tilde = egg::yolk::File::getTildeDirectory();
  ASSERT_GT(tilde.size(), 0u);
  ASSERT_EQ('/', tilde.back());
}

TEST(TestFiles, ResolvePath) {
  auto resolved = egg::yolk::File::resolvePath("~/path/to/file");
  ASSERT_NE('~', resolved[0]);
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  ASSERT_EQ("\\path\\to\\file", egg::yolk::File::resolvePath("/path/to/file"));
  ASSERT_ENDSWITH(resolved, "\\path\\to\\file");
#else
  ASSERT_EQ("/path/to/file", egg::yolk::File::resolvePath("/path/to/file"));
  ASSERT_ENDSWITH(resolved, "/path/to/file");
#endif
}
