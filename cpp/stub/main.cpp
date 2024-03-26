#include "ovum/ovum.h"
#include "ovum/version.h"

#include <iostream>
#include <cctype>

namespace {
  struct LessCaseInsensitive {
    bool operator()(const unsigned char& lhs, const unsigned char& rhs) const {
      return std::tolower(lhs) < std::tolower(rhs);
    }
    bool operator()(const std::string& lhs, const std::string& rhs) const {
      return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), *this);
    }
  };

  class Application {
  private:
    std::vector<std::string> argv;
    std::map<std::string, std::string, LessCaseInsensitive> envp;
  public:
    Application(int argc, char* argv[], char* envp[]) {
      for (auto argi = 0; argi < argc; ++argi) {
        this->argv.push_back((argv[argi] == nullptr) ? std::string() : argv[argi]);
      }
      for (auto envi = 0; envp[envi] != nullptr; ++envi) {
        auto equals = std::strchr(envp[envi], '=');
        if (equals == nullptr) {
          this->envp[envp[envi]] = {};
        } else {
          this->envp[std::string(envp[envi], equals)] = equals + 1;
        }
      }
    }
    int main() {
      if ((argv.size() == 2) && (argv[1] == "version")) {
        return this->cmdVersion();
      }
      this->cmdVersion();
      for (auto& arg : this->argv) {
        std::cout << arg << std::endl;
      }
      for (auto& env : this->envp) {
        std::cout << env.first << " = " << env.second << std::endl;
      }
      return 0;
    }
  private:
    int cmdVersion() {
      std::cout << egg::ovum::Version() << std::endl;
      return 0;
    }
  };
}

int main(int argc, char* argv[], char* envp[]) {
  Application application(argc, argv, envp);
  return application.main();
}
