#include "ovum/test.h"
#include "ovum/os-file.h"
#include "ovum/file.h"

TEST(TestOS_File, NormalizePath) {
  ASSERT_EQ("/path/to/file", egg::ovum::os::file::normalizePath("/path/to/file", false));
  ASSERT_EQ("/path/to/file/", egg::ovum::os::file::normalizePath("/path/to/file/", false));
  ASSERT_EQ("/path/to/file/", egg::ovum::os::file::normalizePath("/path/to/file", true));
  ASSERT_EQ("/path/to/file/", egg::ovum::os::file::normalizePath("/path/to/file/", true));
  if (egg::ovum::os::file::slash() == '\\') {
    ASSERT_EQ("c:/path/to/file", egg::ovum::os::file::normalizePath("C:\\Path\\to\\file", false));
    ASSERT_EQ("c:/path/to/file/", egg::ovum::os::file::normalizePath("C:\\Path\\to\\file\\", false));
    ASSERT_EQ("c:/path/to/file/", egg::ovum::os::file::normalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("c:/path/to/file/", egg::ovum::os::file::normalizePath("C:\\Path\\to\\file\\", true));
  }
}

TEST(TestOS_File, DenormalizePath) {
  ASSERT_EQ("C:\\Path\\to\\file", egg::ovum::os::file::denormalizePath("C:\\Path\\to\\file", false));
  ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::os::file::denormalizePath("C:\\Path\\to\\file\\", false));
  if (egg::ovum::os::file::slash() == '\\') {
    ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::os::file::denormalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("C:\\Path\\to\\file\\", egg::ovum::os::file::denormalizePath("C:\\Path\\to\\file\\", true));
    ASSERT_EQ("\\path\\to\\file", egg::ovum::os::file::denormalizePath("/path/to/file", false));
    ASSERT_EQ("\\path\\to\\file\\", egg::ovum::os::file::denormalizePath("/path/to/file/", false));
    ASSERT_EQ("\\path\\to\\file\\", egg::ovum::os::file::denormalizePath("/path/to/file", true));
    ASSERT_EQ("\\path\\to\\file\\", egg::ovum::os::file::denormalizePath("/path/to/file/", true));
  } else {
    ASSERT_EQ("C:\\Path\\to\\file/", egg::ovum::os::file::denormalizePath("C:\\Path\\to\\file", true));
    ASSERT_EQ("C:\\Path\\to\\file\\/", egg::ovum::os::file::denormalizePath("C:\\Path\\to\\file\\", true));
    ASSERT_EQ("/path/to/file", egg::ovum::os::file::denormalizePath("/path/to/file", false));
    ASSERT_EQ("/path/to/file/", egg::ovum::os::file::denormalizePath("/path/to/file/", false));
    ASSERT_EQ("/path/to/file/", egg::ovum::os::file::denormalizePath("/path/to/file", true));
    ASSERT_EQ("/path/to/file/", egg::ovum::os::file::denormalizePath("/path/to/file/", true));
  }
}

TEST(TestOS_File, GetCurrentDirectory) {
  auto cwd = egg::ovum::os::file::getCurrentDirectory();
  ASSERT_GT(cwd.size(), 0u);
  ASSERT_EQ('/', cwd.back());
}

TEST(TestOS_File, GetDevelopmentDirectory) {
  auto dev = egg::ovum::os::file::getDevelopmentDirectory();
  ASSERT_GT(dev.size(), 0u);
  ASSERT_EQ('/', dev.back());
}

TEST(TestOS_File, GetExecutablePath) {
  auto exe = egg::ovum::os::file::getExecutablePath();
  ASSERT_GT(exe.size(), 0u);
  ASSERT_NE(std::string::npos, exe.find('/'));
}

TEST(TestOS_File, GetExecutableDirectory) {
  auto dir = egg::ovum::os::file::getExecutableDirectory();
  ASSERT_GT(dir.size(), 0u);
  ASSERT_EQ('/', dir.back());
}

TEST(TestOS_File, CreateTemporaryDirectory) {
  auto dir = egg::ovum::os::file::createTemporaryDirectory("egg-test-file-", 100);
  ASSERT_GT(dir.size(), 0u);
  ASSERT_EQ('/', dir.back());
  ASSERT_EQ(egg::ovum::File::Kind::Directory, egg::ovum::File::getKind(dir));
}

TEST(TestOS_File, Slash) {
  auto slash = egg::ovum::os::file::slash();
  ASSERT_TRUE((slash == '/') || (slash == '\\'));
  auto dotslash = egg::ovum::os::file::denormalizePath(".", true);
  ASSERT_EQ(2u, dotslash.size());
  ASSERT_EQ(slash, dotslash[1]);
}
