#include "yolk/test.h"
#include "yolk/egg-compiler.h"

using namespace egg::yolk;

namespace {
  class TestScripts : public ::testing::TestWithParam<std::string> {
  private:
    static const std::string directory;
    static const size_t lbound = 1;
    static const size_t ubound = 13; // Set to zero to directory search
  public:
    void run() {
      // Actually perform the testing
      std::string script = this->GetParam();
      auto resource = TestScripts::directory + "/" + script;
      FileTextStream stream(resource);
      auto actual = TestScripts::execute(stream);
      ASSERT_TRUE(stream.rewind());
      auto expected = TestScripts::expectation(stream);
      ASSERT_EQ(expected, actual);
    }
    static ::testing::internal::ParamGenerator<std::string> generator() {
      // Generate value parameterizations for all the scripts
      return ::testing::ValuesIn((TestScripts::lbound <= TestScripts::ubound) ? TestScripts::list() : TestScripts::find());
    }
    static std::string name(const ::testing::TestParamInfo<std::string>& info) {
      // Format the test case name parameterization nicely
      std::string name = info.param;
      if (name.starts_with("test-")) {
        name = name.substr(5);
      }
      if (name.ends_with(".egg")) {
        name = name.substr(0, name.size() - 4);
      }
      for (auto& ch : name) {
        if (!std::isalnum(ch)) {
          ch = '_';
        }
      }
      return name;
    }
  private:
    static std::string execute(TextStream& stream) {
      egg::test::VM vm;
      vm.logger.resource = stream.getResourceName();
      auto program = EggCompilerFactory::compileFromStream(*vm, stream);
      if (program != nullptr) {
        auto runner = program->createRunner();
        vm.addBuiltins(*runner);
        vm.run(*runner);
      }
      return vm.logger.logged.str();
    }
    static std::string expectation(TextStream& stream) {
      std::string expected;
      std::string line;
      while (stream.readline(line)) {
        // Test output lines always begin with '///'
        if (line.starts_with("///")) {
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
    static std::vector<std::string> find() {
      // Find all the scripts
      auto results = File::readDirectory(TestScripts::directory);
      auto predicate = [](const std::string& path) {
        return File::getKind(TestScripts::directory + "/" + path) != File::Kind::File;
        };
      results.erase(std::remove_if(results.begin(), results.end(), predicate), results.end());
      if (results.empty()) {
        // Push a dummy entry so that problems with script discovery don't just skip all the tests
        results.push_back("missing");
      }
      return results;
    }
    static std::vector<std::string> list() {
      // List all the scripts
      std::vector<std::string> results;
      for (size_t index = TestScripts::lbound; index <= TestScripts::ubound; ++index) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "test-%04zu.egg", index);
        results.emplace_back(buffer);
      }
      return results;
    }
  };

  const std::string TestScripts::directory = "~/cpp/yolk/test/scripts";
}

TEST_P(TestScripts, Run) {
  this->run();
}

EGG_INSTANTIATE_TEST_CASE_P(TestScripts)
