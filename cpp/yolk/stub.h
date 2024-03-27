#include <cctype>

namespace egg::yolk {
  class Stub {
  private:
    struct LessCaseInsensitive {
      bool operator()(const unsigned char& lhs, const unsigned char& rhs) const {
        return std::tolower(lhs) < std::tolower(rhs);
      }
      bool operator()(const std::string& lhs, const std::string& rhs) const {
        return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), *this);
      }
    };
    std::vector<std::string> argv;
    std::map<std::string, std::string, LessCaseInsensitive> envp;
  public:
    Stub(int argc, char* argv[], char* envp[]) {
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
    int main();
  private:
    int cmdVersion();
  };
}
