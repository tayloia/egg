#include "ovum/test.h"

namespace {
  using namespace egg::ovum;

  HardPtr<IVMProgram> createHelloWorldProgram(egg::test::VM& vm) {
    auto builder = vm->createProgramBuilder();
    // print("hello world);
    builder->addStatement(
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprLiteral(builder->createHardValue("hello world"))
      )
    );
    return builder->build();
  }
  HardPtr<IVMProgramRunner> createRunnerWithPrint(egg::test::VM& vm, IVMProgram& program) {
    auto runner = program.createRunner();
    vm.addBuiltinPrint(*runner);
    return runner;
  }
  IVMProgramRunner::RunOutcome buildAndRun(egg::test::VM& vm, IVMProgramBuilder& builder, HardValue& retval, IVMProgramRunner::RunFlags flags = IVMProgramRunner::RunFlags::Default) {
    auto runner = builder.build()->createRunner();
    vm.addBuiltins(*runner);
    auto outcome = runner->run(retval, flags);
    if (outcome == IVMProgramRunner::RunOutcome::Faulted) {
      vm.logger.log(ILogger::Source::User, ILogger::Severity::Error, vm.allocator.concat(retval));
    }
    return outcome;
  }
  void buildAndRunSuccess(egg::test::VM& vm, IVMProgramBuilder& builder, IVMProgramRunner::RunFlags flags = IVMProgramRunner::RunFlags::Default) {
    HardValue retval;
    auto outcome = buildAndRun(vm, builder, retval, flags);
    ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Completed, outcome);
    ASSERT_VALUE(egg::ovum::HardValue::Void, retval);
  }
  void buildAndRunFault(egg::test::VM& vm, IVMProgramBuilder& builder, IVMProgramRunner::RunFlags flags = IVMProgramRunner::RunFlags::Default) {
    HardValue retval;
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

TEST(TestVM, CreateHardValueVoid) {
  egg::test::VM vm;
  auto value = vm->createHardValueVoid();
  ASSERT_TRUE(value->getVoid());
}

TEST(TestVM, CreateHardValueNull) {
  egg::test::VM vm;
  auto value = vm->createHardValueNull();
  ASSERT_TRUE(value->getNull());
}

TEST(TestVM, CreateHardValueBool) {
  egg::test::VM vm;
  egg::ovum::Bool actual = true;
  auto value = vm->createHardValueBool(false);
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_FALSE(actual);
  value = vm->createHardValueBool(true);
  ASSERT_TRUE(value->getBool(actual));
  ASSERT_TRUE(actual);
}

TEST(TestVM, CreateHardValueInt) {
  egg::test::VM vm;
  egg::ovum::Int actual = 0;
  auto value = vm->createHardValueInt(12345);
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(12345, actual);
  value = vm->createHardValueInt(-12345);
  ASSERT_TRUE(value->getInt(actual));
  ASSERT_EQ(-12345, actual);
}

TEST(TestVM, CreateHardValueFloat) {
  egg::test::VM vm;
  egg::ovum::Float actual = 0;
  auto value = vm->createHardValueFloat(1234.5);
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(1234.5, actual);
  value = vm->createHardValueFloat(-1234.5);
  ASSERT_TRUE(value->getFloat(actual));
  ASSERT_EQ(-1234.5, actual);
}

TEST(TestVM, CreateHardValueString) {
  egg::test::VM vm;
  egg::ovum::String actual;
  auto expected = vm->createString("hello");
  auto value = vm->createHardValueString(expected);
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("hello", actual);
  value = vm->createHardValueString(vm->createString(u8"egg \U0001F95A"));
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING(u8"egg \U0001F95A", actual);
  value = vm->createHardValueString(vm->createString(U"goodbye"));
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("goodbye", actual);
}

TEST(TestVM, CreateHardValue) {
  egg::test::VM vm;
  egg::ovum::String actual;
  auto value = vm->createHardValue("hello");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("hello", actual);
  value = vm->createHardValue(u8"egg \U0001F95A");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING(u8"egg \U0001F95A", actual);
  value = vm->createHardValue(U"goodbye");
  ASSERT_TRUE(value->getString(actual));
  ASSERT_STRING("goodbye", actual);
}

TEST(TestVM, CreateProgram) {
  egg::test::VM vm;
  auto program = createHelloWorldProgram(vm);
  ASSERT_STRING("[VMProgram]", vm.allocator.concat(program));
}

TEST(TestVM, RunProgram) {
  egg::test::VM vm;
  auto program = createHelloWorldProgram(vm);
  auto runner = createRunnerWithPrint(vm, *program);
  egg::ovum::HardValue retval;
  auto outcome = runner->run(retval);
  ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Completed, outcome);
  ASSERT_VALUE(egg::ovum::HardValue::Void, retval);
  ASSERT_EQ("hello world\n", vm.logger.logged.str());
}

