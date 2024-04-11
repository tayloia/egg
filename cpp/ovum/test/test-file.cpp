#include "ovum/test.h"
#include "ovum/file.h"

TEST(TestFile, ReadDirectory) {
  auto filenames = egg::ovum::File::readDirectory(egg::test::resolvePath("data"));
  ASSERT_FALSE(filenames.empty());
  filenames = egg::ovum::File::readDirectory(egg::test::resolvePath("missing-in-action"));
  ASSERT_TRUE(filenames.empty());
}

TEST(TestFile, KindUnknown) {
  ASSERT_EQ(egg::ovum::File::Kind::Unknown, egg::ovum::File::getKind(egg::test::resolvePath("missing-in-action")));
}

TEST(TestFile, KindDirectory) {
  ASSERT_EQ(egg::ovum::File::Kind::Directory, egg::ovum::File::getKind(egg::test::resolvePath("data")));
}

TEST(TestFile, KindFile) {
  ASSERT_EQ(egg::ovum::File::Kind::File, egg::ovum::File::getKind(egg::test::resolvePath("data/egg.png")));
}

TEST(TestFile, Slurp) {
  auto jabberwocky = egg::ovum::File::slurp(egg::test::resolvePath("cpp/data/jabberwocky.txt"));
  ASSERT_EQ(1008u, jabberwocky.size());
  ASSERT_CONTAINS(jabberwocky, "Twas brillig, and the slithy toves");
}
