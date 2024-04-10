#include "yolk/yolk.h"
#include "yolk/stub.h"
#include "yolk/options.h"
#include "yolk/engine.h"
#include "ovum/eggbox.h"
#include "ovum/file.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"
#include "ovum/stream.h"
#include "ovum/version.h"

#include <cctype>
#include <iostream>

#define STUB_LOG(severity) \
  if (!this->isLogging(Severity::severity)) {} else this->logStream(Severity::severity)

namespace {
  using namespace egg::ovum;
  using namespace egg::yolk;

  class Stub;

  class LogStream {
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;
  public:
    using FunctionCallback = std::function<void(std::ostream&)>;
    using MemberCallback = void(Stub::*)(std::ostream&);
    Stub& stub;
    ILogger::Severity severity;
    std::stringstream sstream;
    std::ostream& ostream;
    LogStream(Stub& stub, ILogger::Severity severity, std::ostream& ostream)
      : stub(stub),
        severity(severity),
        ostream(ostream) {
    }
    LogStream(Stub& stub, ILogger::Severity severity)
      : LogStream(stub, severity, sstream) {
    }
    ~LogStream();
    template<typename T>
    LogStream& operator<<(const T& value) {
      this->ostream << value;
      return *this;
    }
    LogStream& operator<<(const FunctionCallback& callback) {
      callback(this->ostream);
      return *this;
    }
    LogStream& operator<<(const MemberCallback& callback);
    template<typename T>
    static FunctionCallback print(const T& value) {
      return [value](std::ostream& os) {
        Print::write(os, value, Print::Options::DEFAULT);
      };
    }
  };

