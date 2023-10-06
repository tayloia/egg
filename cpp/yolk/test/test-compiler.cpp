#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-compiler.h"

TEST(TestCompiler, ExampleFile) {
  egg::test::VM vm;
  std::string path = "~/cpp/yolk/test/data/example.egg";
  auto lexer = egg::yolk::LexerFactory::createFromPath(path);
  auto tokenizer = egg::yolk::EggTokenizerFactory::createFromLexer(vm->getAllocator(), lexer);
  auto compiler = egg::yolk::EggCompilerFactory::createFromTokenizer(*vm, tokenizer);
  ASSERT_TRUE(compiler->compile());
  ASSERT_STRING(path.c_str(), compiler->resource());
}
