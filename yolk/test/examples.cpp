#include "yolk/test.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"

using namespace egg::yolk;

namespace {
  class TestExamples : public ::testing::TestWithParam<int> {
    EGG_NO_COPY(TestExamples);
  public:
    enum class Age { Old, New };
    TestExamples() {}
    void run(Age age) {
      // Actually perform the testing
      int example = this->GetParam();
      auto resource = "~/examples/example-" + TestExamples::formatIndex(example) + ".egg";
      FileTextStream stream(resource);
      auto actual = TestExamples::execute(stream, age);
      ASSERT_TRUE(stream.rewind());
      auto expected = TestExamples::expectation(stream, age);
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
    static std::string execute(TextStream& stream, Age age) {
      egg::test::Allocator allocator;
      auto logger = std::make_shared<egg::test::Logger>(stream.getResourceName());
      switch (age) {
      case Age::Old:
        TestExamples::executeOld(stream, allocator, logger);
        break;
      case Age::New:
        TestExamples::executeNew(stream, allocator, logger);
        break;
      }
      return logger->logged.str();
    }
    static void executeOld(TextStream& stream, egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger) {
      auto engine = EggEngineFactory::createEngineFromTextStream(stream);
      auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
      if (engine->prepare(*preparation) != egg::ovum::ILogger::Severity::Error) {
        auto execution = EggEngineFactory::createExecutionContext(allocator, logger);
        engine->execute(*execution);
      }
    }
    static void executeNew(TextStream& stream, egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger) {
      auto engine = EggEngineFactory::createEngineFromTextStream(stream);
      auto preparation = EggEngineFactory::createPreparationContext(allocator, logger);
      if (engine->prepare(*preparation) != egg::ovum::ILogger::Severity::Error) {
        auto compilation = EggEngineFactory::createCompilationContext(allocator, logger);
        egg::ovum::Module module;
        if (engine->compile(*compilation, module) != egg::ovum::ILogger::Severity::Error) {
          auto program = egg::ovum::ProgramFactory::createProgram(allocator, *logger);
          auto result = program->run(*module);
          if (result.stripFlowControl(egg::ovum::ValueFlags::Throw)) {
            if (!result.isVoid()) {
              // Don't re-print a rethrown exception
              logger->log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, result.toString().toUTF8());
            }
          } else if (!result.isVoid()) {
            // We expect 'void' here
            auto message = "Internal runtime error: Expected statement to return 'void', but got '" + result.getRuntimeType().toString().toUTF8() + "' instead";
            logger->log(egg::ovum::ILogger::Source::Runtime, egg::ovum::ILogger::Severity::Error, message);
          }
        }
      }
    }
    static std::string expectation(TextStream& stream, Age age) {
      std::string expected;
      std::string line;
      while (stream.readline(line)) {
        // Example output lines always begin with '///'
        if ((line.length() > 3) && (line[0] == '/') && (line[1] == '/') && (line[2] == '/')) {
          switch (age) {
          case Age::Old:
            if (String::startsWith(line, "///OLD")) {
              line = line.substr(3);
            }
            break;
          case Age::New:
            if (String::startsWith(line, "///NEW")) {
              line = line.substr(3);
            }
            break;
          }
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

TEST_P(TestExamples, RunOld) {
  this->run(Age::Old);
}

TEST_P(TestExamples, RunNew) {
  this->run(Age::New);
}

EGG_INSTANTIATE_TEST_CASE_P(TestExamples)
