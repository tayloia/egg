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
      std::string needle{ "/egg-test." };
      replaceFirst(exe, needle, "/egg-stub.");
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

TEST(TestCLI, UnknownCommand) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::pexec(ss, executable() + " unknown");
  ASSERT_EQ(2, exitcode);
  auto actual = ss.str();
  auto expected = "egg-stub: Unknown command: 'unknown'\n"
                  "Usage: egg-stub [<general-option>]... <command> [<command-option>|<command-argument>]...\n";
  ASSERT_STARTSWITH(actual, expected);
}

TEST(TestCLI, MissingSubcommand) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::pexec(ss, executable() + " sandwich");
  ASSERT_EQ(2, exitcode);
  auto actual = ss.str();
  auto expected = "egg-stub sandwich: Missing subcommand\n"
                  "Usage: egg-stub sandwich <subcommand>\n"
                  " <subcommand> is one of:\n"
                  "  make --target=<exe-file> --zip=<zip-file>\n";
  ASSERT_EQ(actual, expected);
}

TEST(TestCLI, UnknownSubcommand) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::pexec(ss, executable() + " sandwich unknown");
  ASSERT_EQ(2, exitcode);
  auto actual = ss.str();
  auto expected = "egg-stub sandwich: Unknown subcommand: 'unknown'\n"
                  "Usage: egg-stub sandwich <subcommand>\n"
                  " <subcommand> is one of:\n"
                  "  make --target=<exe-file> --zip=<zip-file>\n";
  ASSERT_EQ(actual, expected);
}

TEST(TestCLI, UnknownOption) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::pexec(ss, executable() + " --unknown");
  ASSERT_EQ(2, exitcode);
  auto actual = ss.str();
  auto expected = "egg-stub: Unknown general option: '--unknown'\n"
                  "Usage: egg-stub [<general-option>]... <command> [<command-option>|<command-argument>]...\n";
  ASSERT_STARTSWITH(actual, expected);
}

TEST(TestCLI, DuplicatedOption) {
  std::stringstream ss;
  auto exitcode = egg::ovum::os::process::pexec(ss, executable() + " --log-level=debug --log-level=none");
  ASSERT_EQ(2, exitcode);
  auto actual = ss.str();
  auto expected = "egg-stub: Duplicated general option: '--log-level'\n"
                  "Usage: egg-stub [<general-option>]... <command> [<command-option>|<command-argument>]...\n";
  ASSERT_STARTSWITH(actual, expected);
}

TEST(TestCLI, Empty) {
  auto actual = spawn("");
  auto expected = "Welcome to egg v" + egg::ovum::Version::semver() + "\n"
                  "Try 'egg-stub help' for more information\n";
  ASSERT_EQ(expected, actual);
}

TEST(TestCLI, Help) {
  auto actual = spawn("help");
  ASSERT_STARTSWITH(actual, "Usage: egg-stub [<general-option>]... <command> [<command-option>|<command-argument>]...\n");
  ASSERT_CONTAINS(actual, "\n  <general-option> is any of:\n");
  ASSERT_CONTAINS(actual, "\n    --log-level=debug|verbose|information|warning|error|none\n");
  ASSERT_CONTAINS(actual, "\n  <command> is one of:\n");
  ASSERT_CONTAINS(actual, "\n    help\n");
}

TEST(TestCLI, Version) {
  std::stringstream ss;
  ss << "egg v" << egg::ovum::Version() << '\n';
  auto expected = ss.str();
  auto actual = spawn("version");
  ASSERT_EQ(expected, actual);
}

TEST(TestCLI, SmokeTest) {
  auto expected = "Hello, world!\n";
  auto actual = spawn("smoke-test");
  ASSERT_EQ(expected, actual);
}