TEST(TestVM, StepProgram) {
  egg::test::VM vm;
  auto program = createHelloWorldProgram(vm);
  auto runner = createRunnerWithPrint(vm, *program);
  egg::ovum::HardValue retval;
  auto outcome = runner->run(retval, egg::ovum::IVMProgramRunner::RunFlags::Step);
  ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Stepped, outcome);
  ASSERT_VALUE(egg::ovum::HardValue::Void, retval);
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestVM, PrintPrint) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // print(print);
      builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
      builder->exprVariable(builder->createString("print"))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("[builtin print]\n", vm.logger.logged.str());
}

TEST(TestVM, PrintUnknown) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // print(unknown);
      builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
      builder->exprVariable(builder->createString("unknown"))
    )
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Unknown variable symbol: 'unknown'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var v;
      builder->stmtVariableDeclare(builder->createString("v")),
      // print(v);
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprVariable(builder->createString("v"))
      )
    )
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable uninitialized: 'v'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDeclareTwice) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var v;
      builder->stmtVariableDeclare(builder->createString("v")),
      // var v;
      builder->stmtVariableDeclare(builder->createString("v"))
    )
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared: 'v'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefine) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var i;
      builder->stmtVariableDefine(builder->createString("i"), builder->exprLiteral(builder->createHardValueInt(12345))),
      // print(i);
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprVariable(builder->createString("i"))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("12345\n", vm.logger.logged.str());
}

