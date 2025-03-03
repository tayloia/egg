#include "ovum/test.h"
#include "ovum/lexer.h"
#include "ovum/egg-tokenizer.h"
#include "ovum/egg-parser.h"
#include "ovum/egg-compiler.h"

TEST(TestEggCompiler, ExplicitSteps) {
  egg::test::VM vm;
  auto lexer = egg::ovum::LexerFactory::createFromPath(egg::test::resolvePath("cpp/yolk/test/scripts/test-0001.egg"));
  auto tokenizer = egg::ovum::EggTokenizerFactory::createFromLexer(vm->getAllocator(), lexer);
  auto parser = egg::ovum::EggParserFactory::createFromTokenizer(vm->getAllocator(), tokenizer);
  auto pbuilder = vm->createProgramBuilder();
  pbuilder->addBuiltin(vm->createString("print"), egg::ovum::Type::Object); // TODO
  auto compiler = egg::ovum::EggCompilerFactory::createFromProgramBuilder(pbuilder);
  auto module = compiler->compile(*parser);
  ASSERT_TRUE(module != nullptr);
  auto program = pbuilder->build();
  ASSERT_TRUE(program != nullptr);
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestEggCompiler, Success) {
  egg::test::VM vm;
  auto program = egg::ovum::EggCompilerFactory::compileFromPath(*vm, egg::test::resolvePath("cpp/yolk/test/scripts/test-0001.egg"));
  ASSERT_TRUE(program != nullptr);
  ASSERT_EQ(1u, program->getModuleCount());
  ASSERT_NE(nullptr, program->getModule(0));
  ASSERT_EQ(nullptr, program->getModule(1));
  ASSERT_EQ("", vm.logger.logged.str());
}

TEST(TestEggCompiler, Failure) {
  egg::test::VM vm;
  auto program = egg::ovum::EggCompilerFactory::compileFromText(*vm, "print($$$);");
  ASSERT_TRUE(program == nullptr);
  ASSERT_EQ("<COMPILER><ERROR>(1,7): Unexpected character: '$'\n", vm.logger.logged.str());
}
