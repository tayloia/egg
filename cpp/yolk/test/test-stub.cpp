#include "yolk/test.h"
#include "yolk/stub.h"

TEST(TestStub, Main) {
  char arg0[] = "arg0";
  char* argv[] = { arg0, nullptr };
  char* envp[] = { nullptr };
  auto exitcode = egg::yolk::IStub::main(1, argv, envp);
  ASSERT_EQ(0, exitcode);
}

TEST(TestStub, Empty) {
  auto stub = egg::yolk::IStub::make();
  auto exitcode = stub->main();
  ASSERT_EQ(egg::yolk::IStub::ExitCode::OK, exitcode);
}

TEST(TestStub, CommandMissing) {
  auto stub = egg::yolk::IStub::make();
  stub->withArgument("/path/to/executable.exe");
  auto exitcode = stub->main();
  ASSERT_EQ(egg::yolk::IStub::ExitCode::OK, exitcode);
}

TEST(TestStub, CommandUnknown) {
  auto stub = egg::yolk::IStub::make();
  stub->withArgument("/path/to/executable.exe");
  stub->withArgument("unknown");
  auto exitcode = stub->main();
  ASSERT_EQ(egg::yolk::IStub::ExitCode::Usage, exitcode);
}
