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
      replaceFirst(exe, needle, "/egg-stub.");
      replaceFirst(exe, needle, "/egg-stub.");
    } else {
      // e.g. "/mnt/c/Project/egg/bin/wsl/gcc/release/egg-test.exe"
      std::string needle{ "/egg-test" };
      replaceFirst(exe, needle, "/egg-stub");
    }
    return egg::ovum::os::file::denormalizePath(exe, false);
  }
  std::string spawn(const std::string& arguments) {
    std::stringstream ss;
    auto exitcode = egg::ovum::os::process::pexec(ss, executable() + " " + arguments);
    if (exitcode != 0) {
      ss << "exitcode=" << exitcode;
    }
    return ss.str();
  }
}

TEST(TestCLI, Version) {
  std::stringstream ss;
  ss << egg::ovum::Version() << '\n';
  auto expected = ss.str();
  auto actual = spawn("version");
  ASSERT_EQ(expected, actual);
}
