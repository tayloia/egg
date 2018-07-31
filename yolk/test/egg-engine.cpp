#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"

using namespace egg::yolk;

namespace {
  class TestLogger : public IEggEngineLogger {
  public:
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) override {
      auto text = TestLogger::logSourceToString(source) + ":" + TestLogger::logSeverityToString(severity) + ":" + message;
      std::cout << text << std::endl;
      this->logged += text + "\n";
    }
    std::string logged;

    // Helpers
    static std::string logSourceToString(egg::lang::LogSource source) {
      static const egg::yolk::String::StringFromEnum table[] = {
        { int(egg::lang::LogSource::Compiler), "COMPILER" },
        { int(egg::lang::LogSource::Runtime), "RUNTIME" },
        { int(egg::lang::LogSource::User), "USER" },
      };
      return String::fromEnum(source, table);
    }
    static std::string logSeverityToString(egg::lang::LogSeverity severity) {
      // Ignore LogSeverity::None
      static const egg::yolk::String::StringFromEnum table[] = {
        { int(egg::lang::LogSeverity::Debug), "DEBUG" },
        { int(egg::lang::LogSeverity::Verbose), "VERBOSE" },
        { int(egg::lang::LogSeverity::Information), "INFO" },
        { int(egg::lang::LogSeverity::Warning), "WARN" },
        { int(egg::lang::LogSeverity::Error), "ERROR" }
      };
      return String::fromEnum(severity, table);
    }
  };

  std::string logFromEngine(TextStream& stream) {
    auto root = EggParserFactory::parseModule(stream);
    auto engine = EggEngineFactory::createEngineFromParsed(root);
    auto logger = std::make_shared<TestLogger>();
    auto preparation = EggEngineFactory::createPreparationContext(logger);
    if (engine->prepare(*preparation) != egg::lang::LogSeverity::Error) {
      auto execution = EggEngineFactory::createExecutionContext(logger);
      engine->execute(*execution);
    }
    return logger->logged;
  }
}

TEST(TestEggEngine, CreateEngineFromParsed) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto root = EggParserFactory::parseModule(stream);
  auto engine = EggEngineFactory::createEngineFromParsed(root);
  auto logger = std::make_shared<TestLogger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Error, engine->prepare(*preparation));
  ASSERT_STARTSWITH(logger->logged, "COMPILER:ERROR:~/cpp/test/data/example.egg(2,14): Unknown identifier: 'first'");
}

TEST(TestEggEngine, CreateEngineFromTextStream) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<TestLogger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Error, engine->prepare(*preparation));
  ASSERT_STARTSWITH(logger->logged, "COMPILER:ERROR:~/cpp/test/data/example.egg(2,14): Unknown identifier: 'first'");
}

TEST(TestEggEngine, CreateEngineFromGarbage) {
  StringTextStream stream("$");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<TestLogger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Error, engine->prepare(*preparation));
  ASSERT_EQ("COMPILER:ERROR:(1, 1): Unexpected character: '$'\n", logger->logged);
}

TEST(TestEggEngine, PrepareTwice) {
  StringTextStream stream("print(123);");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<TestLogger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged);
  ASSERT_EQ(egg::lang::LogSeverity::Error, engine->prepare(*preparation));
  ASSERT_EQ("COMPILER:ERROR:Program prepared more than once\n", logger->logged);
}

TEST(TestEggEngine, ExecuteUnprepared) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<TestLogger>();
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Error, engine->execute(*execution));
  ASSERT_EQ("RUNTIME:ERROR:Program not prepared before execution\n", logger->logged);
}

TEST(TestEggEngine, LogFromEngine) {
  StringTextStream stream("print(`hello`, 123);");
  ASSERT_EQ("USER:INFO:hello123\n", logFromEngine(stream));
}

TEST(TestEggEngine, DuplicateSymbols) {
  StringTextStream stream("var a = 1;\nvar a;");
  ASSERT_STARTSWITH(logFromEngine(stream), "COMPILER:ERROR:(2,5): Duplicate symbol declared at module level: 'a'\n"
                                           "COMPILER:INFO:(1,5): Previous declaration was here\n");
}

TEST(TestEggEngine, WorkingFile) {
  // TODO still needed?
  FileTextStream stream("~/cpp/test/data/working.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<TestLogger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged);
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Information, engine->execute(*execution));
  ASSERT_EQ("USER:INFO:55\nUSER:INFO:4950\n", logger->logged);
}

TEST(TestEggEngine, Coverage) {
  // This script is used to cover most language feature
  FileTextStream stream("~/cpp/test/data/coverage.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<TestLogger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged);
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->execute(*execution));
  ASSERT_EQ("", logger->logged);
}
