#include "yolk/test.h"
#include "ovum/lexer.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"

TEST(TestEggCompiler, ExplicitSteps) {
  egg::test::VM vm;
  auto lexer = egg::ovum::LexerFactory::createFromPath("~/cpp/yolk/test/scripts/test-0001.egg");
  auto tokenizer = egg::yolk::EggTokenizerFactory::createFromLexer(vm->getAllocator(), lexer);
  auto parser = egg::yolk::EggParserFactory::createFromTokenizer(vm->getAllocator(), tokenizer);
  auto pbuilder = vm->createProgramBuilder();
  pbuilder->addBuiltin(vm->createString("print"), egg::ovum::Type::Object); // TODO
  auto compiler = egg::yolk::EggCompilerFactory::createFromProgramBuilder(pbuilder);
  auto module = compiler->compile(*parser);
  ASSERT_TRUE(module != nullptr);
  auto program = pbuilder->build();
  ASSERT_TRUE(program != nullptr);
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestEggCompiler, Success) {
  egg::test::VM vm;
  auto program = egg::yolk::EggCompilerFactory::compileFromPath(*vm, "~/cpp/yolk/test/scripts/test-0001.egg");
  ASSERT_TRUE(program != nullptr);
  ASSERT_EQ(1u, program->getModuleCount());
  ASSERT_NE(nullptr, program->getModule(0));
  ASSERT_EQ(nullptr, program->getModule(1));
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestEggCompiler, Failure) {
  egg::test::VM vm;
  auto program = egg::yolk::EggCompilerFactory::compileFromText(*vm, "print($$$);");
  ASSERT_TRUE(program == nullptr);
  ASSERT_EQ("<COMPILER><ERROR>(1,7): Unexpected character: '$'\n", vm.logger.logged.str());
}