  struct LessCaseInsensitive {
    bool operator()(const unsigned char& lhs, const unsigned char& rhs) const {
      return std::tolower(lhs) < std::tolower(rhs);
    }
    bool operator()(const std::string& lhs, const std::string& rhs) const {
      return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), *this);
    }
  };

  class Stub : public IStub {
    Stub(const Stub&) = delete;
    Stub& operator=(const Stub&) = delete;
  private:
    using CommandMember = ExitCode(Stub::*)();
    using OptionMember = bool(Stub::*)(const std::string&, const std::string*);
    struct Subcommand {
      std::string usage;
      CommandMember handler;
    };
    struct Command {
      std::string command;
      std::string usage;
      CommandHandler handler;
      std::map<std::string, Subcommand> subcommands;
      Command& withSubcommand(const CommandMember& subhandler, const std::string& subusage) {
        auto key{ subusage };
        auto space = key.find(' ');
        if (space != std::string::npos) {
          key.erase(space);
        }
        this->subcommands.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(subusage, subhandler));
        return *this;
      }
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
      IEggbox* eggbox = nullptr;
      Severity loglevel = Configuration::makeLogLevelMask(Severity::Information);
      bool profileAllocator = false;
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
    };
    std::vector<std::string> arguments;
    std::map<std::string, std::string, LessCaseInsensitive> environment;
    std::map<std::string, Command> commands;
    std::map<std::string, Option> options;
    std::vector<size_t> breadcrumbs;
    Configuration configuration;
    AllocatorDefault uallocator;
    std::shared_ptr<IEggbox> ueggbox;
  public:
    Stub() {
    }
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
      assert(this->configuration.allocator == nullptr);
      this->configuration.allocator = &target;
      return *this;
    }
    virtual IStub& withLogger(ILogger& target) override {
      this->configuration.logger = &target;
      return *this;
    }
    virtual IStub& withEggbox(IEggbox& target) override {
      this->configuration.eggbox = &target;
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
      this->withBuiltinHandler(command, usage, handler);
      return *this;
    }
    virtual IStub& withOption(const std::string& option, const std::string& usage, const OptionHandler& handler) override {
      this->options.emplace(std::piecewise_construct, std::forward_as_tuple(option), std::forward_as_tuple(option, usage, handler));
      return *this;
    }
    virtual IStub& withBuiltins() override {
      this->withBuiltinOption(&Stub::optLogLevel, "log-level=debug|verbose|information|warning|error|none");
      this->withBuiltinOption(&Stub::optProfile, "profile[=allocator|memory|time|all]");
      this->withBuiltinCommand(&Stub::cmdHelp, "help");
      this->withBuiltinCommand(&Stub::subcommand, "sandwich")
        .withSubcommand(&Stub::cmdSandwichMake, "make --target=<exe-file> --zip=<zip-file>");
      this->withBuiltinCommand(&Stub::cmdRun, "run <script-file>");
      this->withBuiltinCommand(&Stub::cmdSmokeTest, "smoke-test");
      this->withBuiltinCommand(&Stub::cmdVersion, "version");
      this->withBuiltinCommand(&Stub::subcommand, "zip")
        .withSubcommand(&Stub::cmdZipMake, "make --target=<zip-file> --directory=<source-path>");
      return *this;
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
        STUB_LOG(Debug) << "No command supplied";
        auto handler = std::bind(&Stub::cmdMissing, this);
        return this->runCommand(handler);
      }
      const auto& command = this->arguments[index];
      STUB_LOG(Debug) << "Command supplied at index " << index << ": '" << command << "'";
      auto found = this->commands.find(command);
      if (found != this->commands.end()) {
        this->breadcrumbs.push_back(index);
        return this->runCommand(found->second.handler);
      }
      this->badUsage("Unknown command: '" + command + "'");
      return ExitCode::Usage;
    }
    const Configuration& getConfiguration() const {
      return this->configuration;
    }
    std::string getApplicationName() const {
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
    ExitCode runCommand(const CommandHandler& handler);
    void error(const std::string& message) {
      STUB_LOG(Error) << BREADCRUMBS << ": " << message;
    }
    void error(const std::exception& exception) {
      STUB_LOG(Error) << BREADCRUMBS << ": Exception: " << exception.what();
    }
    void error(const Exception& exception) {
      STUB_LOG(Error) << BREADCRUMBS << ": " << LogStream::print(exception);
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
    bool isLogging(Severity severity) const {
      return Bits::hasAnySet(this->configuration.loglevel, severity);
    }
    LogStream logStream(Severity severity) {
      if ((this->configuration.logger != nullptr) && (this->configuration.allocator != nullptr)) {
        // Use our attached logger
        return LogStream(*this, severity);
      } else if (Bits::hasAnySet(severity, Bits::set(Severity::Warning, Severity::Error))) {
        // Send to stderr
        return LogStream(*this, severity, std::cerr);
      } else {
        // Send to stdout
        return LogStream(*this, severity, std::cout);
      }
    }
    void logCommit(Source source, Severity severity, const std::string& message) const {
      assert(this->configuration.logger != nullptr);
      assert(this->configuration.allocator != nullptr);
      this->configuration.logger->log(source, severity, makeString(message));
    }
  private:
    Command& withBuiltinCommand(const CommandMember& member, const std::string& usage) {
      auto handler = std::bind(member, this);
      auto space = usage.find(' ');
      if (space == std::string::npos) {
        return this->withBuiltinHandler(usage, usage, handler);
      }
      return this->withBuiltinHandler(usage.substr(0, space), usage, handler);
    }
    Command& withBuiltinHandler(const std::string& command, const std::string& usage, const CommandHandler& handler) {
      return this->commands.emplace(std::piecewise_construct, std::forward_as_tuple(command), std::forward_as_tuple(command, usage, handler)).first->second;
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
        this->configuration.profileAllocator = true;
        this->configuration.profileMemory = true;
        this->configuration.profileTime = true;
      }
      else if (*value == "allocator") {
        this->configuration.profileAllocator = true;
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
    ExitCode subcommand() {
      assert(this->breadcrumbs.size() == 1);
      auto index = this->breadcrumbs.back();
      const auto& command = this->commands[this->arguments[index++]];
      auto missing = true;
      if (index < this->arguments.size()) {
        const auto& key = this->arguments[index];
        if (!key.starts_with("--")) {
          auto found = command.subcommands.find(key);
          if (found != command.subcommands.end()) {
            this->breadcrumbs.push_back(index);
            return (this->*found->second.handler)();
          }
          STUB_LOG(Error) << BREADCRUMBS << ": Unknown subcommand: '" << key << "'";
          missing = false;
        }
      }
      if (missing) {
        STUB_LOG(Error) << BREADCRUMBS << ": Missing subcommand";
      }
      STUB_LOG(Information) << "Usage: " << BREADCRUMBS << " <subcommand>" << this->printSubcommands(command);
      return ExitCode::Usage;
    }
    ExitCode cmdMissing() {
      STUB_LOG(Information) << "Welcome to egg v" << Version::semver() << "\n"
                                 "Try '" << this->getApplicationName() << " help' for more information";
      return ExitCode::OK;
    }
    ExitCode cmdHelp() {
      STUB_LOG(Information) << &Stub::printUsage << &Stub::printGeneralOptions << &Stub::printCommands;
      return ExitCode::OK;
    }
    ExitCode cmdSandwichMake() {
      // stub.exe sandwich make --target=.../egg.exe --zip=.../sandwich.zip
      auto parser = this->makeOptionParser()
        .withStringOption("target", OptionParser::Occurrences::One)
        .withStringOption("zip", OptionParser::Occurrences::One);
      auto suboptions = parser.parse();
      auto target = suboptions.get("target");
      auto zip = suboptions.get("zip");
      auto embedded = File::createSandwichFromFile(target, zip);
      STUB_LOG(Information) << "Embedded " << embedded << " bytes into '" << target << "'";
      return ExitCode::OK;
    }
    ExitCode cmdRun() {
      // egg run <script-path>
      auto parser = this->makeOptionParser()
        .withExtraneousArguments(OptionParser::Occurrences::One);
      auto suboptions = parser.parse();
      auto stream = File::resolveTextStream(suboptions.extraneous()[0]);
      return this->runScript(*stream);
    }
    ExitCode cmdSmokeTest() {
      // egg smoke-test
      // NOTE: We force using the script from the eggbox
      auto parser = this->makeOptionParser();
      auto suboptions = parser.parse();
      auto& eggbox = this->getEggbox();
      EggboxTextStream stream{ eggbox, "command/smoke-test.egg" };
      return this->runScript(stream);
    }
    ExitCode cmdVersion() {
      STUB_LOG(Information) << "egg v" << Version();
      return ExitCode::OK;
    }
    ExitCode cmdZipMake() {
      // stub.exe zip make --target=.../sandwich.zip --directory=box
      auto parser = this->makeOptionParser()
        .withStringOption("target", OptionParser::Occurrences::One)
        .withStringOption("directory", OptionParser::Occurrences::One);
      auto suboptions = parser.parse();
      auto target = suboptions.get("target");
      auto directory = suboptions.get("directory");
      uint64_t compressed, uncompressed;
      auto entries = EggboxFactory::createZipFileFromDirectory(target, directory, compressed, uncompressed);
      std::string readable = "1 entry";
      if (entries != 1) {
        readable = std::to_string(entries) + " entries";
      }
      auto ratio = (uncompressed > 0) ? (compressed * 100) / uncompressed : 100;
      STUB_LOG(Information) << "Zipped " << readable << " into '" << target << "' (compressed=" << compressed << " uncompressed=" << uncompressed << " ratio=" << ratio << "%)";
      return ExitCode::OK;
    }
    IEggbox& getEggbox() {
      if (this->configuration.eggbox == nullptr) {
        assert(this->ueggbox == nullptr);
        this->ueggbox = egg::ovum::EggboxFactory::openDefault();
        this->configuration.eggbox = this->ueggbox.get();
      }
      return *this->configuration.eggbox;
    }
    ExitCode runScript(TextStream& stream) {
      auto engine = this->makeEngine();
      auto script = engine->loadScriptFromTextStream(stream);
      auto retval = script->run();
      if (retval->getPrimitiveFlag() != ValueFlags::Void) {
        STUB_LOG(Error) << "'" << stream.getResourceName() << "' did not return 'void'";
        return ExitCode::Error;
      }
      return ExitCode::OK;
    }
    void badUsage(const std::string& message) {
      STUB_LOG(Error) << BREADCRUMBS << ": " << message;
      STUB_LOG(Information) << &Stub::printUsage;
    }
    void badGeneralOption(const std::string& option, const std::string* value) {
      if (value == nullptr) {
        STUB_LOG(Error) << BREADCRUMBS << ": Missing general option: '--" << option << "'";
      } else {
        STUB_LOG(Error) << BREADCRUMBS << ": Invalid general option: '--" << option << "=" << *value << "'";
      }
      auto known = this->options.find(option);
      if (known != this->options.end()) {
        STUB_LOG(Information) << "Option usage: '--" << known->second.usage << "'";
      }
    }
    void printBreadcrumbs(std::ostream& os) {
      os << this->getApplicationName();
      for (auto breadcrumb : this->breadcrumbs) {
        os << ' ' << this->arguments[breadcrumb];
      }
    }
    void printUsage(std::ostream& os) {
      os << "Usage: " << this->getApplicationName() << " [<general-option>]... <command> [<command-option>|<command-argument>]...";
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
    LogStream::FunctionCallback printSubcommands(const Command& command)  {
      return [command](std::ostream& os) {
        os << "\n <subcommand> is one of:";
        for (auto& subcommand : command.subcommands) {
          os << "\n  " << subcommand.second.usage;
        }
      };
    };
    std::shared_ptr<IEngine> makeEngine() {
      IEngine::Options eo{};
      auto engine = EngineFactory::createWithOptions(eo);
      assert(engine != nullptr);
      if (this->configuration.allocator != nullptr) {
        engine->withAllocator(*this->configuration.allocator);
      }
      if (this->configuration.logger != nullptr) {
        engine->withLogger(*this->configuration.logger);
      }
      return engine;
    }
    String makeString(const std::string& utf8) const {
      assert(this->configuration.allocator != nullptr);
      return String::fromUTF8(*this->configuration.allocator, utf8.data(), utf8.size());
    }
    OptionParser makeOptionParser() {
      OptionParser parser;
      size_t index = this->breadcrumbs.empty() ? 0 : this->breadcrumbs.back();
      while (++index < this->arguments.size()) {
        parser.withArgument(this->arguments[index]);
      }
      return parser;
    }
    static constexpr auto BREADCRUMBS = &Stub::printBreadcrumbs;
  };

  struct ProfileAllocator {
    void report(std::ostream& os, Stub& stub) {
      auto* allocator = stub.getConfiguration().allocator;
      if (allocator == nullptr) {
        os << "profile: allocator: unused";
      } else {
        IAllocator::Statistics statistics;
        if (allocator->statistics(statistics)) {
          os << "profile: allocator:"
            " total-blocks=" << statistics.totalBlocksAllocated <<
            " total-bytes=" << statistics.totalBytesAllocated;
        } else {
          os << "profile: allocator: unavailable";
        }
      }
    }
  };

  struct ProfileMemory {
    void report(std::ostream& os, Stub&) {
      auto snapshot = os::memory::snapshot();
      os << "profile: memory:" <<
        " data=" << snapshot.currentBytesData <<
        " total=" << snapshot.currentBytesTotal <<
        " peak-data=" << snapshot.peakBytesData <<
        " peak-total=" << snapshot.peakBytesTotal;
    }
  };

  struct ProfileTime {
    void report(std::ostream& os, Stub&) {
      auto snapshot = os::process::snapshot();
      os << "profile: time:" <<
        " user=" << snapshot.microsecondsUser <<
        " system=" << snapshot.microsecondsSystem <<
        " elapsed=" << snapshot.microsecondsElapsed;
    }
  };

  template<typename T>
  class Profile final {
  private:
    Stub* stub;
    T instance;
  public:
    explicit Profile(Stub* stub)
      : stub(stub),
        instance() {
    }
    ~Profile() {
      if ((this->stub != nullptr) && this->stub->isLogging(ILogger::Severity::Information)) {
        std::function callback = [this](std::ostream& os) {
          this->instance.report(os, *this->stub);
        };
        this->stub->logStream(ILogger::Severity::Information) << callback;
      }
    }
  };

  Stub::ExitCode Stub::runCommand(const CommandHandler& handler) {
    // Use RAII to ensure reporting even under exceptions
    Profile<ProfileAllocator> profileAllocator{ this->configuration.profileAllocator ? this : nullptr };
    Profile<ProfileMemory> profileMemory{ this->configuration.profileMemory ? this : nullptr };
    Profile<ProfileTime> profileTime{ this->configuration.profileTime ? this : nullptr };
    if (this->configuration.allocator == nullptr) {
      this->withAllocator(this->uallocator);
      return handler(*this);
    }
    return handler(*this);
  }

  LogStream& LogStream::operator<<(const MemberCallback& callback) {
    auto bound = std::bind(callback, &this->stub, std::placeholders::_1);
    bound(this->ostream);
    return *this;
  }

  LogStream::~LogStream() {
    if (&this->ostream == &this->sstream) {
      this->stub.logCommit(ILogger::Source::Command, this->severity, this->sstream.str());
    } else {
      this->ostream << std::endl;
    }
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
