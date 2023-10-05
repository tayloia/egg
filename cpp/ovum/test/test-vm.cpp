#include "ovum/test.h"

// C++2x (post C++17) introduces __VA_OPT__
#if defined(__cplusplus) && __cplusplus > 201703L
#define COMMA(...) __VA_OPT__(,) __VA_ARGS__
#else
#define COMMA(...) , __VA_ARGS__
#endif

#define EXPR_UNARY(op, arg) builder->exprUnaryOp(IVMExecution::UnaryOp::op, arg)
#define EXPR_BINARY(op, lhs, rhs) builder->exprBinaryOp(IVMExecution::BinaryOp::op, lhs, rhs)
#define EXPR_CALL(func, ...) builder->glue(builder->exprFunctionCall(func) COMMA(__VA_ARGS__))
#define EXPR_LITERAL(value) createLiteral(*builder, value)
#define EXPR_PROP_GET(instance, property) builder->exprPropertyGet(instance, property)
#define EXPR_VAR(name) builder->exprVariable(builder->createString(name))
#define STMT_BLOCK(...) builder->glue(builder->stmtBlock() COMMA(__VA_ARGS__))
#define STMT_IF(cond, ...) builder->glue(builder->stmtIf(cond) COMMA(__VA_ARGS__))
#define STMT_WHILE(cond, block) builder->stmtWhile(cond, block)
#define STMT_DO(block, cond) builder->stmtDo(block, cond)
#define STMT_FOR(init, cond, advance, block) builder->stmtFor(init, cond, advance, block)
#define STMT_SWITCH(expr, defidx, ...) builder->glue(builder->stmtSwitch(expr, defidx) COMMA(__VA_ARGS__))
#define STMT_CASE(block, ...) builder->glue(builder->stmtCase(block) COMMA(__VA_ARGS__))
#define STMT_BREAK() builder->stmtBreak()
#define STMT_CONTINUE() builder->stmtContinue()
#define STMT_CALL(func, ...) builder->glue(builder->stmtFunctionCall(func) COMMA(__VA_ARGS__))
#define STMT_PRINT(...) builder->glue(builder->stmtFunctionCall(EXPR_VAR("print")) COMMA(__VA_ARGS__))
#define STMT_PROP_SET(instance, property, value) builder->stmtPropertySet(instance, property, value)
#define STMT_VAR_DECLARE(name, ...) builder->glue(builder->stmtVariableDeclare(builder->createString(name)) COMMA(__VA_ARGS__))
#define STMT_VAR_DEFINE(name, value, ...) builder->glue(builder->stmtVariableDefine(builder->createString(name), value) COMMA(__VA_ARGS__))
#define STMT_VAR_SET(name, value) builder->stmtVariableSet(builder->createString(name), value)
#define STMT_VAR_MUTATE(name, op, value) builder->stmtVariableMutate(builder->createString(name), IVMExecution::MutationOp::op, value)

#define VOID VoidTag{}

#define ADD_STATEMENT_MUTATE(op, lhs, rhs) \
  builder->addStatement( \
    STMT_VAR_DEFINE("x", EXPR_LITERAL(lhs), \
      STMT_VAR_MUTATE("x", op, EXPR_LITERAL(rhs)), \
      STMT_PRINT(EXPR_VAR("x"))))

namespace {
  using namespace egg::ovum;

  struct VoidTag {};

  IVMProgramBuilder::Node& createLiteral(IVMProgramBuilder& builder, const VoidTag&) {
    return builder.exprLiteral(builder.createHardValueVoid());
  }

  template<typename T>
  IVMProgramBuilder::Node& createLiteral(IVMProgramBuilder& builder, T value) {
    return builder.exprLiteral(builder.createHardValue(value));
  }

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
    if (retval.hasFlowControl()) {
      vm.logger.log(ILogger::Source::User, ILogger::Severity::Error, vm.allocator.concat(retval));
    }
    return outcome;
  }
  void buildAndRunSucceeded(egg::test::VM& vm, IVMProgramBuilder& builder, IVMProgramRunner::RunFlags flags = IVMProgramRunner::RunFlags::Default) {
    HardValue retval;
    auto outcome = buildAndRun(vm, builder, retval, flags);
    ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Succeeded, outcome);
    ASSERT_VALUE(egg::ovum::HardValue::Void, retval);
  }
  void buildAndRunFailed(egg::test::VM& vm, IVMProgramBuilder& builder, IVMProgramRunner::RunFlags flags = IVMProgramRunner::RunFlags::Default) {
    HardValue retval;
    auto outcome = buildAndRun(vm, builder, retval, flags);
    ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Failed, outcome);
    ASSERT_EQ(egg::ovum::ValueFlags::Throw | egg::ovum::ValueFlags::String, retval->getFlags());
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
  ASSERT_EQ(egg::ovum::IVMProgramRunner::RunOutcome::Succeeded, outcome);
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
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("[builtin print]\n", vm.logger.logged.str());
}

