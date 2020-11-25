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
    TestExamples() {}
    void run() {
      // Actually perform the testing
      int example = this->GetParam();
      auto resource = "~/examples/example-" + TestExamples::formatIndex(example) + ".egg";
      FileTextStream stream(resource);
      auto actual = TestExamples::execute(stream);
      ASSERT_TRUE(stream.rewind());
      auto expected = TestExamples::expectation(stream);
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
      egg::test::Allocator allocator;
      auto logger = std::make_shared<egg::test::Logger>(stream.getResourceName());
      auto context = EggEngineFactory::createContext(allocator, logger);
      auto engine = EggEngineFactory::createEngineFromTextStream(stream);
      (void)engine->run(*context);
      return logger->logged.str();
    }
    static std::string expectation(TextStream& stream) {
      std::string expected;
      std::string line;
      while (stream.readline(line)) {
        // Example output lines always begin with '///'
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
    static bool isDisabled(int index) {
      return index > 25;
    }
    static bool isDisabledWOBBLE(int index) {
      static const int disabled[] = { 25,26,27,28,29,30,35,36,40,41,42,43,44,45,46,47,48,49,50,51 };
      auto* p = disabled;
      auto* q = p + EGG_NELEMS(disabled);
      return std::find(p, q, index) != q;
    }
    static std::string formatIndex(int index) {
      // Format the example index nicely
      char buffer[32];
      std::snprintf(buffer, EGG_NELEMS(buffer), "%s%04d", TestExamples::isDisabled(index) ? "DISABLED_" : "", index);
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
