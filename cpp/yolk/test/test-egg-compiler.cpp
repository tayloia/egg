#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"

TEST(TestEggCompiler, Test0001) {
  egg::test::VM vm;
  std::string path = "~/cpp/yolk/test/scripts/test-0001.egg";
  auto lexer = egg::yolk::LexerFactory::createFromPath(path);
  auto tokenizer = egg::yolk::EggTokenizerFactory::createFromLexer(vm->getAllocator(), lexer);
  auto parser = egg::yolk::EggParserFactory::createFromTokenizer(vm->getAllocator(), tokenizer);
  auto compiler = egg::yolk::EggCompilerFactory::createFromParser(*vm, parser);
  ASSERT_TRUE(compiler->compile());
  ASSERT_STRING(path.c_str(), compiler->resource());
}
