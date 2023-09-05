#include "ovum/test.h"

namespace {
  egg::ovum::HardPtr<egg::ovum::IVMProgram> createHelloWorldProgram(egg::test::VM& vm) {
    auto builder = vm.vm->createProgramBuilder();
    builder->addStatement(
      builder->stmtFunctionCall(builder->exprVariable(vm->createString("print"))),
      builder->exprLiteral(vm->createValue("hello world"))
    );
    return builder->build();
  }
  egg::ovum::HardPtr<egg::ovum::IVMProgramRunner> createHelloWorldRunnerWithPrint(egg::test::VM& vm) {
    auto program = createHelloWorldProgram(vm);
    auto runner = program->createRunner();
    runner->builtin(program->createString("print"), vm.vm->createValue("[Object PRINT]"));
    return runner;
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

TEST(TestVM, CreateString) {
  egg::test::VM vm;
  auto s = vm->createString("ASCII");
  ASSERT_STRING("ASCII", s);
  s = vm->createString(u8"UTF8");
  ASSERT_STRING("UTF8", s);
  s = vm->createString(U"UTF32");
  ASSERT_STRING("UTF32", s);
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
  ASSERT_TRUE(value.isVoid());
}

TEST(TestVM, CreateValueNull) {
  egg::test::VM vm;
  auto value = vm->createValueNull();
  ASSERT_TRUE(value->getNull());
  ASSERT_TRUE(value.isNull());
}

TEST(TestVM, CreateValueBool) {
  egg::test::VM vm;
  egg::ovum::Bool actual = true;
  auto value = vm->createValueBool(false);
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_FALSE(actual);
  ASSERT_TRUE(value.isBool());
  ASSERT_TRUE(value.isBool(false));
  ASSERT_FALSE(value.isBool(true));
  value = vm->createValueBool(true);
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_TRUE(actual);
  ASSERT_TRUE(value.isBool());
  ASSERT_FALSE(value.isBool(false));
  ASSERT_TRUE(value.isBool(true));
}

TEST(TestVM, CreateValueInt) {
  egg::test::VM vm;
  egg::ovum::Int actual = 0;
  auto value = vm->createValueInt(12345);
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(12345, actual);
  ASSERT_TRUE(value.isInt());
  ASSERT_TRUE(value.isInt(12345));
  ASSERT_FALSE(value.isInt(0));
  value = vm->createValueInt(-12345);
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(-12345, actual);
  ASSERT_TRUE(value.isInt());
  ASSERT_TRUE(value.isInt(-12345));
  ASSERT_FALSE(value.isInt(0));
}

TEST(TestVM, CreateValueFloat) {
  egg::test::VM vm;
  egg::ovum::Float actual = 0;
  auto value = vm->createValueFloat(1234.5);
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(1234.5, actual);
  ASSERT_TRUE(value.isFloat());
  ASSERT_TRUE(value.isFloat(1234.5));
  ASSERT_FALSE(value.isFloat(0));
  value = vm->createValueFloat(-1234.5);
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(-1234.5, actual);
  ASSERT_TRUE(value.isFloat());
  ASSERT_TRUE(value.isFloat(-1234.5));
  ASSERT_FALSE(value.isFloat(0));
}

TEST(TestVM, CreateValueString) {
  egg::test::VM vm;
  egg::ovum::String actual;
  auto expected = vm->createString("hello");
  auto value = vm->createValueString(expected);
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("hello", actual);
  ASSERT_TRUE(value.isString());
  ASSERT_TRUE(value.isString("hello"));
  ASSERT_FALSE(value.isString("goodbye"));
  value = vm->createValueString(vm->createString(u8"egg \U0001F95A"));
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING(u8"egg \U0001F95A", actual);
  value = vm->createValueString(vm->createString(U"goodbye"));
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("goodbye", actual);
}

TEST(TestVM, CreateValue) {
  egg::test::VM vm;
  egg::ovum::String actual;
  auto value = vm->createValue("hello");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("hello", actual);
  ASSERT_TRUE(value.isString());
  ASSERT_TRUE(value.isString("hello"));
  ASSERT_FALSE(value.isString("goodbye"));
  value = vm->createValue(u8"egg \U0001F95A");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING(u8"egg \U0001F95A", actual);
  value = vm->createValue(U"goodbye");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("goodbye", actual);
}

TEST(TestVM, CreateProgram) {
  egg::test::VM vm;
  auto program = createHelloWorldProgram(vm);
  ASSERT_NE(nullptr, program);
}

TEST(TestVM, RunProgram) {
  egg::test::VM vm;
  auto runner = createHelloWorldRunnerWithPrint(vm);
  egg::ovum::Value retval;
  auto outcome = runner->run(retval);
  ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Completed, outcome);
  ASSERT_VALUE(egg::ovum::Value::Void, retval);
}

TEST(TestVM, StepProgram) {
  egg::test::VM vm;
  auto runner = createHelloWorldRunnerWithPrint(vm);
  egg::ovum::Value retval;
  auto outcome = runner->run(retval, egg::ovum::IVMProgramRunner::RunFlags::Step);
  ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Stepped, outcome);
  ASSERT_VALUE(egg::ovum::Value::Void, retval);
}
