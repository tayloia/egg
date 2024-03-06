#include "yolk/test.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"
#include "ovum/version.h"

#include <cstdio>
#include <sstream>

namespace {
  void replaceFirst(std::string& haystack, const std::string& needle, const std::string& replacement) {
    auto pos = haystack.find(needle);
    ASSERT_NE(pos, std::string::npos);
    haystack.replace(pos, needle.size(), replacement);
  }
  std::string executable() {
    auto exe = egg::ovum::os::file::getExecutablePath();
    if (egg::ovum::os::file::slash() == '\\') {
      // e.g. "c:/project/egg/bin/msvc/yolk-test.debug.x64/yolk-test.exe"
      std::string needle{ "/yolk-test." };
      replaceFirst(exe, needle, "/cli.");
      replaceFirst(exe, needle, "/egg.");
    } else {
      // e.g. "/mnt/c/Project/egg/bin/wsl/gcc/release/egg-testsuite.exe"
      std::string needle{ "/egg-testsuite" };
      replaceFirst(exe, needle, "/egg");
    }
    return exe;
  }
  std::string spawn(const std::string& command) {
    std::stringstream ss;
    auto* fp = egg::ovum::os::process::popen(command);
    if (fp == nullptr) {
      return "Failed to run: " + command;
    }
    for (auto ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp)) {
      ss.put(char(ch));
    }
    egg::ovum::os::process::pclose(fp);
    return ss.str();
  }
}

TEST(TestCLI, Version) {
  std::stringstream ss;
  ss << egg::ovum::Version() << '\n';
  auto expected = ss.str();
  auto actual = spawn(executable() + " version");
  ASSERT_EQ(expected, actual);
}
