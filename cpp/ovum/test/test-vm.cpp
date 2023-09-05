#include "ovum/test.h"

namespace {
  egg::ovum::HardPtr<egg::ovum::IVMProgram> createHelloWorld(egg::test::VM& vm) {
    auto pb = vm.vm->createProgramBuilder();
    pb->addStatement(
      pb->stmtFunctionCall(pb->exprVariable(pb->createStringUTF8("print"))),
      pb->exprLiteralString(pb->createStringUTF8("hello world"))
    );
    return pb->build();
  }
}

TEST(TestVM, CreateDefaultInstance) {
  egg::test::Allocator allocator;
  auto vm = egg::ovum::VMFactory::createDefault(allocator);
  ASSERT_NE(nullptr, vm);
}

TEST(TestVM, CreateTestInstance) {
  egg::test::Allocator allocator;
  auto vm = egg::ovum::VMFactory::createTest(allocator);
  ASSERT_NE(nullptr, vm);
}

TEST(TestVM, CreateStringUTF8) {
  egg::test::VM vm;
  auto s = vm->createStringUTF8(u8"hello");
  ASSERT_STRING("hello", s);
}

TEST(TestVM, CreateStringUTF32) {
  egg::test::VM vm;
  auto s = vm->createStringUTF32(U"hello");
  ASSERT_STRING("hello", s);
}

TEST(TestVM, CreateValueVoid) {
  egg::test::VM vm;
  auto value = vm->createValueVoid();
  ASSERT_TRUE(value->getVoid());
}

TEST(TestVM, CreateValueNull) {
  egg::test::VM vm;
  auto value = vm->createValueNull();
  ASSERT_TRUE(value->getNull());
}

TEST(TestVM, CreateValueBool) {
  egg::test::VM vm;
  egg::ovum::Bool actual = true;
  auto value = vm->createValueBool(false);
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_FALSE(actual);
  value = vm->createValueBool(true);
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_TRUE(actual);
}

TEST(TestVM, CreateValueInt) {
  egg::test::VM vm;
  egg::ovum::Int actual = 0;
  auto value = vm->createValueInt(12345);
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(12345, actual);
  value = vm->createValueInt(-12345);
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(-12345, actual);
}

TEST(TestVM, CreateValueFloat) {
  egg::test::VM vm;
  egg::ovum::Float actual = 0;
  auto value = vm->createValueFloat(1234.5);
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(1234.5, actual);
  value = vm->createValueFloat(-1234.5);
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(-1234.5, actual);
}

TEST(TestVM, CreateValueString) {
  egg::test::VM vm;
  egg::ovum::String actual;
  auto value = vm->createValueString("hello");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("hello", actual);
  value = vm->createValueString(u8"egg \U0001F95A");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING(u8"egg \U0001F95A", actual);
  value = vm->createValueString(U"goodbye");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("goodbye", actual);
}

TEST(TestVM, CreateProgram) {
  egg::test::VM vm;
  auto program = createHelloWorld(vm);
  ASSERT_NE(nullptr, program);
}

TEST(TestVM, RunProgram) {
  egg::test::VM vm;
  auto program = createHelloWorld(vm);
  auto runner = program->createRunner();
  egg::ovum::Value retval;
  auto outcome = runner->run(retval);
  ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Completed, outcome);
  ASSERT_VALUE(egg::ovum::Value::Void, retval);
}