TEST(TestVM, VariableUndeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i;
    builder->stmtVariableDeclare(builder->createString("i"))
  );
  builder->addStatement(
    // print(i);
    builder->glue(
      builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
      builder->exprVariable(builder->createString("i"))
    )
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Unknown variable symbol: 'i'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineNull) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var n = null;
      builder->stmtVariableDefine(builder->createString("n"), builder->exprLiteral(builder->createHardValueNull())),
      // print(n);
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprVariable(builder->createString("n"))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("null\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineBool) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var b = true;
      builder->stmtVariableDefine(builder->createString("b"), builder->exprLiteral(builder->createHardValueBool(true))),
      // print(b);
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprVariable(builder->createString("b"))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("true\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineInt) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var i = 12345;
      builder->stmtVariableDefine(builder->createString("i"), builder->exprLiteral(builder->createHardValueInt(12345))),
      // print(i);
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprVariable(builder->createString("i"))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("12345\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineFloat) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var f = 1234.5;
      builder->stmtVariableDefine(builder->createString("f"), builder->exprLiteral(builder->createHardValueFloat(1234.5))),
      // print(f);
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprVariable(builder->createString("f"))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("1234.5\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineString) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var s = "hello world";
      builder->stmtVariableDefine(builder->createString("s"), builder->exprLiteral(builder->createHardValue("hello world"))),
      // print(s);
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprVariable(builder->createString("s"))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("hello world\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineObject) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // var o = print;
  builder->addStatement(
    builder->glue(
      // var o = print;
      builder->stmtVariableDefine(builder->createString("o"), builder->exprVariable(builder->createString("print"))),
      // print(o);
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprVariable(builder->createString("o"))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("[builtin print]\n", vm.logger.logged.str());
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
  // var print;
  builder->addStatement(
    builder->stmtVariableDefine(builder->createString("print"), builder->exprLiteral(builder->createHardValueNull()))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared as a builtin: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinSet) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // print = 12345;
  builder->addStatement(
    builder->stmtVariableSet(builder->createString("print"), builder->exprLiteral(builder->createHardValueInt(12345)))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Cannot modify builtin symbol: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, AssertTrue) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // assert(true);
  builder->addStatement(
    builder->glue(
      builder->stmtFunctionCall(builder->exprVariable(builder->createString("assert"))),
      builder->exprLiteral(builder->createHardValueBool(true))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestVM, AssertFalse) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // assert(false);
  builder->addStatement(
    builder->glue(
      builder->stmtFunctionCall(builder->exprVariable(builder->createString("assert"))),
      builder->exprLiteral(builder->createHardValueBool(false))
    )
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Assertion failure\n", vm.logger.logged.str());
}

TEST(TestVM, ExpandoPair) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var a = expando();
      builder->stmtVariableDefine(builder->createString("a"), builder->exprFunctionCall(builder->exprVariable(builder->createString("expando")))),
      builder->glue(
        // var b = expando();
        builder->stmtVariableDefine(builder->createString("b"), builder->exprFunctionCall(builder->exprVariable(builder->createString("expando")))),
        // a.x = b;
        builder->stmtPropertySet(builder->exprVariable(builder->createString("a")),
                                 builder->exprLiteral(builder->createHardValue("x")),
                                 builder->exprVariable(builder->createString("b"))),
        // print(a,b);
        builder->glue(
          builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
          builder->exprVariable(builder->createString("a")),
          builder->exprVariable(builder->createString("b"))
        )
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("[expando][expando]\n", vm.logger.logged.str());
}

TEST(TestVM, ExpandoCycle) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var a = expando();
      builder->stmtVariableDefine(builder->createString("a"), builder->exprFunctionCall(builder->exprVariable(builder->createString("expando")))),
      builder->glue(
        // var b = expando();
        builder->stmtVariableDefine(builder->createString("b"), builder->exprFunctionCall(builder->exprVariable(builder->createString("expando")))),
        // a.x = b;
        builder->stmtPropertySet(builder->exprVariable(builder->createString("a")),
                                 builder->exprLiteral(builder->createHardValue("x")),
                                 builder->exprVariable(builder->createString("b"))),
        // b.x = a;
        builder->stmtPropertySet(builder->exprVariable(builder->createString("b")),
                                 builder->exprLiteral(builder->createHardValue("x")),
                                 builder->exprVariable(builder->createString("a"))),
        // print(a,b);
        builder->glue(
          builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
          builder->exprVariable(builder->createString("a")),
          builder->exprVariable(builder->createString("b"))
        )
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("[expando][expando]\n", vm.logger.logged.str());
}

TEST(TestVM, ExpandoCollector) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var a = expando();
      builder->stmtVariableDefine(builder->createString("a"), builder->exprFunctionCall(builder->exprVariable(builder->createString("expando")))),
      builder->glue(
        // var b = expando();
        builder->stmtVariableDefine(builder->createString("b"), builder->exprFunctionCall(builder->exprVariable(builder->createString("expando")))),
        // a.x = b;
        builder->stmtPropertySet(builder->exprVariable(builder->createString("a")),
                                 builder->exprLiteral(builder->createHardValue("x")),
                                 builder->exprVariable(builder->createString("b"))),
        // b.x = a;
        builder->stmtPropertySet(builder->exprVariable(builder->createString("b")),
                                  builder->exprLiteral(builder->createHardValue("x")),
                                  builder->exprVariable(builder->createString("a"))),
        // print(collector()); -- should print '0'
        builder->glue(
          builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
          builder->exprFunctionCall(builder->exprVariable(builder->createString("collector")))
        ),
        // a =   null;
        builder->stmtVariableSet(builder->createString("a"), builder->exprLiteral(builder->createHardValue(nullptr))),
        // print(collector()); -- should print '0'
        builder->glue(
          builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
          builder->exprFunctionCall(builder->exprVariable(builder->createString("collector")))
        ),
        // b = null;
        builder->stmtVariableSet(builder->createString("b"), builder->exprLiteral(builder->createHardValue(nullptr))),
        // print(collector()); -- should print '4'
        builder->glue(
          builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
          builder->exprFunctionCall(builder->exprVariable(builder->createString("collector")))
        )
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("0\n0\n4\n", vm.logger.logged.str());
}

TEST(TestVM, ExpandoKeyOrdering) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    builder->glue(
      // var x = expando();
      builder->stmtVariableDefine(builder->createString("x"), builder->exprFunctionCall(builder->exprVariable(builder->createString("expando")))),
      // x.n = null;
      builder->stmtPropertySet(builder->exprVariable(builder->createString("x")),
                               builder->exprLiteral(builder->createHardValue("n")),
                               builder->exprLiteral(builder->createHardValueNull())),
      // x.b = true;
      builder->stmtPropertySet(builder->exprVariable(builder->createString("x")),
                               builder->exprLiteral(builder->createHardValue("b")),
                               builder->exprLiteral(builder->createHardValueBool(true))),
      // x.i = 12345;
      builder->stmtPropertySet(builder->exprVariable(builder->createString("x")),
                               builder->exprLiteral(builder->createHardValue("i")),
                               builder->exprLiteral(builder->createHardValueInt(12345))),
      // x.f = 1234.5;
      builder->stmtPropertySet(builder->exprVariable(builder->createString("x")),
                               builder->exprLiteral(builder->createHardValue("f")),
                               builder->exprLiteral(builder->createHardValueFloat(1234.5))),
      // x.s = "hello world";
      builder->stmtPropertySet(builder->exprVariable(builder->createString("x")),
                               builder->exprLiteral(builder->createHardValue("s")),
                               builder->exprLiteral(builder->createHardValue("hello world"))),
      // x.o = x;
      builder->stmtPropertySet(builder->exprVariable(builder->createString("x")),
                               builder->exprLiteral(builder->createHardValue("o")),
                               builder->exprVariable(builder->createString("x"))),
      // print(x.b); -- should print 'true'
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprPropertyGet(builder->exprVariable(builder->createString("x")),
                                 builder->exprLiteral(builder->createHardValue("b")))
      ),
      // print(x.f); -- should print '1234.5'
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprPropertyGet(builder->exprVariable(builder->createString("x")),
                                 builder->exprLiteral(builder->createHardValue("f")))
      ),
      // print(x.i); -- should print '12345'
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprPropertyGet(builder->exprVariable(builder->createString("x")),
                                 builder->exprLiteral(builder->createHardValue("i")))
      ),
      // print(x.n); -- should print 'null'
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprPropertyGet(builder->exprVariable(builder->createString("x")),
                                 builder->exprLiteral(builder->createHardValue("n")))
      ),
      // print(x.o); -- should print '[expando]'
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprPropertyGet(builder->exprVariable(builder->createString("x")),
                                 builder->exprLiteral(builder->createHardValue("o")))
      ),
      // print(x.s); -- should print 'hello world'
      builder->glue(
        builder->stmtFunctionCall(builder->exprVariable(builder->createString("print"))),
        builder->exprPropertyGet(builder->exprVariable(builder->createString("x")),
                                 builder->exprLiteral(builder->createHardValue("s")))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("true\n1234.5\n12345\nnull\n[expando]\nhello world\n", vm.logger.logged.str());
}
