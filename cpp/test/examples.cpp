#include "test.h"
#include "lexers.h"
#include "egg-tokenizer.h"
#include "egg-syntax.h"
#include "egg-parser.h"
#include "egg-engine.h"

using namespace egg::yolk;

namespace {
  class TestLogger : public IEggEngineLogger {
  private:
    std::string resource;
  public:
    explicit TestLogger(const std::string& resource)
      : resource(resource) {
    }
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) override {
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
      auto text = String::fromEnum(source, sourceTable) + String::fromEnum(severity, severityTable);
      if (this->resource.empty()) {
        text += message;
      } else {
        text += String::replace(message, this->resource, "<RESOURCE>");
      }
      std::cout << text << std::endl;
      this->logged += text + "\n";
    }
    std::string logged;
  };

  class TestExamples : public ::testing::TestWithParam<int> {
    EGG_NO_COPY(TestExamples);
  public:
    TestExamples() {}
    void run() {
      // Actually perform the testing
      int example = this->GetParam();
      auto resource = "~/examples/example-" + TestExamples::formatIndex(example) + ".egg";
      FileTextStream stream(resource);
      auto actual = execute(stream);
      ASSERT_TRUE(stream.rewind());
      auto expected = expectation(stream);
      ASSERT_EQ(expected, actual);
    }
    static ::testing::internal::ParamGenerator<int> generator() {
      // Generate value parameterizations for all the examples
      return ::testing::ValuesIn(TestExamples::find());
    }
    static std::string name(const ::testing::TestParamInfo<int>& info) {
      // Format the test case name parameterization nicely
      return TestExamples::formatIndex(info.param);
    }
  private:
    static std::string execute(TextStream& stream) {
      auto root = EggParserFactory::parseModule(stream);
      auto engine = EggEngineFactory::createEngineFromParsed(root);
      auto logger = std::make_shared<TestLogger>(stream.getResourceName());
      auto preparation = EggEngineFactory::createPreparationContext(logger);
      if (engine->prepare(*preparation) != egg::lang::LogSeverity::Error) {
        auto execution = EggEngineFactory::createExecutionContext(logger);
        engine->execute(*execution);
      }
      return logger->logged;
    }
    static std::string expectation(TextStream& stream) {
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
    static std::vector<int> find() {
      // Find all the examples
      auto files = File::readDirectory("~/examples");
      std::vector<int> results;
      for (auto& file : files) {
        auto index = TestExamples::extractIndex(file);
        if (index >= 0) {
          results.push_back(index);
        }
      }
      if (results.empty()) {
        // Push a dummy entry so that problems with example discovery don't just skip all the tests
        results.push_back(0);
      }
      return results;
    }
    static std::string formatIndex(int index) {
      // Format the example index nicely
      char buffer[32];
      std::snprintf(buffer, EGG_NELEMS(buffer), "%04d", index);
      return buffer;
    }
    static int extractIndex(const std::string& text) {
      // Look for "example-####.egg"
      size_t pos = 8;
      if (text.substr(0, pos) == "example-") {
        int index = 0;
        while (std::isdigit(text[pos])) {
          index = (index * 10) + text[pos] - '0';
          pos++;
        }
        if ((pos > 8) && (text.substr(pos) == ".egg")) {
          return index;
        }
      }
      return -1; // no match
    }
  };
}

TEST_P(TestExamples, Run) {
  this->run();
}

EGG_INSTANTIATE_TEST_CASE_P(TestExamples)
