#include "ovum/test.h"

// C++2x (post C++17) introduces __VA_OPT__
#if defined(__cplusplus) && __cplusplus > 201703L
#define COMMA(...) __VA_OPT__(,) __VA_ARGS__
#else
#define COMMA(...) , __VA_ARGS__
#endif

#define EXPR_BINARY(op, lhs, rhs) builder->exprBinaryOp(IVMExecution::BinaryOp::op, lhs, rhs)
#define EXPR_CALL(func, ...) builder->glue(builder->exprFunctionCall(func) COMMA(__VA_ARGS__))
#define EXPR_LITERAL(value) builder->exprLiteral(builder->createHardValue(value))
#define EXPR_PROP_GET(instance, property) builder->exprPropertyGet(instance, property)
#define EXPR_VAR(name) builder->exprVariable(builder->createString(name))
#define STMT_CALL(func, ...) builder->glue(builder->stmtFunctionCall(func) COMMA(__VA_ARGS__))
#define STMT_PRINT(...) builder->glue(builder->stmtFunctionCall(EXPR_VAR("print")) COMMA(__VA_ARGS__))
#define STMT_PROP_SET(instance, property, value) builder->stmtPropertySet(instance, property, value)
#define STMT_VAR_DECLARE(name, ...) builder->glue(builder->stmtVariableDeclare(builder->createString(name)) COMMA(__VA_ARGS__))
#define STMT_VAR_DEFINE(name, value, ...) builder->glue(builder->stmtVariableDefine(builder->createString(name), value) COMMA(__VA_ARGS__))
#define STMT_VAR_SET(name, value) builder->stmtVariableSet(builder->createString(name), value)

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
  // print(print);
  builder->addStatement(STMT_PRINT(EXPR_VAR("print")));
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("[builtin print]\n", vm.logger.logged.str());
}

