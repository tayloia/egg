#include "ovum/test.h"
#include "ovum/exception.h"
#include "ovum/lexer.h"
#include "ovum/egg-tokenizer.h"
#include "ovum/egg-parser.h"
#include "ovum/egg-compiler.h"

TEST(TestEggRunner, Succeeded) {
  egg::test::VM vm;
  auto program = egg::ovum::EggCompilerFactory::compileFromText(*vm, "print(\"Hello, World!\");");
  ASSERT_TRUE(program != nullptr);
  auto runner = program->createRunner();
  ASSERT_TRUE(runner != nullptr);
  vm.addBuiltins(*runner);
  ASSERT_TRUE(vm.run(*runner));
  ASSERT_EQ("Hello, World!\n", vm.logger.logged.str());
}

TEST(TestEggRunner, Failed) {
  egg::test::VM vm;
  auto program = egg::ovum::EggCompilerFactory::compileFromText(*vm, "// comment\n  print(\"Hello, World!\");", "greeting.egg");
  ASSERT_TRUE(program != nullptr);
  auto runner = program->createRunner();
  ASSERT_TRUE(runner != nullptr);
  // Deliberately fail to call vm.addBuiltins(*runner);
  ASSERT_FALSE(vm.run(*runner));
  ASSERT_EQ("<RUNTIME><ERROR>greeting.egg(2,3-7): Unknown identifier: 'print'\n", vm.logger.logged.str());
}
