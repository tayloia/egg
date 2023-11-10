#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-compiler.h"

TEST(TestEggRunner, Succeeded) {
  egg::test::VM vm;
  auto program = egg::yolk::EggCompilerFactory::compileFromPath(*vm, "~/cpp/yolk/test/scripts/test-0001.egg");
  ASSERT_TRUE(program != nullptr);
  auto runner = program->createRunner();
  ASSERT_TRUE(runner != nullptr);
  vm.addBuiltins(*runner);
  ASSERT_TRUE(vm.run(*runner));
  ASSERT_EQ("Hello, World!\n", vm.logger.logged.str());
}

TEST(TestEggRunner, Failed) {
  egg::test::VM vm;
  auto program = egg::yolk::EggCompilerFactory::compileFromPath(*vm, "~/cpp/yolk/test/scripts/test-0001.egg");
  ASSERT_TRUE(program != nullptr);
  auto runner = program->createRunner();
  ASSERT_TRUE(runner != nullptr);
  // Deliberately fail to call vm.addBuiltins(*runner);
  ASSERT_FALSE(vm.run(*runner));
  ASSERT_EQ("<RUNTIME><ERROR>throw Unknown variable symbol: 'print'\n", vm.logger.logged.str());
}