TEST(TestVM, PrintUnknown) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // print(unknown);
  builder->addStatement(STMT_PRINT(EXPR_VAR("unknown")));
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Unknown variable symbol: 'unknown'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var v;
    STMT_VAR_DECLARE("v",
      // print(v);
      STMT_PRINT(EXPR_VAR("v"))
    )
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable uninitialized: 'v'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDeclareTwice) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var v;
    STMT_VAR_DECLARE("v",
      // var v;
      STMT_VAR_DECLARE("v")
    )
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared: 'v'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefine) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i = 12345;
    STMT_VAR_DEFINE("i", EXPR_LITERAL(12345),
     // print(i);
      STMT_PRINT(EXPR_VAR("i"))
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
    STMT_VAR_DECLARE("i")
  );
  builder->addStatement(
    // print(i);
    STMT_PRINT(EXPR_VAR("i"))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Unknown variable symbol: 'i'\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineNull) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var n = null;
    STMT_VAR_DEFINE("n", EXPR_LITERAL(nullptr),
      // print(n);
      STMT_PRINT(EXPR_VAR("n"))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("null\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineBool) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var b = true;
    STMT_VAR_DEFINE("b", EXPR_LITERAL(true),
      // print(b);
      STMT_PRINT(EXPR_VAR("b"))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("true\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineInt) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i = 12345;
    STMT_VAR_DEFINE("i", EXPR_LITERAL(12345),
        // print(i);
      STMT_PRINT(EXPR_VAR("i"))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("12345\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineFloat) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var f = 1234.5;
    STMT_VAR_DEFINE("f", EXPR_LITERAL(1234.5),
        // print(f);
      STMT_PRINT(EXPR_VAR("f"))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("1234.5\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineString) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var s = "hello world";
    STMT_VAR_DEFINE("s", EXPR_LITERAL("hello world"),
      // print(s);
      STMT_PRINT(EXPR_VAR("s"))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("hello world\n", vm.logger.logged.str());
}

TEST(TestVM, VariableDefineObject) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var o = print;
    STMT_VAR_DEFINE("o", EXPR_VAR("print"),
      // print(o);
      STMT_PRINT(EXPR_VAR("o"))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("[builtin print]\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinDeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var print;
    STMT_VAR_DECLARE("print")
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared as a builtin: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinDefine) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var print = null;
    STMT_VAR_DEFINE("print", EXPR_LITERAL(nullptr))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared as a builtin: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinSet) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // print = 12345;
  builder->addStatement(
    STMT_VAR_SET("print", EXPR_LITERAL(12345))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Cannot modify builtin symbol: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, AssertTrue) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // assert(true);
  builder->addStatement(
    STMT_CALL(EXPR_VAR("assert"), EXPR_LITERAL(true))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestVM, AssertFalse) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // assert(false);
  builder->addStatement(
    STMT_CALL(EXPR_VAR("assert"), EXPR_LITERAL(false))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("<ERROR>throw Assertion failure\n", vm.logger.logged.str());
}

TEST(TestVM, ExpandoPair) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var a = expando();
    STMT_VAR_DEFINE("a", EXPR_CALL(EXPR_VAR("expando")),
      // var b = expando();
      STMT_VAR_DEFINE("b", EXPR_CALL(EXPR_VAR("expando")),
        // a.x = b;
        STMT_PROP_SET(EXPR_VAR("a"), EXPR_LITERAL("x"), EXPR_VAR("b")),
        // print(a,b);
        STMT_PRINT(EXPR_VAR("a"), EXPR_VAR("b"))
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
    // var a = expando();
    STMT_VAR_DEFINE("a", EXPR_CALL(EXPR_VAR("expando")),
      // var b = expando();
      STMT_VAR_DEFINE("b", EXPR_CALL(EXPR_VAR("expando")),
        // a.x = b;
        STMT_PROP_SET(EXPR_VAR("a"), EXPR_LITERAL("x"), EXPR_VAR("b")),
        // b.x = a;
        STMT_PROP_SET(EXPR_VAR("b"), EXPR_LITERAL("x"), EXPR_VAR("a")),
        // print(a,b);
        STMT_PRINT(EXPR_VAR("a"), EXPR_VAR("b"))
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
    // var a = expando();
    STMT_VAR_DEFINE("a", EXPR_CALL(EXPR_VAR("expando")),
      // var b = expando();
      STMT_VAR_DEFINE("b", EXPR_CALL(EXPR_VAR("expando")),
        // a.x = b;
        STMT_PROP_SET(EXPR_VAR("a"), EXPR_LITERAL("x"), EXPR_VAR("b")),
        // b.x = a;
        STMT_PROP_SET(EXPR_VAR("b"), EXPR_LITERAL("x"), EXPR_VAR("a")),
        // print(collector()); -- should print '0'
        STMT_PRINT(EXPR_CALL(EXPR_VAR("collector"))),
        // a =   null;
        STMT_VAR_SET("a", EXPR_LITERAL(nullptr)),
        // print(collector()); -- should print '0'
        STMT_PRINT(EXPR_CALL(EXPR_VAR("collector"))),
        // b = null;
        STMT_VAR_SET("b", EXPR_LITERAL(nullptr)),
        // print(collector()); -- should print '4'
        STMT_PRINT(EXPR_CALL(EXPR_VAR("collector")))
      )
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("0\n0\n4\n", vm.logger.logged.str());
}

TEST(TestVM, ExpandoKeys) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var x = expando();
    STMT_VAR_DEFINE("x", EXPR_CALL(EXPR_VAR("expando")),
      // x.n = null;
      STMT_PROP_SET(EXPR_VAR("x"), EXPR_LITERAL("n"), EXPR_LITERAL(nullptr)),
      // x.b = true;
      STMT_PROP_SET(EXPR_VAR("x"), EXPR_LITERAL("b"), EXPR_LITERAL(true)),
      // x.i = 12345;
      STMT_PROP_SET(EXPR_VAR("x"), EXPR_LITERAL("i"), EXPR_LITERAL(12345)),
      // x.f = 1234.5;
      STMT_PROP_SET(EXPR_VAR("x"), EXPR_LITERAL("f"), EXPR_LITERAL(1234.5)),
      // x.s = "hello world";
      STMT_PROP_SET(EXPR_VAR("x"), EXPR_LITERAL("s"), EXPR_LITERAL("hello world")),
      // x.o = x;
      STMT_PROP_SET(EXPR_VAR("x"), EXPR_LITERAL("o"), EXPR_VAR("x")),
      // print(x.b); -- should print 'true'
      STMT_PRINT(EXPR_PROP_GET(EXPR_VAR("x"), EXPR_LITERAL("b"))),
      // print(x.f); -- should print '1234.5'
      STMT_PRINT(EXPR_PROP_GET(EXPR_VAR("x"), EXPR_LITERAL("f"))),
      // print(x.i); -- should print '12345'
      STMT_PRINT(EXPR_PROP_GET(EXPR_VAR("x"), EXPR_LITERAL("i"))),
      // print(x.n); -- should print 'null'
      STMT_PRINT(EXPR_PROP_GET(EXPR_VAR("x"), EXPR_LITERAL("n"))),
      // print(x.o); -- should print '[expando]'
      STMT_PRINT(EXPR_PROP_GET(EXPR_VAR("x"), EXPR_LITERAL("o"))),
      // print(x.s); -- should print 'hello world'
      STMT_PRINT(EXPR_PROP_GET(EXPR_VAR("x"), EXPR_LITERAL("s")))
    )
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("true\n1234.5\n12345\nnull\n[expando]\nhello world\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryAdd) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 + 456);
    STMT_PRINT(EXPR_BINARY(Add, EXPR_LITERAL(123), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123.5 + 456);
    STMT_PRINT(EXPR_BINARY(Add, EXPR_LITERAL(123.25), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123 + 456.5);
    STMT_PRINT(EXPR_BINARY(Add, EXPR_LITERAL(123), EXPR_LITERAL(456.5)))
  );
  builder->addStatement(
    // print(123.25 + 456.5);
    STMT_PRINT(EXPR_BINARY(Add, EXPR_LITERAL(123.25), EXPR_LITERAL(456.5)))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("579\n579.25\n579.5\n579.75\n", vm.logger.logged.str());
}

TEST(TestVM, BinarySub) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 - 456);
    STMT_PRINT(EXPR_BINARY(Sub, EXPR_LITERAL(123), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123.5 - 456);
    STMT_PRINT(EXPR_BINARY(Sub, EXPR_LITERAL(123.25), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123 - 456.5);
    STMT_PRINT(EXPR_BINARY(Sub, EXPR_LITERAL(123), EXPR_LITERAL(456.5)))
  );
  builder->addStatement(
    // print(123.25 - 456.5);
    STMT_PRINT(EXPR_BINARY(Sub, EXPR_LITERAL(123.25), EXPR_LITERAL(456.5)))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("-333\n-332.75\n-333.5\n-333.25\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryMul) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 * 456);
    STMT_PRINT(EXPR_BINARY(Mul, EXPR_LITERAL(123), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123.5 * 456);
    STMT_PRINT(EXPR_BINARY(Mul, EXPR_LITERAL(123.25), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123 * 456.5);
    STMT_PRINT(EXPR_BINARY(Mul, EXPR_LITERAL(123), EXPR_LITERAL(456.5)))
  );
  builder->addStatement(
    // print(123.25 * 456.5);
    STMT_PRINT(EXPR_BINARY(Mul, EXPR_LITERAL(123.25), EXPR_LITERAL(456.5)))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("56088\n56202.0\n56149.5\n56263.625\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryDiv) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 / 456);
    STMT_PRINT(EXPR_BINARY(Div, EXPR_LITERAL(123), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123.5 / 456);
    STMT_PRINT(EXPR_BINARY(Div, EXPR_LITERAL(123.25), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123 / 456.5);
    STMT_PRINT(EXPR_BINARY(Div, EXPR_LITERAL(123), EXPR_LITERAL(456.5)))
  );
  builder->addStatement(
    // print(123.25 / 456.5);
    STMT_PRINT(EXPR_BINARY(Div, EXPR_LITERAL(123.25), EXPR_LITERAL(456.5)))
  );
  buildAndRunSuccess(vm, *builder);
  ASSERT_EQ("0\n0.270285087719\n0.269441401972\n0.269989047097\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryDivZero) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123.5 / 0);
    STMT_PRINT(EXPR_BINARY(Div, EXPR_LITERAL(123.25), EXPR_LITERAL(0)))
  );
  builder->addStatement(
    // print(123 / 0.0);
    STMT_PRINT(EXPR_BINARY(Div, EXPR_LITERAL(123), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(123.25 / 0.0);
    STMT_PRINT(EXPR_BINARY(Div, EXPR_LITERAL(123.25), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(123 / 0);
    STMT_PRINT(EXPR_BINARY(Div, EXPR_LITERAL(123), EXPR_LITERAL(0)))
  );
  buildAndRunFault(vm, *builder);
  ASSERT_EQ("#+INF\n#+INF\n#+INF\n<ERROR>throw TODO: Integer division by zero in '/' division binary operator\n", vm.logger.logged.str());
}
