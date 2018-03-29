#include "test.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"

using namespace egg::yolk;

namespace {
  class Logger : public IEggEngineLogger {
  public:
    virtual ~Logger() {
    }
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) {
      auto text = Logger::logSourceToString(source) + ":" + Logger::logSeverityToString(severity) + ":" + message;
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
    std::string logSeverityToString(egg::lang::LogSeverity severity) {
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
    auto logger = std::make_shared<Logger>();
    auto execution = EggEngineFactory::createExecutionContext(logger);
    engine->execute(*execution);
    return logger->logged;
  }
}

TEST(TestEggEngine, CreateEngineFromParsed) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto root = EggParserFactory::parseModule(stream);
  auto engine = EggEngineFactory::createEngineFromParsed(root);
  auto logger = std::make_shared<Logger>();
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->execute(*execution));
  ASSERT_STARTSWITH(logger->logged, "USER:INFO:");
}

TEST(TestEggEngine, CreateEngineFromTextStream) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged);
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->execute(*execution));
  ASSERT_STARTSWITH(logger->logged, "USER:INFO:");
}

TEST(TestEggEngine, CreateEngineFromGarbage) {
  StringTextStream stream("$");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Error, engine->prepare(*preparation));
  ASSERT_EQ("COMPILER:ERROR:(1, 1): Unexpected character: '$'\n", logger->logged);
}

TEST(TestEggEngine, PrepareTwice) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged);
  ASSERT_EQ(egg::lang::LogSeverity::Error, engine->prepare(*preparation));
  ASSERT_EQ("COMPILER:ERROR:Program prepared more than once\n", logger->logged);
}

TEST(TestEggEngine, ExecuteUnprepared) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<Logger>();
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Error, engine->execute(*execution));
  ASSERT_EQ("RUNTIME:ERROR:Program not prepared before execution\n", logger->logged);
}

TEST(TestEggEngine, LogFromEngine) {
  StringTextStream stream("");
  ASSERT_EQ("USER:INFO:execute\n", logFromEngine(stream));
}

TEST(TestEggEngine, WorkingFile) {
  FileTextStream stream("~/cpp/test/data/working.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged);
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->execute(*execution));
  ASSERT_EQ("USER:INFO:execute\n", logger->logged);
  //ASSERT_STARTSWITH(logger->logged, "USER:INFO:");
}
