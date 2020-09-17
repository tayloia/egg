#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"

using namespace egg::yolk;

namespace {
  std::string logFromEngine(TextStream& stream) {
    egg::test::Allocator allocator;
    auto engine = EggEngineFactory::createEngineFromTextStream(stream);
    auto logger = std::make_shared<egg::test::Logger>();
    auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
    if (engine->prepare(*preparation) != egg::ovum::ILogger::Severity::Error) {
      auto execution = EggEngineFactory::createExecutionContext(allocator, logger);
      engine->execute(*execution);
    }
    return logger->logged.str();
  }
}

TEST(TestEggEngine, CreateEngineFromParsed) {
  egg::test::Allocator allocator;
  FileTextStream stream("~/yolk/test/data/example.egg");
  auto root = EggParserFactory::parseModule(allocator, stream);
  auto engine = EggEngineFactory::createEngineFromParsed(allocator, "<parsed>", root);
  auto logger = std::make_shared<egg::test::Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->prepare(*preparation));
  ASSERT_STARTSWITH(logger->logged.str(), "<COMPILER><ERROR>~/yolk/test/data/example.egg(2,14): Unknown identifier: 'first'");
}

TEST(TestEggEngine, CreateEngineFromTextStream) {
  egg::test::Allocator allocator;
  FileTextStream stream("~/yolk/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<egg::test::Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->prepare(*preparation));
  ASSERT_STARTSWITH(logger->logged.str(), "<COMPILER><ERROR>~/yolk/test/data/example.egg(2,14): Unknown identifier: 'first'");
}

TEST(TestEggEngine, CreateEngineFromGarbage) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  StringTextStream stream("$");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<egg::test::Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->prepare(*preparation));
  ASSERT_EQ("<COMPILER><ERROR>(1, 1): Unexpected character: '$'\n", logger->logged.str());
}

TEST(TestEggEngine, PrepareTwice) {
  egg::test::Allocator allocator;
  StringTextStream stream("print(123);");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<egg::test::Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged.str());
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->prepare(*preparation));
  ASSERT_EQ("<COMPILER><ERROR>Program prepared more than once\n", logger->logged.str());
}

TEST(TestEggEngine, ExecuteUnprepared) {
  egg::test::Allocator allocator{ egg::test::Allocator::Expectation::NoAllocations };
  FileTextStream stream("~/yolk/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<egg::test::Logger>();
  auto execution = EggEngineFactory::createExecutionContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Error, engine->execute(*execution));
  ASSERT_EQ("<RUNTIME><ERROR>Program not prepared before execution\n", logger->logged.str());
}

TEST(TestEggEngine, LogFromEngine) {
  StringTextStream stream("print(`hello`, 123);");
  ASSERT_EQ("hello123\n", logFromEngine(stream));
}

TEST(TestEggEngine, DuplicateSymbols) {
  StringTextStream stream("var a = 1;\nvar a;");
  ASSERT_STARTSWITH(logFromEngine(stream), "<COMPILER><ERROR>(2,5): Duplicate symbol declared at module level: 'a'\n"
                                           "<COMPILER>(1,5): Previous declaration was here\n");
}

TEST(TestEggEngine, WorkingFile) {
  // TODO still needed?
  egg::test::Allocator allocator;
  FileTextStream stream("~/yolk/test/data/working.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<egg::test::Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged.str());
  auto execution = EggEngineFactory::createExecutionContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::Information, engine->execute(*execution));
  ASSERT_EQ("55\n4950\n", logger->logged.str());
}

TEST(TestEggEngine, Coverage) {
  // This script is used to cover most language feature
  egg::test::Allocator allocator;
  FileTextStream stream("~/yolk/test/data/coverage.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<egg::test::Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged.str());
  auto execution = EggEngineFactory::createExecutionContext(allocator, logger);
  ASSERT_EQ(egg::ovum::ILogger::Severity::None, engine->execute(*execution));
  ASSERT_EQ("", logger->logged.str());
}
