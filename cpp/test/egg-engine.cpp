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
}

TEST(TestEggEngine, WorkingFile) {
  auto parser = EggParserFactory::createModuleParser();
  auto lexer = LexerFactory::createFromPath("~/cpp/test/data/working.egg"); //TODO
  auto tokenizer = EggTokenizerFactory::createFromLexer(lexer);
  auto root = parser->parse(*tokenizer);
  auto engine = EggEngineFactory::createEngineFromParsed(root);
  ASSERT_NE(nullptr, engine);
  auto logger = std::make_shared<Logger>();
  ASSERT_NE(nullptr, logger);
  auto execution = engine->createExecutionFromLogger(logger);
  ASSERT_NE(nullptr, execution);
  engine->execute(*execution);
  ASSERT_EQ("USER:INFO:hello\n", logger->logged);
}
