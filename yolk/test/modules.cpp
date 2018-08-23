#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"

#include "ovum/node.h"
#include "ovum/module.h"

using namespace egg::yolk;

namespace {
  class TestLogger : public egg::ovum::ILogger {
    TestLogger(const TestLogger&) = delete;
    TestLogger operator=(const TestLogger&) = delete;
  public:
    std::stringstream logged;
    TestLogger() = default;
    virtual void log(Source source, Severity severity, const std::string& message) override {
      auto text = TestLogger::logSourceToString(source) + ":" + TestLogger::logSeverityToString(severity) + ":" + message;
      std::cout << text << std::endl;
      this->logged << text << '\n';
    }

    // Helpers
    static std::string logSourceToString(Source source) {
      static const egg::yolk::String::StringFromEnum table[] = {
        { int(Source::Compiler), "COMPILER" },
        { int(Source::Runtime), "RUNTIME" },
        { int(Source::User), "USER" },
      };
      return String::fromEnum(source, table);
    }
    static std::string logSeverityToString(Severity severity) {
      // Ignore LogSeverity::None
      static const egg::yolk::String::StringFromEnum table[] = {
        { int(Severity::Debug), "DEBUG" },
        { int(Severity::Verbose), "VERBOSE" },
        { int(Severity::Information), "INFO" },
        { int(Severity::Warning), "WARN" },
        { int(Severity::Error), "ERROR" }
      };
      return String::fromEnum(severity, table);
    }
  };

  void hexdump(std::ostream& stream, const egg::ovum::Memory& memory) {
    static const char hex[] = "0123456789ABCDEF";
    if (memory != nullptr) {
      auto* p = memory->begin();
      auto* q = memory->end();
      if (p < q) {
        stream << hex[*p >> 4] << hex[*p & 0xF];
        p++;
      }
      while (p < q) {
        stream << ' ' << hex[*p >> 4] << hex[*p & 0xF];
        p++;
      }
    }
  }

  std::string compile(TextStream& stream) {
    egg::test::Allocator allocator;
    auto root = EggParserFactory::parseModule(allocator, stream);
    auto engine = EggEngineFactory::createEngineFromParsed(allocator, root);
    auto logger = std::make_shared<TestLogger>();
    auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
    if (engine->prepare(*preparation) != egg::ovum::ILogger::Severity::Error) {
      auto compilation = EggEngineFactory::createCompilationContext(allocator, logger);
      egg::ovum::Module module;
      if (engine->compile(*compilation, module) != egg::ovum::ILogger::Severity::Error) {
        hexdump(logger->logged, egg::ovum::ModuleFactory::toMemory(allocator));
      }
    }
    return logger->logged.str();
  }
}

TEST(TestModules, Minimal) {
  StringTextStream stream("");
  auto retval = compile(stream);
  ASSERT_EQ("COMPILER:WARN:WIBBLE\n57 49 42 42 4C 45", retval);
}