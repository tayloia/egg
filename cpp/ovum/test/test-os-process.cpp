#include "ovum/test.h"
#include "ovum/os-process.h"

TEST(TestOS_Process, PopenEcho) {
  std::stringstream ss;
  auto* fp = egg::ovum::os::process::popen("echo hello");
  ASSERT_NE(nullptr, fp);
  for (auto ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp)) {
    ss.put(char(ch));
  }
  egg::ovum::os::process::pclose(fp);
  ASSERT_EQ("hello\n", ss.str());
}
