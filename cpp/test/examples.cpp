#include "test.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"

using namespace egg::yolk;

namespace {
  class TestLogger : public IEggEngineLogger {
  public:
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) {
      static const egg::yolk::String::StringFromEnum sourceTable[] = {
        { int(egg::lang::LogSource::Compiler), "<COMPILER>" },
        { int(egg::lang::LogSource::Runtime), "<RUNTIME>" },
        { int(egg::lang::LogSource::User), "" },
      };
      static const egg::yolk::String::StringFromEnum severityTable[] = {
        { int(egg::lang::LogSeverity::Debug), "<DEBUG>" },
        { int(egg::lang::LogSeverity::Verbose), "<VERBOSE>" },
        { int(egg::lang::LogSeverity::Information), "" },
        { int(egg::lang::LogSeverity::Warning), "<WARN>" },
        { int(egg::lang::LogSeverity::Error), "<ERROR>" }
      };
      auto text = String::fromEnum(source, sourceTable) + String::fromEnum(severity, severityTable) + message;
      std::cout << text << std::endl;
      this->logged += text + "\n";
    }
    std::string logged;
  };

  std::string execute(TextStream& stream) {
    auto root = EggParserFactory::parseModule(stream);
    auto engine = EggEngineFactory::createEngineFromParsed(root);
    auto logger = std::make_shared<TestLogger>();
    auto execution = EggEngineFactory::createExecutionContext(logger);
    engine->execute(*execution);
    return logger->logged;
  }

  std::string expectation(TextStream& stream) {
    std::string expected;
    std::string line;
    while (stream.readline(line)) {
      // Exmaple output lines always begin with '///'
      if ((line.length() > 3) && (line[0] == '/') && (line[1] == '/') && (line[2] == '/')) {
        switch (line[3]) {
        case '>':
          // '///>message' for normal USER/INFO output, e.g. print()
          expected += line.substr(4) + "\n";
          break;
        case '<':
          // '///<SOURCE><SEVERITY>message' for other log output
          expected += line.substr(3) + "\n";
          break;
        }
      }
    }
    return expected;
  }
}

#define TEST_EXAMPLE(nnnn) \
  TEST(TestExamples, Example##nnnn) { \
    FileTextStream stream("~/examples/example-" #nnnn ".egg"); \
    auto actual = execute(stream); \
    ASSERT_TRUE(stream.rewind()); \
    auto expected = expectation(stream); \
    ASSERT_EQ(expected, actual); \
  }

TEST_EXAMPLE(0001)
TEST_EXAMPLE(0002)
TEST_EXAMPLE(0003)
TEST_EXAMPLE(0004)
TEST_EXAMPLE(0005)
TEST_EXAMPLE(0006)
TEST_EXAMPLE(0007)
TEST_EXAMPLE(0008)
TEST_EXAMPLE(0009)
TEST_EXAMPLE(0010)
TEST_EXAMPLE(0011)
TEST_EXAMPLE(0012)
