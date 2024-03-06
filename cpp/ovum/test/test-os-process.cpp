#include "ovum/test.h"
#include "ovum/os-process.h"

TEST(TestOS_Process, PopenEcho) {
  std::string s;
  auto* fp = egg::ovum::os::process::popen("echo hello", "r");
  ASSERT_NE(nullptr, fp);
  for (auto ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp)) {
    s.push_back(char(ch));
  }
  auto exitcode = egg::ovum::os::process::pclose(fp);
  ASSERT_EQ(0, exitcode);
  ASSERT_EQ("hello\n", s);
}

TEST(TestOS_Process, PopenExit) {
  auto* fp = egg::ovum::os::process::popen("exit 123", "r");
  ASSERT_NE(nullptr, fp);
  auto exitcode = egg::ovum::os::process::pclose(fp);
  ASSERT_EQ(123, exitcode);
}

TEST(TestOS_Process, PopenFail) {
  std::string s;
  auto* fp = egg::ovum::os::process::popen("does-not-exist", "r");
  ASSERT_NE(nullptr, fp);
  for (auto ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp)) {
    s.push_back(char(ch));
  }
  auto exitcode = egg::ovum::os::process::pclose(fp);
  ASSERT_LT(0, exitcode);
  ASSERT_EQ("", s);
}

TEST(TestOS_Process, PexecEcho) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::pexec(ss, "echo hello");
  ASSERT_EQ(0, exitcode);
  ASSERT_EQ("hello\n", ss.str());
}

TEST(TestOS_Process, PexecExit) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::pexec(ss, "exit 123");
  ASSERT_EQ(123, exitcode);
  ASSERT_EQ("", ss.str());
}

TEST(TestOS_Process, PexecFail) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::pexec(ss, "does-not-exist");
  ASSERT_LT(0, exitcode);
  ASSERT_NE("", ss.str());
}
