#include "ovum/test.h"

namespace {
  using namespace egg::ovum;

  HardPtr<IVMProgram> createHelloWorldProgram(egg::test::VM& vm) {
    auto builder = vm->createProgramBuilder();
    // print("hello world);
    builder->addStatement(
      builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
      builder->exprLiteral(builder->createValue("hello world"))
    );
    return builder->build();
  }
  HardPtr<IVMProgramRunner> createRunnerWithPrint(egg::test::VM& vm, IVMProgram& program) {
    auto runner = program.createRunner();
    vm.addBuiltinPrint(*runner);
    return runner;
  }
  IVMProgramRunner::RunOutcome buildAndRun(egg::test::VM& vm, IVMProgramBuilder& builder, Value& retval, IVMProgramRunner::RunFlags flags = IVMProgramRunner::RunFlags::Default) {
    auto runner = builder.build()->createRunner();
    auto print = vm->createBuiltinPrint();
    vm.addBuiltins(*runner);
    auto outcome = runner->run(retval, flags);
    if (outcome == IVMProgramRunner::RunOutcome::Faulted) {
      vm.logger.log(ILogger::Source::User, ILogger::Severity::Error, vm.allocator.concat(retval));
    }
    return outcome;
  }
  void buildAndRunSuccess(egg::test::VM& vm, IVMProgramBuilder& builder, IVMProgramRunner::RunFlags flags = IVMProgramRunner::RunFlags::Default) {
    Value retval;
    auto outcome = buildAndRun(vm, builder, retval, flags);
    ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Completed, outcome);
    ASSERT_VALUE(egg::ovum::Value::Void, retval);
  }
  void buildAndRunFault(egg::test::VM& vm, IVMProgramBuilder& builder, IVMProgramRunner::RunFlags flags = IVMProgramRunner::RunFlags::Default) {
    Value retval;
    auto outcome = buildAndRun(vm, builder, retval, flags);
    ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Faulted, outcome);
    ASSERT_EQ(egg::ovum::ValueFlags::Throw|egg::ovum::ValueFlags::String, retval->getFlags());
  }
}

TEST(TestVM, CreateDefaultInstance) {
  egg::test::Allocator allocator;
  egg::test::Logger logger;
  auto vm = egg::ovum::VMFactory::createDefault(allocator, logger);
  ASSERT_NE(nullptr, vm);
}

TEST(TestVM, CreateTestInstance) {
  egg::test::Allocator allocator;
  egg::test::Logger logger;
  auto vm = egg::ovum::VMFactory::createTest(allocator, logger);
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
  auto program = createHelloWorldProgram(vm);
  auto runner = createRunnerWithPrint(vm, *program);
  egg::ovum::Value retval;
  auto outcome = runner->run(retval);
  ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Completed, outcome);
  ASSERT_VALUE(egg::ovum::Value::Void, retval);
  ASSERT_EQ("hello world\n", vm.logger.logged.str());
}

TEST(TestVM, StepProgram) {
  egg::test::VM vm;
  auto program = createHelloWorldProgram(vm);
  auto runner = createRunnerWithPrint(vm, *program);
  egg::ovum::Value retval;
  auto outcome = runner->run(retval, egg::ovum::IVMProgramRunner::RunFlags::Step);
  ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Stepped, outcome);
  ASSERT_VALUE(egg::ovum::Value::Void, retval);
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestVM, PrintPrint) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // print(print);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("print"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("[builtin print]\n", vm.logger.logged.str());
}

TEST(TestVM, PrintUnknown) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // print(unknown);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("unknown"))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Unknown variable symbol: 'unknown'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var v;
  builder->addStatement(
    builder->stmtVariableDeclare(builder->createString("v"))
  );
  // print(v);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("v"))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable uninitialized: 'v'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDeclareTwice) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var v;
  builder->addStatement(
    builder->stmtVariableDeclare(builder->createString("v"))
  );
  // var v;
  builder->addStatement(
    builder->stmtVariableDeclare(builder->createString("v"))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared: 'v'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDeclareAndSet) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var i;
  builder->addStatement(
    builder->stmtVariableDeclare(builder->createString("i"))
  );
  // i = 12345;
  builder->addStatement(
    builder->stmtVariableSet(builder->createString("i")),
    builder->exprLiteral(builder->createValueInt(12345))
  );
  // print(i);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("i"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("12345\n", vm.logger.logged.str());
}

TEST(TestVM, VariableUndeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var i;
  builder->addStatement(
    builder->stmtVariableDeclare(builder->createString("i"))
  );
  // ~i
  builder->addStatement(
    builder->stmtVariableUndeclare(builder->createString("i"))
  );
  // print(i);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("i"))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Unknown variable symbol: 'i'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineNull) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var n = null;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("n")),
    builder->exprLiteral(builder->createValueNull())
  );
  // print(n);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("n"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("null\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineBool) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var b = true;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("b")),
    builder->exprLiteral(builder->createValueBool(true))
  );
  // print(b);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("b"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("true\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineInt) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var i = 12345;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("i")),
    builder->exprLiteral(builder->createValueInt(12345))
  );
  // print(i);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("i"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("12345\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineFloat) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var f = 1234.5;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("f")),
    builder->exprLiteral(builder->createValueFloat(1234.5))
  );
  // print(f);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("f"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("1234.5\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineString) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var s = "hello world";
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("s")),
    builder->exprLiteral(builder->createValue("hello world"))
  );
  // print(s);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("s"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("hello world\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineObject) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var o = print;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("o")),
    builder->exprVariable(builder->createString("print"))
  );
  // print(o);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("o"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("[builtin print]\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineTwice) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var i = 12345;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("i")),
    builder->exprLiteral(builder->createValueInt(12345))
  );
  // var i = 54321;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("i")),
    builder->exprLiteral(builder->createValueInt(54321))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared: 'i'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineAndSet) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var i = 12345;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("i")),
    builder->exprLiteral(builder->createValueInt(12345))
  );
  // i = 54321;
  builder->addStatement(
    builder->stmtVariableSet(builder->createString("i")),
    builder->exprLiteral(builder->createValueInt(54321))
  );
  // print(i);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
    builder->exprVariable(builder->createString("i"))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("54321\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinDeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var print;
  builder->addStatement(
    builder->stmtVariableDeclare(builder->createString("print"))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared as a builtin: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinDefine) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var print = 12345;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("print")),
    builder->exprLiteral(builder->createValueInt(12345))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared as a builtin: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinSet) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // print = 12345;
  builder->addStatement(
    builder->stmtVariableSet(builder->createString("print")),
    builder->exprLiteral(builder->createValueInt(12345))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Cannot modify builtin symbol: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinUndeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // ~print
  builder->addStatement(
    builder->stmtVariableUndeclare(builder->createString("print"))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Cannot undeclare builtin symbol: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, AssertTrue) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // assert(true);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("assert"))),
    builder->exprLiteral(builder->createValueBool(true))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestVM, AssertFalse) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // assert(false);
  builder->addStatement(
    builder->stmtFunctionCall(builder->exprVariable(builder->createString("assert"))),
    builder->exprLiteral(builder->createValueBool(false))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Assertion failure\n", vm.logger.logged.str());
}
