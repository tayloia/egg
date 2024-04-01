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

TEST(TestOS_Process, PlinesEcho) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::plines("echo hello", [&](const std::string& line) {
    ss << '[' << line << ']';
  });
  ASSERT_EQ(0, exitcode);
  ASSERT_EQ("[hello]", ss.str());
}

TEST(TestOS_Process, PlinesExit) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::plines("exit 123", [&](const std::string& line) {
    ss << '[' << line << ']';
  });
  ASSERT_EQ(123, exitcode);
  ASSERT_EQ("", ss.str());
}

TEST(TestOS_Process, PlinesFail) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::plines("does-not-exist", [&](const std::string& line) {
    ss << '[' << line << ']';
  });
  ASSERT_LT(0, exitcode);
  ASSERT_NE("", ss.str());
}

TEST(TestOS_Process, Snapshot) {
  auto snapshot = egg::ovum::os::process::snapshot();
  ASSERT_GE(snapshot.microsecondsUser, 0u);
  ASSERT_GE(snapshot.microsecondsSystem, 0u);
  ASSERT_GT(snapshot.microsecondsElapsed, 0u);
}
