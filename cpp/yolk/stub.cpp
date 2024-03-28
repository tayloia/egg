#include "yolk/yolk.h"
#include "yolk/stub.h"
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
    struct Command {
      std::string command;
      std::string usage;
      CommandHandler handler;
    };
    struct Option {
      std::string option;
      std::string usage;
      OptionHandler handler;
      size_t occurrences;
    };
    struct Configuration {
      Severity verbose = Severity::Information;
    };
    std::vector<std::string> arguments;
    std::map<std::string, std::string, LessCaseInsensitive> environment;
    std::map<std::string, Command> commands;
    std::map<std::string, Option> options;
    Configuration configuration;
    IAllocator* allocator = nullptr;
    ILogger* logger = nullptr;
  public:
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
    virtual IStub& withArguments(int argc, char* argv[]) override {
      for (auto argi = 0; argi < argc; ++argi) {
        this->arguments.push_back((argv[argi] == nullptr) ? std::string() : argv[argi]);
      }
      return *this;
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
    virtual IStub& withCommand(const std::string& command, const std::string& usage, const CommandHandler& handler) override {
      this->commands.emplace(std::piecewise_construct, std::forward_as_tuple(command), std::forward_as_tuple(command, usage, handler));
      return *this;
    }
    virtual IStub& withOption(const std::string& option, const std::string& usage, const OptionHandler& handler) override {
      this->options.emplace(std::piecewise_construct, std::forward_as_tuple(option), std::forward_as_tuple(option, usage, handler));
      return *this;
    }
    virtual IStub& withBuiltins() override {
      this->withBuiltinOption(&Stub::optLogLevel, "log-level=debug|verbose|information|warning|error|none");
      this->withBuiltinCommand(&Stub::cmdHelp, "help");
      this->withBuiltinCommand(&Stub::cmdVersion, "version");
      return *this;
    }
    virtual ExitCode main() override {
      auto index = this->parseGeneralOptions();
      if (index == 0) {
        // Bad option error already spewed
        return ExitCode::Usage;
      }
      if (index >= this->arguments.size()) {
        // No command supplied
        return this->cmdEmpty();
      }
      const auto& command = this->arguments[index];
      auto found = this->commands.find(command);
      if (found != this->commands.end()) {
        return found->second.handler(index);
      }
      this->badUsage("Unknown command: '" + command + "'");
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
    using CommandMember = ExitCode(Stub::*)(size_t);
    using OptionMember = bool(Stub::*)(const std::string&, const std::string*);
    void withBuiltinCommand(const CommandMember& member, const std::string& usage) {
      auto handler = std::bind(member, this, std::placeholders::_1);
      auto space = usage.find(' ');
      if (space == std::string::npos) {
        this->withCommand(usage, usage, handler);
      } else {
        this->withCommand(usage.substr(0, space), usage, handler);
      }
    }
    void withBuiltinOption(const OptionMember& member, const std::string& usage) {
      auto handler = std::bind(member, this, std::placeholders::_1, std::placeholders::_2);
      auto equals = usage.find_first_of("=[");
      if (equals == std::string::npos) {
        this->withOption(usage, usage, handler);
      } else {
        this->withOption(usage.substr(0, equals), usage, handler);
      }
    }
    size_t parseGeneralOptions() {
      size_t index = 1;
      while (index < this->arguments.size()) {
        const auto& argument = this->arguments[index];
        if (!argument.starts_with("--")) {
          break;
        }
        auto equals = argument.find('=');
        if ((equals < 3) || (equals == std::string::npos)) {
          // '--option' or '--=...'
          if (!this->parseGeneralOption(argument.substr(2), nullptr)) {
            // Bad option
            return 0;
          }
        } else {
          // '--option=...'
          std::string value = argument.substr(equals + 1);
          if (!this->parseGeneralOption(argument.substr(2, equals - 2), &value)) {
            // Bad option
            return 0;
          }
        }
      }
      return index;
    }
    bool parseGeneralOption(const std::string& option, const std::string* value) {
      auto found = this->options.find(option);
      if (found == this->options.end()) {
        this->badUsage("Unknown general option: '--" + option + "'");
        return false;
      }
      found->second.occurrences++;
      return found->second.handler(option, value);
    }
    bool optLogLevel(const std::string& option, const std::string* value) {
      if (this->options[option].occurrences > 1) {
        this->badUsage("Duplicated general option: '--" + option + "'");
        return false;
      }
      if (value == nullptr) {
        this->badUsage("Missing value for general option '--" + option + "'");
        return false;
      }
      this->configuration.verbose = Severity::Verbose;
      return true;
    }
    ExitCode cmdEmpty() {
      this->redirect(Source::Command, Severity::Information, [this](std::ostream& os) {
        os << "Welcome to egg v" << Version::semver() << "\n";
        os << "Try '" << this->appname() << " help' for more information";
      });
      return ExitCode::OK;
    }
    ExitCode cmdHelp(size_t) {
      this->redirect(Source::Command, Severity::Information, [this](std::ostream& os) {
        this->printUsage(os);
        this->printGeneralOptions(os);
        this->printCommands(os);
      });
      return ExitCode::OK;
    }
    ExitCode cmdVersion(size_t) {
      this->redirect(Source::Command, Severity::Information, [](std::ostream& os) {
        os << "egg v" << Version();
      });
      return ExitCode::OK;
    }
    void badUsage(const std::string& message) {
      this->redirect(Source::Command, Severity::Error, [=,this](std::ostream& os) {
        os << this->appname() << ": " << message;
      });
      this->redirect(Source::Command, Severity::Information, [this](std::ostream& os) {
        this->printUsage(os);
      });
    }
    void printUsage(std::ostream& os) {
      os << "Usage: " << this->appname() << " [<general-option>]... <command> [<command-option>|<command-argument>]...";
    }
    void printGeneralOptions(std::ostream& os) {
      os << "\n  <general-option> is any of:";
      for (const auto& option : this->options) {
        os << "\n    --" << option.second.usage;
      }
    }
    void printCommands(std::ostream& os) {
      os << "\n  <command> is one of:";
      for (const auto& command : this->commands) {
        os << "\n    " << command.second.usage;
      }
    }
    void redirect(Source source, Severity severity, const std::function<void(std::ostream&)>& callback) {
      if ((this->logger != nullptr) && (this->allocator != nullptr)) {
        // Use our attached logger
        std::stringstream ss;
        callback(ss);
        this->logger->log(source, severity, String::fromUTF8(*this->allocator, ss.str().c_str()));
      } else if ((severity == Severity::Error) || (severity == Severity::Warning)) {
        // Send to stderr
        callback(std::cerr);
        std::cerr << std::endl;
      } else {
        // Send to stdout
        callback(std::cout);
        std::cout << std::endl;
      }
    }
  };
}

int egg::yolk::IStub::main(int argc, char* argv[], char* envp[]) noexcept {
  auto exitcode = ExitCode::Error;
  Stub stub;
  try {
    exitcode = stub.withArguments(argc, argv).withEnvironment(envp).withBuiltins().main();
  } catch (const Exception& exception) {
    stub.error(exception);
  } catch (const std::exception& exception) {
    stub.error(exception);
  } catch (...) {
    stub.error(stub.appname() + ": Fatal exception");
  }
  return static_cast<int>(exitcode);
}
