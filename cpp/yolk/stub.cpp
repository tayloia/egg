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
      IAllocator* allocator = nullptr;
      ILogger* logger = nullptr;
      Severity loglevel = Configuration::makeLogLevelMask(Severity::Information);
      bool profileMemory = false;
      bool profileTime = false;
      // Helpers
      static Severity makeLogLevelMask(Severity severity) {
        if (severity == Severity::None) {
          return Severity::None;
        }
        assert(Bits::hasOneSet(severity));
        auto underlying = Bits::underlying(severity);
        return Severity(underlying | (underlying - 1));
      }
      bool isLogging(Severity severity) const {
        return Bits::hasAnySet(this->loglevel, severity);
      }
    };
    std::vector<std::string> arguments;
    std::map<std::string, std::string, LessCaseInsensitive> environment;
    std::map<std::string, Command> commands;
    std::map<std::string, Option> options;
    std::vector<size_t> breadcrumbs;
    Configuration configuration;
  public:
    virtual void log(Source source, Severity severity, const String& message) override {
      if (this->configuration.logger != nullptr) {
        this->configuration.logger->log(source, severity, message);
        return;
      }
      std::string origin;
      switch (source) {
      case Source::Compiler:
        origin = "<COMPILER>";
        break;
      case Source::Runtime:
        origin = "<RUNTIME>";
        break;
      case Source::Command:
        origin = "<COMMAND>";
        break;
      case Source::User:
        break;
      }
      switch (severity) {
      case Severity::None:
      default:
        std::cout << origin << message.toUTF8() << std::endl;
        break;
      case Severity::Debug:
        std::cout << origin << "<DEBUG>" << message.toUTF8() << std::endl;
        break;
      case Severity::Verbose:
        std::cout << origin << "<VERBOSE>" << message.toUTF8() << std::endl;
        break;
      case Severity::Information:
        std::cout << origin << "<INFORMATION>" << message.toUTF8() << std::endl;
        break;
      case Severity::Warning:
        std::cerr << origin << "<WARNING>" << message.toUTF8() << std::endl;
        break;
      case Severity::Error:
        std::cerr << origin << "<ERROR>" << message.toUTF8() << std::endl;
        break;
      }
    }
    virtual IStub& withAllocator(IAllocator& target) override {
      this->configuration.allocator = &target;
      return *this;
    }
    virtual IStub& withLogger(ILogger& target) override {
      this->configuration.logger = &target;
      return *this;
    }
    virtual IStub& withArgument(const std::string& argument) override {
      this->arguments.push_back(argument);
      return *this;
    }
    virtual IStub& withEnvironment(const std::string& key, const std::string& value) override {
      this->environment.emplace(key, value);
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
      this->withBuiltinOption(&Stub::optProfile, "profile[=memory|time|all]");
      this->withBuiltinCommand(&Stub::cmdHelp, "help");
      this->withBuiltinCommand(&Stub::cmdVersion, "version");
      return *this;
    }
    virtual size_t getArgumentCount() const override {
      return this->arguments.size();
    }
    virtual size_t getArgumentCommand() const override {
      if (this->breadcrumbs.empty()) {
        return this->breadcrumbs.front();
      }
      return 0;
    }
    virtual const std::string* queryArgument(size_t index) const override {
      return (index < this->arguments.size()) ? &this->arguments[index] : nullptr;
    }
    virtual const std::string* queryEnvironment(const std::string& key) const override {
      auto found = this->environment.find(key);
      return (found != this->environment.end()) ? &found->second : nullptr;
    }
    virtual ExitCode main() override {
      auto index = this->parseGeneralOptions();
      if (index == 0) {
        // Bad option error already spewed
        return ExitCode::Usage;
      }
      if (index >= this->arguments.size()) {
        // No command supplied
        auto handler = std::bind(&Stub::cmdMissing, this, std::placeholders::_1);
        return this->runCommand(handler);
      }
      const auto& command = this->arguments[index];
      auto found = this->commands.find(command);
      if (found != this->commands.end()) {
        this->breadcrumbs.push_back(index);
        return this->runCommand(found->second.handler);
      }
      this->badUsage("Unknown command: '" + command + "'");
      return ExitCode::Usage;
    }
    ExitCode runCommand(const CommandHandler& handler);
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
      this->redirect(Source::Command, Severity::Error, [=, this](std::ostream& os) {
        this->printBreadcrumbs(os);
        os << message;
      });
    }
    void error(const std::exception& exception) {
      this->redirect(Source::Command, Severity::Error, [=, this](std::ostream& os) {
        this->printBreadcrumbs(os);
        os << "Exception: " << exception.what();
      });
    }
    void error(const Exception& exception) {
      this->redirect(Source::Command, Severity::Error, [=, this](std::ostream& os) {
        this->printBreadcrumbs(os);
        Print::write(os, exception, Print::Options::DEFAULT);
      });
    }
    Stub& withArguments(int argc, char* argv[]) {
      for (auto argi = 0; argi < argc; ++argi) {
        this->arguments.push_back((argv[argi] == nullptr) ? std::string() : argv[argi]);
      }
      return *this;
    }
    Stub& withEnvironments(char* envp[]) {
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
    void redirect(Source source, Severity severity, const std::function<void(std::ostream&)>& callback) {
      if (this->configuration.isLogging(severity)) {
        if ((this->configuration.logger != nullptr) && (this->configuration.allocator != nullptr)) {
          // Use our attached logger
          std::stringstream ss;
          callback(ss);
          this->configuration.logger->log(source, severity, this->makeString(ss.str()));
        } else if (Bits::hasAnySet(severity, Bits::set(Severity::Warning, Severity::Error))) {
          // Send to stderr
          callback(std::cerr);
          std::cerr << std::endl;
        } else {
          // Send to stdout
          callback(std::cout);
          std::cout << std::endl;
        }
      }
    }
  private:
    using CommandMember = ExitCode(Stub::*)(const IStub&);
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
      size_t index = 0;
      while (++index < this->arguments.size()) {
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
        this->badGeneralOption(option, value);
        return false;
      }
      if (*value == "debug") {
        this->configuration.loglevel = Configuration::makeLogLevelMask(Severity::Debug);
      } else if (*value == "verbose") {
        this->configuration.loglevel = Configuration::makeLogLevelMask(Severity::Verbose);
      } else if (*value == "information") {
        this->configuration.loglevel = Configuration::makeLogLevelMask(Severity::Information);
      } else if (*value == "warning") {
        this->configuration.loglevel = Configuration::makeLogLevelMask(Severity::Warning);
      } else if (*value == "error") {
        this->configuration.loglevel = Configuration::makeLogLevelMask(Severity::Error);
      } else if (*value == "none") {
        this->configuration.loglevel = Configuration::makeLogLevelMask(Severity::None);
      } else {
        this->badGeneralOption(option, value);
        return false;
      }
      return true;
    }
    bool optProfile(const std::string& option, const std::string* value) {
      if ((value == nullptr) || (*value == "all")) {
        this->configuration.profileMemory = true;
        this->configuration.profileTime = true;
      } else if (*value == "memory") {
        this->configuration.profileMemory = true;
      } else if (*value == "time") {
        this->configuration.profileTime = true;
      } else {
        this->badGeneralOption(option, value);
        return false;
      }
      return true;
    }
    ExitCode cmdMissing(const IStub&) {
      this->redirect(Source::Command, Severity::Information, [this](std::ostream& os) {
        os << "Welcome to egg v" << Version::semver() << "\n";
        os << "Try '" << this->appname() << " help' for more information";
      });
      return ExitCode::OK;
    }
    ExitCode cmdHelp(const IStub&) {
      this->redirect(Source::Command, Severity::Information, [this](std::ostream& os) {
        this->printUsage(os);
        this->printGeneralOptions(os);
        this->printCommands(os);
      });
      return ExitCode::OK;
    }
    ExitCode cmdVersion(const IStub&) {
      this->redirect(Source::Command, Severity::Information, [](std::ostream& os) {
        os << "egg v" << Version();
      });
      return ExitCode::OK;
    }
    void badUsage(const std::string& message) {
      this->redirect(Source::Command, Severity::Error, [=, this](std::ostream& os) {
        this->printBreadcrumbs(os);
        os << message;
      });
      this->redirect(Source::Command, Severity::Information, [this](std::ostream& os) {
        this->printUsage(os);
      });
    }
    void badGeneralOption(const std::string& option, const std::string* value) {
      this->redirect(Source::Command, Severity::Error, [=, this](std::ostream& os) {
        this->printBreadcrumbs(os);
        if (value == nullptr) {
          os << "Missing general option: '--" << option << "'";
        } else {
          os << "Invalid general option: '--" << option << "=" << *value << "'";
        }
      });
      auto known = this->options.find(option);
      if (known != this->options.end()) {
        this->redirect(Source::Command, Severity::Information, [=](std::ostream& os) {
          os << "Option usage: '--" << known->second.usage << "'";
        });
      }
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
    void printBreadcrumbs(std::ostream& os) {
      os << this->appname();
      for (auto breadcrumb : this->breadcrumbs) {
        os << ' ' << this->arguments[breadcrumb];
      }
      os << ": ";
    }
    String makeString(const std::string& utf8) const {
      assert(this->configuration.allocator != nullptr);
      return String::fromUTF8(*this->configuration.allocator, utf8.data(), utf8.size());
    }
  };

  struct ProfileMemory {
    void initialize(Stub&) {
    }
    void report(std::ostream& os, Stub&) {
      os << "PROFILE: MEMORY: WIBBLE";
    }
  };

  struct ProfileTime {
    void initialize(Stub&) {
    }
    void report(std::ostream& os, Stub&) {
      os << "PROFILE: TIME: WIBBLE";
    }
  };

  template<typename T>
  class Profile final {
  private:
    Stub* stub;
    T profile;
  public:
    explicit Profile(Stub* stub)
      : stub(stub),
        profile() {
      if (this->stub != nullptr) {
        this->profile.initialize(*this->stub);
      }
    }
    ~Profile() {
      if (this->stub != nullptr) {
        this->stub->redirect(ILogger::Source::Command, ILogger::Severity::Information, [this](std::ostream& os) {
          this->profile.report(os, *this->stub);
        });
      }
    }
  };

  Stub::ExitCode Stub::runCommand(const CommandHandler& handler) {
    // Use RAII to ensure reporting even under exceptions
    Profile<ProfileMemory> profileMemory{ this->configuration.profileMemory ? this : nullptr };
    Profile<ProfileTime> profileTime{ this->configuration.profileTime ? this : nullptr };
    return handler(*this);
  }
}

int egg::yolk::IStub::main(int argc, char* argv[], char* envp[]) noexcept {
  auto exitcode = ExitCode::Error;
  Stub stub;
  try {
    exitcode = stub.withArguments(argc, argv).withEnvironments(envp).withBuiltins().main();
  } catch (const Exception& exception) {
    stub.error(exception);
  } catch (const std::exception& exception) {
    stub.error(exception);
  } catch (...) {
    stub.error("Fatal exception");
  }
  return static_cast<int>(exitcode);
}

std::unique_ptr<egg::yolk::IStub> egg::yolk::IStub::make() {
  return std::make_unique<Stub>();
}
