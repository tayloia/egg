#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"
#include "yolk/egg-module.h"

TEST(TestEggCompiler, ExplicitSteps) {
  egg::test::VM vm;
  auto lexer = egg::yolk::LexerFactory::createFromPath("~/cpp/yolk/test/scripts/test-0001.egg");
  auto tokenizer = egg::yolk::EggTokenizerFactory::createFromLexer(vm->getAllocator(), lexer);
  auto parser = egg::yolk::EggParserFactory::createFromTokenizer(vm->getAllocator(), tokenizer);
  auto pbuilder = vm->createProgramBuilder();
  auto compiler = egg::yolk::EggCompilerFactory::createFromProgramBuilder(pbuilder);
  auto module = compiler->compile(*parser);
  ASSERT_TRUE(module != nullptr);
}

TEST(TestEggCompiler, Helper) {
  egg::test::VM vm;
  auto module = egg::yolk::EggModuleFactory::createFromPath(*vm, "~/cpp/yolk/test/scripts/test-0001.egg");
  ASSERT_TRUE(module != nullptr);
}
