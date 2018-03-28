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
    virtual void log(LogSource source, LogSeverity severity, const std::string& message) {
      auto text = Logger::logSourceToString(source) + ":" + Logger::logSeverityToString(severity) + ":" + message;
      std::cout << text << std::endl;
      this->logged += text + "\n";
    }
    std::string logged;

    // Helpers
    static std::string logSourceToString(LogSource source) {
      static const egg::yolk::String::StringFromEnum table[] = {
        { int(LogSource::Compiler), "COMPILER" },
        { int(LogSource::Runtime), "RUNTIME" },
        { int(LogSource::User), "USER" },
      };
      return String::fromEnum(source, table);
    }
    std::string logSeverityToString(LogSeverity severity) {
      // Ignore LogSeverity::None
      static const egg::yolk::String::StringFromEnum table[] = {
        { int(LogSeverity::Debug), "DEBUG" },
        { int(LogSeverity::Verbose), "VERBOSE" },
        { int(LogSeverity::Information), "INFO" },
        { int(LogSeverity::Warning), "WARN" },
        { int(LogSeverity::Error), "ERROR" }
      };
      return String::fromEnum(severity, table);
    }
  };
}

TEST(TestEggEngine, CreateEngineFromParsed) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto root = EggParserFactory::parseModule(stream);
  auto engine = EggEngineFactory::createEngineFromParsed(root);
  auto logger = std::make_shared<Logger>();
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Information, engine->execute(*execution));
  ASSERT_EQ("USER:INFO:execute\n", logger->logged);
}

TEST(TestEggEngine, CreateEngineFromTextStream) {
  FileTextStream stream("~/cpp/test/data/example.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged);
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Information, engine->execute(*execution));
  ASSERT_EQ("USER:INFO:execute\n", logger->logged);
}

TEST(TestEggEngine, ExecuteDoublyPrepared) {
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

TEST(TestEggEngine, WorkingFile) {
  FileTextStream stream("~/cpp/test/data/working.egg");
  auto engine = EggEngineFactory::createEngineFromTextStream(stream);
  auto logger = std::make_shared<Logger>();
  auto preparation = EggEngineFactory::createPreparationContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::None, engine->prepare(*preparation));
  ASSERT_EQ("", logger->logged);
  auto execution = EggEngineFactory::createExecutionContext(logger);
  ASSERT_EQ(egg::lang::LogSeverity::Information, engine->execute(*execution));
  ASSERT_EQ("USER:INFO:execute\n", logger->logged);
}