TEST(TestVM, PrintUnknown) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // print(unknown);
  builder->addStatement(STMT_PRINT(EXPR_VAR("unknown")));
  buildAndRunFailed(vm, *builder);
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
  buildAndRunFailed(vm, *builder);
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
  buildAndRunFailed(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunFailed(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("[builtin print]\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinDeclare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var print;
    STMT_VAR_DECLARE("print")
  );
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared as a builtin: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinDefine) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var print = null;
    STMT_VAR_DEFINE("print", EXPR_LITERAL(nullptr))
  );
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("<ERROR>throw Variable symbol already declared as a builtin: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, BuiltinSet) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // print = 12345;
  builder->addStatement(
    STMT_VAR_SET("print", EXPR_LITERAL(12345))
  );
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("<ERROR>throw Cannot modify builtin symbol: 'print'\n", vm.logger.logged.str());
}

TEST(TestVM, AssertTrue) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // assert(true);
  builder->addStatement(
    STMT_CALL(EXPR_VAR("assert"), EXPR_LITERAL(true))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestVM, AssertFalse) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  // assert(false);
  builder->addStatement(
    STMT_CALL(EXPR_VAR("assert"), EXPR_LITERAL(false))
  );
  buildAndRunFailed(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
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
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("true\n1234.5\n12345\nnull\n[expando]\nhello world\n", vm.logger.logged.str());
}

