#include "ovum/test.h"
#include "ovum/version.h"

TEST(TestVersion, Version) {
  egg::ovum::Version version;
  ASSERT_GE(version.major, 0u);
  ASSERT_GE(version.minor, 0u);
  ASSERT_GE(version.patch, 0u);
}

TEST(TestVersion, Commit) {
  egg::ovum::Version version;
  ASSERT_REGEX("^[0-9a-f]{40}$", version.commit);
  ASSERT_STRNE("0000000000000000000000000000000000000000", version.commit);
}

TEST(TestVersion, Timestamp) {
  egg::ovum::Version version;
  ASSERT_REGEX("^[0-9]{4}-[01][0-9]-[0-3][0-9] [0-2][0-9]:[0-5][0-9]:[0-6][0-9]Z$", version.timestamp);
  ASSERT_STRNE("0000-00-00 00:00:00Z", version.timestamp);
}
