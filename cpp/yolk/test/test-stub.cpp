#include "yolk/test.h"
#include "yolk/stub.h"
#include "ovum/version.h"

namespace {
  std::string toString(egg::yolk::IStub::ExitCode exitcode) {
    switch (exitcode) {
    case egg::yolk::IStub::ExitCode::OK:
      return "OK";
    case egg::yolk::IStub::ExitCode::Error:
      return "Error";
    case egg::yolk::IStub::ExitCode::Usage:
      return  "Usage";
    }
    return std::to_string(std::underlying_type_t<egg::yolk::IStub::ExitCode>(exitcode));
  }

  struct Stub {
    const std::string WELCOME =
      "<COMMAND><INFORMATION>Welcome to egg v" + egg::ovum::Version::semver() + "\n"
      "Try 'executable help' for more information\n";
    egg::test::Allocator allocator;
    egg::test::Logger logger;
    std::unique_ptr<egg::yolk::IStub> stub;
    explicit Stub(std::initializer_list<std::string> argv)
      : stub(egg::yolk::IStub::make()) {
      this->stub->withAllocator(this->allocator);
      this->stub->withLogger(this->logger);
      this->stub->withBuiltins();
      for (const auto& arg : argv) {
        this->stub->withArgument(arg);
      }
    }
    egg::yolk::IStub& operator*() {
      return *this->stub;
    }
    egg::yolk::IStub* operator->() {
      return this->stub.get();
    }
    std::string expect(egg::yolk::IStub::ExitCode expected) {
      auto actual = this->stub->main();
      if (actual != expected) {
        return "[exitcode actual=" + toString(actual) + ", expected=" + toString(expected) + "]";
      }
      return this->logger.logged.str();
    }
  };
}

TEST(TestStub, Main) {
  char arg0[] = "arg0";
  char* argv[] = { arg0, nullptr };
  char* envp[] = { nullptr };
  auto exitcode = egg::yolk::IStub::main(1, argv, envp);
  ASSERT_EQ(0, exitcode);
}

TEST(TestStub, Make) {
  auto stub = egg::yolk::IStub::make();
  auto exitcode = stub->main();
  ASSERT_EQ(egg::yolk::IStub::ExitCode::OK, exitcode);
}

TEST(TestStub, CommandMissing) {
  Stub stub{ "/path/to/executable.exe" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::OK);
  ASSERT_EQ(stub.WELCOME, logged);
  ASSERT_EQ(1u, stub.logger.counts.size());
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Information]);
}

TEST(TestStub, CommandUnknown) {
  Stub stub{ "/path/to/executable.exe", "unknown" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::Usage);
  ASSERT_STARTSWITH(logged, "<COMMAND><ERROR>executable: Unknown command: 'unknown'\n"
                            "<COMMAND><INFORMATION>Usage: executable ");
  ASSERT_EQ(2u, stub.logger.counts.size());
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Error]);
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Information]);
}

TEST(TestStub, SubcommandMissing) {
  Stub stub{ "/path/to/executable.exe", "zip" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::Usage);
  ASSERT_STARTSWITH(logged, "<COMMAND><ERROR>executable zip: Missing subcommand\n"
                            "<COMMAND><INFORMATION>Usage: executable zip <subcommand>\n"
                            " <subcommand> is one of:\n  ");
  ASSERT_EQ(2u, stub.logger.counts.size());
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Error]);
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Information]);
}

TEST(TestStub, SubcommandUnknown) {
  Stub stub{ "/path/to/executable.exe", "zip", "unknown" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::Usage);
  ASSERT_STARTSWITH(logged, "<COMMAND><ERROR>executable zip: Unknown subcommand: 'unknown'\n"
                            "<COMMAND><INFORMATION>Usage: executable zip <subcommand>\n"
                            " <subcommand> is one of:\n  ");
  ASSERT_EQ(2u, stub.logger.counts.size());
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Error]);
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Information]);
}

TEST(TestStub, GeneralOptionUnknown) {
  Stub stub{ "/path/to/executable.exe", "--unknown" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::Usage);
  ASSERT_STARTSWITH(logged, "<COMMAND><ERROR>executable: Unknown general option: '--unknown'\n<COMMAND><INFORMATION>Usage: executable ");
  ASSERT_EQ(2u, stub.logger.counts.size());
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Error]);
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Information]);
}

TEST(TestStub, LogLevelUnknown) {
  Stub stub{ "/path/to/executable.exe", "--log-level=unknown" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::Usage);
  auto expected = "<COMMAND><ERROR>executable: Invalid general option: '--log-level=unknown'\n"
                  "<COMMAND><INFORMATION>Option usage: '--log-level=debug|verbose|information|warning|error|none'\n";
  ASSERT_EQ(expected, logged);
  ASSERT_EQ(2u, stub.logger.counts.size());
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Error]);
  ASSERT_EQ(1u, stub.logger.counts[egg::ovum::ILogger::Severity::Information]);
}

TEST(TestStub, LogLevelDebug) {
  Stub stub{ "/path/to/executable.exe", "--log-level=debug" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::OK);
  ASSERT_EQ("<COMMAND><DEBUG>No command supplied\n" + stub.WELCOME, logged);
}

TEST(TestStub, LogLevelVerbose) {
  Stub stub{ "/path/to/executable.exe", "--log-level=verbose" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::OK);
  ASSERT_EQ(stub.WELCOME, logged);
}

TEST(TestStub, LogLevelInformation) {
  Stub stub{ "/path/to/executable.exe", "--log-level=information" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::OK);
  ASSERT_EQ(stub.WELCOME, logged);
}

TEST(TestStub, LogLevelWarning) {
  Stub stub{ "/path/to/executable.exe", "--log-level=warning" };
  stub.allocator.expectation = egg::test::Allocator::Expectation::NoAllocations;
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::OK);
  ASSERT_EQ("", logged);
}

TEST(TestStub, LogLevelError) {
  Stub stub{ "/path/to/executable.exe", "--log-level=error" };
  stub.allocator.expectation = egg::test::Allocator::Expectation::NoAllocations;
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::OK);
  ASSERT_EQ("", logged);
}

TEST(TestStub, LogLevelNone) {
  Stub stub{ "/path/to/executable.exe", "--log-level=none" };
  stub.allocator.expectation = egg::test::Allocator::Expectation::NoAllocations;
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::OK);
  ASSERT_EQ("", logged);
}

TEST(TestStub, ProfileAll) {
  Stub stub{ "/path/to/executable.exe", "--profile", "smoke-test" };
  auto logged = stub.expect(egg::yolk::IStub::ExitCode::OK);
  ASSERT_STARTSWITH(logged, "Hello, world!\n");
  ASSERT_CONTAINS(logged, "\n<COMMAND><INFORMATION>profile: time: ");
  ASSERT_CONTAINS(logged, "\n<COMMAND><INFORMATION>profile: memory: ");
  ASSERT_CONTAINS(logged, "\n<COMMAND><INFORMATION>profile: allocator: ");
}
