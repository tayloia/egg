namespace egg::ovum {
  class IEggbox;
}

namespace egg::yolk {
  class IStub : public egg::ovum::ILogger {
  public:
    enum class ExitCode : int {
      // See https://stackoverflow.com/a/40484670
      OK = 0,
      Error = 1,
      Usage = 2
    };
    using CommandHandler = std::function<ExitCode(const IStub&)>;
    using OptionHandler = std::function<bool(const std::string&, const std::string*)>;
    // Construction interface
    virtual ~IStub() = default;
    virtual IStub& withAllocator(egg::ovum::IAllocator& target) = 0;
    virtual IStub& withLogger(egg::ovum::ILogger& target) = 0;
    virtual IStub& withEggbox(egg::ovum::IEggbox& target) = 0;
    virtual IStub& withArgument(const std::string& argument) = 0;
    virtual IStub& withEnvironment(const std::string& key, const std::string& value) = 0;
    virtual IStub& withCommand(const std::string& command, const std::string& usage, const CommandHandler& handler) = 0;
    virtual IStub& withOption(const std::string& option, const std::string& usage, const OptionHandler& handler) = 0;
    virtual IStub& withBuiltins() = 0;
    virtual ExitCode main() = 0;
    // Interrogation interface
    virtual const std::string* queryArgument(size_t index) const = 0;
    virtual const std::string* queryEnvironment(const std::string& key) const = 0;
    // Helpers
    std::string getArgument(size_t index, const std::string defval = {}) const {
      auto q = this->queryArgument(index);
      return (q == nullptr) ? defval : *q;
    }
    std::string getEnvironment(const std::string& key, const std::string defval = {}) const {
      auto q = this->queryEnvironment(key);
      return (q == nullptr) ? defval : *q;
    }
    static int main(int argc, char* argv[], char* envp[]) noexcept;
    static std::unique_ptr<IStub> make();
  };
}
