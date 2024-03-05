#include "yolk/test.h"
#include "ovum/file.h"

#include <cstdio>
#include <sstream>

namespace {
  void replaceFirst(std::string& haystack, const std::string& needle, const std::string& replacement) {
    auto pos = haystack.find(needle);
    ASSERT_NE(pos, std::string::npos);
    haystack.replace(pos, needle.size(), replacement);
  }
  std::string executable() {
    auto exe = egg::ovum::File::getExecutablePath();
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
    // e.g. "c:/project/egg/bin/msvc/yolk-test.debug.x64/yolk-test.exe"
    std::string needle{ "/yolk-test." };
    replaceFirst(exe, needle, "/cli.");
    replaceFirst(exe, needle, "/egg.");
#else
    // e.g. "/mnt/c/Project/egg/bin/wsl/gcc/release/egg-testsuite.exe"
    std::string needle{ "/egg-testsuite" };
    replaceFirst(exe, needle, "/egg");
#endif
    return exe;
  }
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  // Microsoft aliases
  auto popen = _popen;
  auto pclose = _pclose;
#endif
  std::string spawn(const std::string& command) {
    std::stringstream ss;
    auto* fp = popen(command.c_str(), "r");
    if (fp == nullptr) {
      return "Failed to run: " + command;
    }
    for (auto ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp)) {
      ss.put(char(ch));
    }
    pclose(fp);
    return ss.str();
  }
}

TEST(TestCLI, Spawn) {
  std::string arg0 = executable();
  std::string arg1 = "--WIBBLE";
  auto output = spawn(arg0 + " " + arg1);
  ASSERT_EQ(arg0 + "\n" + arg1 + "\n", output);
}
