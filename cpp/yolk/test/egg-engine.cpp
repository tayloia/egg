#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/test/etest.h"

using namespace egg::yolk;

TEST(TestEggEngine, CreateEngineFromParsed) {
  egg::test::Allocator allocator;
  egg::ovum::TypeFactory factory{ allocator };
  FileTextStream stream("~/cpp/yolk/test/data/example.egg");
  auto root = EggParserFactory::parseModule(factory, stream);
  auto engine = EggEngineFactory::createEngineFromParsed(allocator, "<parsed>", root);
  egg::test::EggEngineContextFromFactory context{ factory };
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->prepare(context));
  ASSERT_STARTSWITH(context.logged(), "<COMPILER><ERROR>~/cpp/yolk/test/data/example.egg(2,14): Unknown identifier: 'first'");
}

TEST(TestEggEngine, CreateEngineFromTextStream) {
  egg::test::EggEngineContext context;
  FileTextStream stream("~/cpp/yolk/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->prepare(context));
  ASSERT_STARTSWITH(context.logged(), "<COMPILER><ERROR>~/cpp/yolk/test/data/example.egg(2,14): Unknown identifier: 'first'");
}

TEST(TestEggEngine, CreateEngineFromGarbage) {
  egg::test::EggEngineContext context{ egg::test::Allocator::Expectation::NoAllocations };
  StringTextStream stream("$");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->prepare(context));
  ASSERT_EQ("<COMPILER><ERROR>(1, 1): Unexpected character: '$'\n", context.logged());
}

TEST(TestEggEngine, PrepareTwice) {
  egg::test::EggEngineContext context;
  StringTextStream stream("print(123);");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->prepare(context));
  ASSERT_EQ("", context.logged());
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->prepare(context));
  ASSERT_EQ("<COMPILER><ERROR>Program prepared more than once\n", context.logged());
}

TEST(TestEggEngine, ExecuteUnprepared) {
  egg::test::EggEngineContext context{ egg::test::Allocator::Expectation::NoAllocations };
  FileTextStream stream("~/cpp/yolk/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->execute(context));
  ASSERT_EQ("<RUNTIME><ERROR>Program not prepared before compilation\n", context.logged());
}

TEST(TestEggEngine, LogFromEngine) {
  egg::test::EggEngineContext context;
  StringTextStream stream("print(`hello`, 123);");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->run(context));
  ASSERT_EQ("hello123\n", context.logged());
}

TEST(TestEggEngine, DuplicateSymbols) {
  egg::test::EggEngineContext context;
  StringTextStream stream("var a = 1;\nvar a;");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->run(context));
  ASSERT_STARTSWITH(context.logged(), "<COMPILER><ERROR>(2,5): Duplicate symbol declared at module level: 'a'\n"
                                      "<COMPILER><INFORMATION>(1,5): Previous declaration was here\n");
}

TEST(TestEggEngine, WorkingFile) {
  // TODO still needed?
  egg::test::EggEngineContext context;
  FileTextStream stream("~/cpp/yolk/test/data/working.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->prepare(context));
  ASSERT_EQ("", context.logged());
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->execute(context));
  ASSERT_EQ("55\n4950\n", context.logged());
}

TEST(TestEggEngine, Coverage) {
  // This script is used to cover most language features
  egg::test::EggEngineContext context;
  FileTextStream stream("~/cpp/yolk/test/data/coverage.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->prepare(context));
  ASSERT_EQ("", context.logged());
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->execute(context));
  ASSERT_EQ("", context.logged());
}