TEST(TestVM, UnaryNegate) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(-(123));
    STMT_PRINT(EXPR_UNARY(Negate, EXPR_LITERAL(123)))
  );
  builder->addStatement(
    // print(-(-123));
    STMT_PRINT(EXPR_UNARY(Negate, EXPR_LITERAL(-123)))
  );
  builder->addStatement(
    // print(-(123.5));
    STMT_PRINT(EXPR_UNARY(Negate, EXPR_LITERAL(123.5)))
  );
  builder->addStatement(
    // print(-(-123.5));
    STMT_PRINT(EXPR_UNARY(Negate, EXPR_LITERAL(-123.5)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("-123\n123\n-123.5\n123.5\n", vm.logger.logged.str());
}

TEST(TestVM, UnaryBitwiseNot) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(~5);
    STMT_PRINT(EXPR_UNARY(BitwiseNot, EXPR_LITERAL(5)))
  );
  builder->addStatement(
    // print(~-5);
    STMT_PRINT(EXPR_UNARY(BitwiseNot, EXPR_LITERAL(-5)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("-6\n4\n", vm.logger.logged.str());
}

TEST(TestVM, UnaryLogicalNot) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(!false);
    STMT_PRINT(EXPR_UNARY(LogicalNot, EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(!true);
    STMT_PRINT(EXPR_UNARY(LogicalNot, EXPR_LITERAL(true)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("true\nfalse\n", vm.logger.logged.str());
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
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("579\n579.25\n579.5\n579.75\n", vm.logger.logged.str());
}

TEST(TestVM, BinarySubtract) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 - 456);
    STMT_PRINT(EXPR_BINARY(Subtract, EXPR_LITERAL(123), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123.5 - 456);
    STMT_PRINT(EXPR_BINARY(Subtract, EXPR_LITERAL(123.25), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123 - 456.5);
    STMT_PRINT(EXPR_BINARY(Subtract, EXPR_LITERAL(123), EXPR_LITERAL(456.5)))
  );
  builder->addStatement(
    // print(123.25 - 456.5);
    STMT_PRINT(EXPR_BINARY(Subtract, EXPR_LITERAL(123.25), EXPR_LITERAL(456.5)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("-333\n-332.75\n-333.5\n-333.25\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryMultiply) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 * 456);
    STMT_PRINT(EXPR_BINARY(Multiply, EXPR_LITERAL(123), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123.5 * 456);
    STMT_PRINT(EXPR_BINARY(Multiply, EXPR_LITERAL(123.25), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123 * 456.5);
    STMT_PRINT(EXPR_BINARY(Multiply, EXPR_LITERAL(123), EXPR_LITERAL(456.5)))
  );
  builder->addStatement(
    // print(123.25 * 456.5);
    STMT_PRINT(EXPR_BINARY(Multiply, EXPR_LITERAL(123.25), EXPR_LITERAL(456.5)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("56088\n56202.0\n56149.5\n56263.625\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryDivide) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 / 456);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(123), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123.5 / 456);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(123.25), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123 / 456.5);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(123), EXPR_LITERAL(456.5)))
  );
  builder->addStatement(
    // print(123.25 / 456.5);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(123.25), EXPR_LITERAL(456.5)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("0\n0.270285087719\n0.269441401972\n0.269989047097\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryDivideZero) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123.5 / 0);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(123.25), EXPR_LITERAL(0)))
  );
  builder->addStatement(
    // print(123 / 0.0);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(123), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(123.25 / 0.0);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(123.25), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(0 / 0.0);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(0), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(0.0 / 0.0);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(0.0), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(123 / 0);
    STMT_PRINT(EXPR_BINARY(Divide, EXPR_LITERAL(123), EXPR_LITERAL(0)))
  );
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("#+INF\n#+INF\n#+INF\n#NAN\n#NAN\n<ERROR>throw TODO: Integer division by zero in '/' division operator\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryRemainder) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 % 34);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(123), EXPR_LITERAL(34)))
  );
  builder->addStatement(
    // print(123.5 % 34);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(123.25), EXPR_LITERAL(34)))
  );
  builder->addStatement(
    // print(123 % 34.5);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(123), EXPR_LITERAL(34.5)))
  );
  builder->addStatement(
    // print(123.25 % 34.5);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(123.25), EXPR_LITERAL(34.5)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("21\n21.25\n19.5\n19.75\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryRemainderZero) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123.5 % 34);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(123.25), EXPR_LITERAL(0)))
  );
  builder->addStatement(
    // print(123 % 34.5);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(123), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(123.25 % 34.5);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(123.25), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(0 % 0.0);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(0), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(0.0 % 0.0);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(0.0), EXPR_LITERAL(0.0)))
  );
  builder->addStatement(
    // print(123 % 0);
    STMT_PRINT(EXPR_BINARY(Remainder, EXPR_LITERAL(123), EXPR_LITERAL(0)))
  );
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("#NAN\n#NAN\n#NAN\n#NAN\n#NAN\n<ERROR>throw TODO: Integer division by zero in '%' remainder operator\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryCompare) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(123 < 234);
    STMT_PRINT(EXPR_BINARY(LessThan, EXPR_LITERAL(123), EXPR_LITERAL(234)))
  );
  builder->addStatement(
    // print(123 <= 234);
    STMT_PRINT(EXPR_BINARY(LessThanOrEqual, EXPR_LITERAL(123), EXPR_LITERAL(234)))
  );
  builder->addStatement(
    // print(123 == 234);
    STMT_PRINT(EXPR_BINARY(Equal, EXPR_LITERAL(123), EXPR_LITERAL(234)))
  );
  builder->addStatement(
    // print(123 != 234);
    STMT_PRINT(EXPR_BINARY(NotEqual, EXPR_LITERAL(123), EXPR_LITERAL(234)))
  );
  builder->addStatement(
    // print(123 >= 234);
    STMT_PRINT(EXPR_BINARY(GreaterThanOrEqual, EXPR_LITERAL(123), EXPR_LITERAL(234)))
  );
  builder->addStatement(
    // print(123 > 234);
    STMT_PRINT(EXPR_BINARY(GreaterThan, EXPR_LITERAL(123), EXPR_LITERAL(234)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("true\ntrue\nfalse\ntrue\nfalse\nfalse\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryBitwiseBool) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(false & false);
    STMT_PRINT(EXPR_BINARY(BitwiseAnd, EXPR_LITERAL(false), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(false & true);
    STMT_PRINT(EXPR_BINARY(BitwiseAnd, EXPR_LITERAL(false), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(true & false);
    STMT_PRINT(EXPR_BINARY(BitwiseAnd, EXPR_LITERAL(true), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(true & true);
    STMT_PRINT(EXPR_BINARY(BitwiseAnd, EXPR_LITERAL(true), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(false | false);
    STMT_PRINT(EXPR_BINARY(BitwiseOr, EXPR_LITERAL(false), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(false | true);
    STMT_PRINT(EXPR_BINARY(BitwiseOr, EXPR_LITERAL(false), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(true | false);
    STMT_PRINT(EXPR_BINARY(BitwiseOr, EXPR_LITERAL(true), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(true | true);
    STMT_PRINT(EXPR_BINARY(BitwiseOr, EXPR_LITERAL(true), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(false ^ false);
    STMT_PRINT(EXPR_BINARY(BitwiseXor, EXPR_LITERAL(false), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(false ^ true);
    STMT_PRINT(EXPR_BINARY(BitwiseXor, EXPR_LITERAL(false), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(true ^ false);
    STMT_PRINT(EXPR_BINARY(BitwiseXor, EXPR_LITERAL(true), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(true ^ true);
    STMT_PRINT(EXPR_BINARY(BitwiseXor, EXPR_LITERAL(true), EXPR_LITERAL(true)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("false\nfalse\nfalse\ntrue\n"
            "false\ntrue\ntrue\ntrue\n"
            "false\ntrue\ntrue\nfalse\n",
            vm.logger.logged.str());
}

TEST(TestVM, BinaryBitwiseInt) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(10 & 3);
    STMT_PRINT(EXPR_BINARY(BitwiseAnd, EXPR_LITERAL(10), EXPR_LITERAL(3)))
  );
  builder->addStatement(
    // print(10 | 3);
    STMT_PRINT(EXPR_BINARY(BitwiseOr, EXPR_LITERAL(10), EXPR_LITERAL(3)))
  );
  builder->addStatement(
    // print(10 ^ 3);
    STMT_PRINT(EXPR_BINARY(BitwiseXor, EXPR_LITERAL(10), EXPR_LITERAL(3)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("2\n11\n9\n", vm.logger.logged.str());
}

TEST(TestVM, BinaryShift) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(7 << 2);
    STMT_PRINT(EXPR_BINARY(ShiftLeft, EXPR_LITERAL(7), EXPR_LITERAL(2)))
  );
  builder->addStatement(
    // print(7 << -2);
    STMT_PRINT(EXPR_BINARY(ShiftLeft, EXPR_LITERAL(7), EXPR_LITERAL(-2)))
  );
  builder->addStatement(
    // print(-7 << 2);
    STMT_PRINT(EXPR_BINARY(ShiftLeft, EXPR_LITERAL(-7), EXPR_LITERAL(2)))
  );
  builder->addStatement(
    // print(-7 << -2);
    STMT_PRINT(EXPR_BINARY(ShiftLeft, EXPR_LITERAL(-7), EXPR_LITERAL(-2)))
  );
  builder->addStatement(
    // print(7 >> 2);
    STMT_PRINT(EXPR_BINARY(ShiftRight, EXPR_LITERAL(7), EXPR_LITERAL(2)))
  );
  builder->addStatement(
    // print(7 >> -2);
    STMT_PRINT(EXPR_BINARY(ShiftRight, EXPR_LITERAL(7), EXPR_LITERAL(-2)))
  );
  builder->addStatement(
    // print(-7 >> 2);
    STMT_PRINT(EXPR_BINARY(ShiftRight, EXPR_LITERAL(-7), EXPR_LITERAL(2)))
  );
  builder->addStatement(
    // print(-7 >> -2);
    STMT_PRINT(EXPR_BINARY(ShiftRight, EXPR_LITERAL(-7), EXPR_LITERAL(-2)))
  );
  builder->addStatement(
    // print(7 >>> 2);
    STMT_PRINT(EXPR_BINARY(ShiftRightUnsigned, EXPR_LITERAL(7), EXPR_LITERAL(2)))
  );
  builder->addStatement(
    // print(7 >>> -2);
    STMT_PRINT(EXPR_BINARY(ShiftRightUnsigned, EXPR_LITERAL(7), EXPR_LITERAL(-2)))
  );
  builder->addStatement(
    // print(-7 >>> 2);
    STMT_PRINT(EXPR_BINARY(ShiftRightUnsigned, EXPR_LITERAL(-7), EXPR_LITERAL(2)))
  );
  builder->addStatement(
    // print(-7 >>> -2);
    STMT_PRINT(EXPR_BINARY(ShiftRightUnsigned, EXPR_LITERAL(-7), EXPR_LITERAL(-2)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("28\n1\n-28\n-2\n"
            "1\n28\n-2\n-28\n"
            "1\n28\n4611686018427387902\n-28\n",
            vm.logger.logged.str());
}

TEST(TestVM, BinaryLogical) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // print(false && false);
    STMT_PRINT(EXPR_BINARY(IfTrue, EXPR_LITERAL(false), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(false && true);
    STMT_PRINT(EXPR_BINARY(IfTrue, EXPR_LITERAL(false), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(true && false);
    STMT_PRINT(EXPR_BINARY(IfTrue, EXPR_LITERAL(true), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(true && true);
    STMT_PRINT(EXPR_BINARY(IfTrue, EXPR_LITERAL(true), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(false || false);
    STMT_PRINT(EXPR_BINARY(IfFalse, EXPR_LITERAL(false), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(false || true);
    STMT_PRINT(EXPR_BINARY(IfFalse, EXPR_LITERAL(false), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(true || false);
    STMT_PRINT(EXPR_BINARY(IfFalse, EXPR_LITERAL(true), EXPR_LITERAL(false)))
  );
  builder->addStatement(
    // print(true || true);
    STMT_PRINT(EXPR_BINARY(IfFalse, EXPR_LITERAL(true), EXPR_LITERAL(true)))
  );
  builder->addStatement(
    // print(null ?? null);
    STMT_PRINT(EXPR_BINARY(IfNull, EXPR_LITERAL(nullptr), EXPR_LITERAL(nullptr)))
  );
  builder->addStatement(
    // print(null ?? 456);
    STMT_PRINT(EXPR_BINARY(IfNull, EXPR_LITERAL(nullptr), EXPR_LITERAL(456)))
  );
  builder->addStatement(
    // print(123 ?? null);
    STMT_PRINT(EXPR_BINARY(IfNull, EXPR_LITERAL(123), EXPR_LITERAL(nullptr)))
  );
  builder->addStatement(
    // print(123 ?? 456);
    STMT_PRINT(EXPR_BINARY(IfNull, EXPR_LITERAL(123), EXPR_LITERAL(456)))
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("false\nfalse\nfalse\ntrue\n"
            "false\ntrue\ntrue\ntrue\n"
            "null\n456\n123\n123\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateDecrement) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i = 12345;
    STMT_VAR_DEFINE("i", EXPR_LITERAL(12345),
      // print(i);
      STMT_PRINT(EXPR_VAR("i")),
      // --i;
      STMT_VAR_MUTATE("i", Decrement, EXPR_LITERAL(VOID)),
      // print(i);
      STMT_PRINT(EXPR_VAR("i"))
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("12345\n12344\n", vm.logger.logged.str());
}

TEST(TestVM, MutateIncrement) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i = 12345;
    STMT_VAR_DEFINE("i", EXPR_LITERAL(12345),
      // print(i);
      STMT_PRINT(EXPR_VAR("i")),
      // ++i;
      STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID)),
      // print(i);
      STMT_PRINT(EXPR_VAR("i"))
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("12345\n12346\n", vm.logger.logged.str());
}

TEST(TestVM, MutateAdd) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(Add, 12345, 0); // 12345
  ADD_STATEMENT_MUTATE(Add, 12345.0, 0); // 12345
  ADD_STATEMENT_MUTATE(Add, 12345, 123); // 12468
  ADD_STATEMENT_MUTATE(Add, 12345, 123.5); // 12468.5
  ADD_STATEMENT_MUTATE(Add, 123.5, 12345); // 12468.5
  ADD_STATEMENT_MUTATE(Add, 123.5, 13.25); // 136.75
  ADD_STATEMENT_MUTATE(Add, 123, "bad");
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("12345\n12345.0\n12468\n12468.5\n12468.5\n136.75\n"
            "<ERROR>throw TODO: Mutation addition is only supported for values of type 'int' or 'float'\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateSubtract) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(Subtract, 12345, 0); // 12345
  ADD_STATEMENT_MUTATE(Subtract, 12345.0, 0); // 12345.0
  ADD_STATEMENT_MUTATE(Subtract, 12345, 123); // 12222
  ADD_STATEMENT_MUTATE(Subtract, 12345, 123.5); // 12221.5
  ADD_STATEMENT_MUTATE(Subtract, 123.5, 12345); // -12221.5
  ADD_STATEMENT_MUTATE(Subtract, 123.5, 13.25); // 110.25
  ADD_STATEMENT_MUTATE(Subtract, 123, "bad");
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("12345\n12345.0\n12222\n12221.5\n-12221.5\n110.25\n"
            "<ERROR>throw TODO: Mutation subtract is only supported for values of type 'int' or 'float'\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateMultiply) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(Multiply, 12345, 0); // 0
  ADD_STATEMENT_MUTATE(Multiply, 12345.0, 0); // 0.0
  ADD_STATEMENT_MUTATE(Multiply, 12345, 123); // 1518435
  ADD_STATEMENT_MUTATE(Multiply, 12345, 123.5); // 1524607.5
  ADD_STATEMENT_MUTATE(Multiply, 123.5, 12345); // 1524607.5
  ADD_STATEMENT_MUTATE(Multiply, 123.5, 13.25); // 1636.375
  ADD_STATEMENT_MUTATE(Multiply, 123, "bad");
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("0\n0.0\n1518435\n1524607.5\n1524607.5\n1636.375\n"
            "<ERROR>throw TODO: Mutation multiply is only supported for values of type 'int' or 'float'\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateDivide) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(Divide, 12345.0, 0); // #+INF
  ADD_STATEMENT_MUTATE(Divide, 12345, 2.5); // 4938.0
  ADD_STATEMENT_MUTATE(Divide, 12345, 2.5); // 4938.0
  ADD_STATEMENT_MUTATE(Divide, 123.5, 2); // 61.75
  ADD_STATEMENT_MUTATE(Divide, 123.5, 2.5); // 49.4
  ADD_STATEMENT_MUTATE(Divide, 12345, 0);
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("#+INF\n4938.0\n4938.0\n61.75\n49.4\n"
            "<ERROR>throw TODO: Division by zero in mutation divide\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateRemainder) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(Remainder, 12345.0, 0); // #NAN
  ADD_STATEMENT_MUTATE(Remainder, 12345, 3.5); // 0.5
  ADD_STATEMENT_MUTATE(Remainder, 12345, 3.5); // 0.5
  ADD_STATEMENT_MUTATE(Remainder, 123.5, 2); // 1.5
  ADD_STATEMENT_MUTATE(Remainder, 123.5, 1.5); // 0.5
  ADD_STATEMENT_MUTATE(Remainder, 12345, 0);
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("#NAN\n0.5\n0.5\n1.5\n0.5\n"
            "<ERROR>throw TODO: Division by zero in mutation remainder\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateBitwiseAnd) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(BitwiseAnd, false, false); // false
  ADD_STATEMENT_MUTATE(BitwiseAnd, false, true); // false
  ADD_STATEMENT_MUTATE(BitwiseAnd, true, false); // false
  ADD_STATEMENT_MUTATE(BitwiseAnd, true, true); // true
  ADD_STATEMENT_MUTATE(BitwiseAnd, 12345, 10); // 8
  ADD_STATEMENT_MUTATE(BitwiseAnd, 12345, false);
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("false\nfalse\nfalse\ntrue\n8\n"
            "<ERROR>throw TODO: Mutation bitwise-and is only supported for values of type 'bool' or 'int'\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateBitwiseOr) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(BitwiseOr, false, false); // false
  ADD_STATEMENT_MUTATE(BitwiseOr, false, true); // true
  ADD_STATEMENT_MUTATE(BitwiseOr, true, false); // true
  ADD_STATEMENT_MUTATE(BitwiseOr, true, true); // true
  ADD_STATEMENT_MUTATE(BitwiseOr, 12345, 10); // 12347
  ADD_STATEMENT_MUTATE(BitwiseOr, 12345, false);
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("false\ntrue\ntrue\ntrue\n12347\n"
            "<ERROR>throw TODO: Mutation bitwise-or is only supported for values of type 'bool' or 'int'\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateBitwiseXor) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(BitwiseXor, false, false); // false
  ADD_STATEMENT_MUTATE(BitwiseXor, false, true); // true
  ADD_STATEMENT_MUTATE(BitwiseXor, true, false); // true
  ADD_STATEMENT_MUTATE(BitwiseXor, true, true); // false
  ADD_STATEMENT_MUTATE(BitwiseXor, 12345, 10); // 12339
  ADD_STATEMENT_MUTATE(BitwiseXor, 12345, false);
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("false\ntrue\ntrue\nfalse\n12339\n"
            "<ERROR>throw TODO: Mutation bitwise-xor is only supported for values of type 'bool' or 'int'\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateShiftLeft) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(ShiftLeft, 12345, 10); // 12641280
  ADD_STATEMENT_MUTATE(ShiftLeft, 12345, -10); // 12
  ADD_STATEMENT_MUTATE(ShiftLeft, -12345, 10); // -12641280
  ADD_STATEMENT_MUTATE(ShiftLeft, -12345, -10); // -13
  ADD_STATEMENT_MUTATE(ShiftLeft, 12345, false);
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("12641280\n12\n-12641280\n-13\n"
            "<ERROR>throw TODO: Mutation shift left is only supported for values of type 'int'\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateShiftRight) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(ShiftRight, 12345, 10); // 12
  ADD_STATEMENT_MUTATE(ShiftRight, 12345, -10); // 12641280
  ADD_STATEMENT_MUTATE(ShiftRight, -12345, 10); // -13
  ADD_STATEMENT_MUTATE(ShiftRight, -12345, -10); // -12641280
  ADD_STATEMENT_MUTATE(ShiftRight, 12345, false);
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("12\n12641280\n-13\n-12641280\n"
            "<ERROR>throw TODO: Mutation shift right is only supported for values of type 'int'\n",
            vm.logger.logged.str());
}

TEST(TestVM, MutateShiftRightUnsigned) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  ADD_STATEMENT_MUTATE(ShiftRightUnsigned, 12345, 10); // 12
  ADD_STATEMENT_MUTATE(ShiftRightUnsigned, 12345, -10); // 12641280
  ADD_STATEMENT_MUTATE(ShiftRightUnsigned, -12345, 10); // 18014398509481971
  ADD_STATEMENT_MUTATE(ShiftRightUnsigned, -12345, -10); // -12641280
  ADD_STATEMENT_MUTATE(ShiftRightUnsigned, 12345, false);
  buildAndRunFailed(vm, *builder);
  ASSERT_EQ("12\n12641280\n18014398509481971\n-12641280\n"
            "<ERROR>throw TODO: Mutation unsigned shift right is only supported for values of type 'int'\n",
            vm.logger.logged.str());
}

TEST(TestVM, Block) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    STMT_BLOCK(
      // print("a");
      STMT_PRINT(EXPR_LITERAL("a")),
      // print("b");
      STMT_PRINT(EXPR_LITERAL("b"))
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("a\nb\n", vm.logger.logged.str());
}

TEST(TestVM, If) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var a = 1;
    STMT_VAR_DEFINE("a", EXPR_LITERAL(1),
      // var b = 2;
      STMT_VAR_DEFINE("b", EXPR_LITERAL(2),
        // if (a < b) { a = "X"; }
        STMT_IF(EXPR_BINARY(LessThan, EXPR_VAR("a"), EXPR_VAR("b")),
          STMT_BLOCK(
            // a = "X";
            STMT_VAR_SET("a", EXPR_LITERAL("X"))
          )
        ),
        // print(a, b);
        STMT_PRINT(EXPR_VAR("a"), EXPR_VAR("b"))
      )
    )
  );
  builder->addStatement(
    // var a = 1;
    STMT_VAR_DEFINE("a", EXPR_LITERAL(1),
      // var b = 2;
      STMT_VAR_DEFINE("b", EXPR_LITERAL(2),
        // if (a > b) { a = "X"; }
        STMT_IF(EXPR_BINARY(GreaterThan, EXPR_VAR("a"), EXPR_VAR("b")),
          STMT_BLOCK(
            // a = "X";
            STMT_VAR_SET("a", EXPR_LITERAL("X"))
          )
        ),
        // print(a, b);
        STMT_PRINT(EXPR_VAR("a"), EXPR_VAR("b"))
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("X2\n12\n", vm.logger.logged.str());
}

TEST(TestVM, IfElse) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var a = 1;
    STMT_VAR_DEFINE("a", EXPR_LITERAL(1),
      // var b = 2;
      STMT_VAR_DEFINE("b", EXPR_LITERAL(2),
        // if (a < b) { a = "X"; } else { b = "X" }
        STMT_IF(EXPR_BINARY(LessThan, EXPR_VAR("a"), EXPR_VAR("b")),
          STMT_BLOCK(
            // a = "X";
            STMT_VAR_SET("a", EXPR_LITERAL("X"))
          ),
          STMT_BLOCK(
            // b = "Y";
            STMT_VAR_SET("b", EXPR_LITERAL("Y"))
          )
        ),
        // print(a, b);
        STMT_PRINT(EXPR_VAR("a"), EXPR_VAR("b"))
      )
    )
  );
  builder->addStatement(
    // var a = 1;
    STMT_VAR_DEFINE("a", EXPR_LITERAL(1),
      // var b = 2;
      STMT_VAR_DEFINE("b", EXPR_LITERAL(2),
        // if (a > b) { a = "X"; } else { b = "X" }
        STMT_IF(EXPR_BINARY(GreaterThan, EXPR_VAR("a"), EXPR_VAR("b")),
          STMT_BLOCK(
            // a = "X";
            STMT_VAR_SET("a", EXPR_LITERAL("X"))
          ),
          STMT_BLOCK(
            // b = "Y";
            STMT_VAR_SET("b", EXPR_LITERAL("Y"))
          )
        ),
        // print(a, b);
        STMT_PRINT(EXPR_VAR("a"), EXPR_VAR("b"))
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("X2\n1Y\n", vm.logger.logged.str());
}

TEST(TestVM, While) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i = 1;
    STMT_VAR_DEFINE("i", EXPR_LITERAL(1),
      // while (i < 10)
      STMT_WHILE(EXPR_BINARY(LessThan, EXPR_VAR("i"), EXPR_LITERAL(10)),
        STMT_BLOCK(
          // print(i);
          STMT_PRINT(EXPR_VAR("i")),
          // ++i;
          STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID))
        )
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("1\n2\n3\n4\n5\n6\n7\n8\n9\n", vm.logger.logged.str());
}

TEST(TestVM, Do) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i = 1;
    STMT_VAR_DEFINE("i", EXPR_LITERAL(1),
      // do ... while (i < 10)
      STMT_DO(
        STMT_BLOCK(
          // print(i);
          STMT_PRINT(EXPR_VAR("i")),
          // ++i;
          STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID))
        ),
        EXPR_BINARY(LessThan, EXPR_VAR("i"), EXPR_LITERAL(10))
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("1\n2\n3\n4\n5\n6\n7\n8\n9\n", vm.logger.logged.str());
}

TEST(TestVM, For) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i;
    STMT_VAR_DECLARE("i",
      // for (...)
      STMT_FOR(
        // i = 1;
        STMT_VAR_SET("i", EXPR_LITERAL(1)),
        // i < 10;
        EXPR_BINARY(LessThan, EXPR_VAR("i"), EXPR_LITERAL(10)),
        // ++i;
        STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID)),
        // print(i);
        STMT_PRINT(EXPR_VAR("i"))
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("1\n2\n3\n4\n5\n6\n7\n8\n9\n", vm.logger.logged.str());
}

TEST(TestVM, SwitchCaseBreak) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i;
    STMT_VAR_DECLARE("i",
      // for (...)
      STMT_FOR(
        // i = 1;
        STMT_VAR_SET("i", EXPR_LITERAL(1)),
        // i < 10;
        EXPR_BINARY(LessThan, EXPR_VAR("i"), EXPR_LITERAL(10)),
        // ++i;
        STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID)),
        // switch (i) without default
        STMT_SWITCH(EXPR_VAR("i"), 0,
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("one")), STMT_BREAK()), EXPR_LITERAL(1)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("two")), STMT_BREAK()), EXPR_LITERAL(2)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("three")), STMT_BREAK()), EXPR_LITERAL(3)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("four")), STMT_BREAK()), EXPR_LITERAL(4)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("five")), STMT_BREAK()), EXPR_LITERAL(5))
        )
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("one\ntwo\nthree\nfour\nfive\n", vm.logger.logged.str());
}

TEST(TestVM, SwitchCaseContinue) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i;
    STMT_VAR_DECLARE("i",
      // for (...)
      STMT_FOR(
        // i = 1;
        STMT_VAR_SET("i", EXPR_LITERAL(1)),
        // i < 10;
        EXPR_BINARY(LessThan, EXPR_VAR("i"), EXPR_LITERAL(10)),
        // ++i;
        STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID)),
        // switch (i) without default
        STMT_SWITCH(EXPR_VAR("i"), 0,
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("one")), STMT_CONTINUE()), EXPR_LITERAL(1)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("two")), STMT_BREAK()), EXPR_LITERAL(2)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("three")), STMT_CONTINUE()), EXPR_LITERAL(3)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("four")), STMT_BREAK()), EXPR_LITERAL(4)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("five")), STMT_CONTINUE()), EXPR_LITERAL(5))
        )
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("one\ntwo\n"
            "two\n"
            "three\nfour\n"
            "four\n"
            "five\none\ntwo\n", vm.logger.logged.str());
}

TEST(TestVM, SwitchDefaultBreak) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i;
    STMT_VAR_DECLARE("i",
      // for (...)
      STMT_FOR(
        // i = 1;
        STMT_VAR_SET("i", EXPR_LITERAL(1)),
        // i < 10;
        EXPR_BINARY(LessThan, EXPR_VAR("i"), EXPR_LITERAL(10)),
        // ++i;
        STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID)),
        // switch (i) with default
        STMT_SWITCH(EXPR_VAR("i"), 6,
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("one")), STMT_BREAK()), EXPR_LITERAL(1)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("two")), STMT_BREAK()), EXPR_LITERAL(2)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("three")), STMT_BREAK()), EXPR_LITERAL(3)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("four")), STMT_BREAK()), EXPR_LITERAL(4)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("five")), STMT_BREAK()), EXPR_LITERAL(5)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("other")), STMT_BREAK()))
        )
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("one\ntwo\nthree\nfour\nfive\nother\nother\nother\nother\n", vm.logger.logged.str());
}

TEST(TestVM, SwitchDefaultContinue) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i;
    STMT_VAR_DECLARE("i",
      // for (...)
      STMT_FOR(
        // i = 1;
        STMT_VAR_SET("i", EXPR_LITERAL(1)),
        // i < 10;
        EXPR_BINARY(LessThan, EXPR_VAR("i"), EXPR_LITERAL(10)),
        // ++i;
        STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID)),
        // switch (i) with default
        STMT_SWITCH(EXPR_VAR("i"), 1,
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("other")), STMT_CONTINUE())),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("one")), STMT_BREAK()), EXPR_LITERAL(1)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("two")), STMT_CONTINUE()), EXPR_LITERAL(2)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("three")), STMT_BREAK()), EXPR_LITERAL(3)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("four")), STMT_BREAK()), EXPR_LITERAL(4)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("five")), STMT_CONTINUE()), EXPR_LITERAL(5))
        )
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("one\n"
            "two\nthree\n"
            "three\n"
            "four\n"
            "five\nother\none\n"
            "other\none\n"
            "other\none\n"
            "other\none\n"
            "other\none\n",
            vm.logger.logged.str());
}

TEST(TestVM, SwitchCaseMultiple) {
  egg::test::VM vm;
  auto builder = vm->createProgramBuilder();
  builder->addStatement(
    // var i;
    STMT_VAR_DECLARE("i",
      // for (...)
      STMT_FOR(
        // i = 1;
        STMT_VAR_SET("i", EXPR_LITERAL(1)),
        // i < 10;
        EXPR_BINARY(LessThan, EXPR_VAR("i"), EXPR_LITERAL(10)),
        // ++i;
        STMT_VAR_MUTATE("i", Increment, EXPR_LITERAL(VOID)),
        // switch (i) with default
        STMT_SWITCH(EXPR_VAR("i"), 2,
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("odd")), STMT_BREAK()), EXPR_LITERAL(1), EXPR_LITERAL(3), EXPR_LITERAL(5), EXPR_LITERAL(7), EXPR_LITERAL(9)),
          STMT_CASE(STMT_BLOCK(STMT_PRINT(EXPR_LITERAL("even")), STMT_BREAK()))
        )
      )
    )
  );
  buildAndRunSucceeded(vm, *builder);
  ASSERT_EQ("odd\neven\nodd\neven\nodd\neven\nodd\neven\nodd\n", vm.logger.logged.str());
}
