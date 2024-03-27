#include "yolk/yolk.h"
#include "yolk/stub.h"
#include "ovum/exception.h"
#include "ovum/os-file.h"
#include "ovum/version.h"

#include <cctype>
#include <iostream>

namespace {
  using namespace egg::ovum;

  struct LessCaseInsensitive {
    bool operator()(const unsigned char& lhs, const unsigned char& rhs) const {
      return std::tolower(lhs) < std::tolower(rhs);
    }
    bool operator()(const std::string& lhs, const std::string& rhs) const {
      return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), *this);
    }
  };

  class Stub : public egg::yolk::IStub {
  private:
    std::vector<std::string> arguments;
    std::map<std::string, std::string, LessCaseInsensitive> environment;
    IAllocator* allocator = nullptr;
    ILogger* logger = nullptr;
  public:
    virtual IStub& withArguments(int argc, char* argv[]) override {
      for (auto argi = 0; argi < argc; ++argi) {
        this->arguments.push_back((argv[argi] == nullptr) ? std::string() : argv[argi]);
      }
      return *this;
    }
    virtual void log(Source source, Severity severity, const String& message) override {
      if (this->logger != nullptr) {
        this->logger->log(source, severity, message);
        return;
      }
      std::string buffer;
      switch (source) {
      case Source::Compiler:
        buffer = "<COMPILER>";
        break;
      case Source::Runtime:
        buffer = "<RUNTIME>";
        break;
      case Source::Command:
        buffer = "<COMMAND>";
        break;
      case Source::User:
        break;
      }
      switch (severity) {
      case Severity::None:
        break;
      case Severity::Debug:
        buffer += "<DEBUG>";
        break;
      case Severity::Verbose:
        buffer += "<VERBOSE>";
        break;
      case Severity::Information:
        buffer += "<INFORMATION>";
        break;
      case Severity::Warning:
        buffer += "<WARNING>";
        break;
      case Severity::Error:
        buffer += "<ERROR>";
        break;
      }
      buffer += message.toUTF8();
      std::cout << buffer << std::endl;
    }
    virtual IStub& withEnvironment(char* envp[]) override {
      for (auto envi = 0; envp[envi] != nullptr; ++envi) {
        auto equals = std::strchr(envp[envi], '=');
        if (equals == nullptr) {
          // KEY
          this->environment[envp[envi]] = {};
        } else if (equals > envp[envi]) {
          // KEY=VALUE
          this->environment[std::string(envp[envi], equals)] = equals + 1;
        }
      }
      return *this;
    }
    virtual ExitCode main() override {
      if (this->arguments.size() < 2) {
        return this->cmdEmpty();
      }
      if ((this->arguments.size() == 2) && (this->arguments[1] == "version")) {
        return this->cmdVersion();
      }
      this->error("Usage: " + this->appname() + " [<general-option>]... <command> [<command-option>|<command-argument>]...");
      return ExitCode::Usage;
    }
    std::string appname() {
      std::string arg0;
      if (!this->arguments.empty()) {
        arg0 = os::file::getExecutableName(this->arguments.front(), true);
      }
      if (arg0.empty()) {
        arg0 = os::file::getExecutableName(os::file::getExecutablePath(), true);
      }
      if (arg0.empty()) {
        arg0 = "egg";
      }
      return arg0;
    }
    void error(const std::string& message) {
      if ((this->logger != nullptr) && (this->allocator != nullptr)) {
        auto utf8 = String::fromUTF8(*this->allocator, message.c_str());
        this->logger->log(Source::Command, Severity::Error, utf8);
      } else {
        std::cerr << message << std::endl;
      }
    }
    void error(const std::exception& exception) {
      if ((this->logger != nullptr) && (this->allocator != nullptr)) {
        auto utf8 = StringBuilder::concat(*this->allocator, this->appname(), ": Exception: ", exception.what());
        this->logger->log(Source::Command, Severity::Error, utf8);
      } else {
        std::cerr << this->appname() << ": Exception: " << exception.what() << std::endl;
      }
    }
    void error(const Exception& exception) {
      if ((this->logger != nullptr) && (this->allocator != nullptr)) {
        StringBuilder sb;
        sb << this->appname() << ": " << exception;
        this->logger->log(Source::Command, Severity::Error, sb.build(*this->allocator));
      } else {
        std::cerr << this->appname() << ": ";
        Print::write(std::cerr, exception, Print::Options::DEFAULT);
        std::cerr << std::endl;
      }
    }
  private:
    ExitCode cmdEmpty() {
      std::cout << Version() << std::endl;
      for (auto& arg : this->arguments) {
        std::cout << arg << std::endl;
      }
      for (auto& env : this->environment) {
        std::cout << env.first << " = " << env.second << std::endl;
      }
      return ExitCode::OK;
    }
    ExitCode cmdVersion() {
      std::cout << Version() << std::endl;
      return ExitCode::OK;
    }
  };
}

int egg::yolk::IStub::main(int argc, char* argv[], char* envp[]) noexcept {
  auto exitcode = ExitCode::Error;
  Stub stub;
  try {
    exitcode = stub.withArguments(argc, argv).withEnvironment(envp).main();
  } catch (const Exception& exception) {
    stub.error(exception);
  } catch (const std::exception& exception) {
    stub.error(exception);
  } catch (...) {
    stub.error(stub.appname() + ": Fatal exception");
  }
  return static_cast<int>(exitcode);
}
