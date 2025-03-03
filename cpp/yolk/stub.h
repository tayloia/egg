namespace egg::yolk {
  class IStub : public egg::ovum::ILogger {
  public:
    enum class ExitCode : int {
      // See https://stackoverflow.com/a/40484670
      OK = 0,
      Error = 1,
      Usage = 2
    };
    using CommandHandler = std::function<ExitCode(size_t)>;
    using OptionHandler = std::function<bool(const std::string&, const std::string*)>;
    // Interface
    virtual ~IStub() = default;
    virtual IStub& withArguments(int argc, char* argv[]) = 0;
    virtual IStub& withEnvironment(char* envp[]) = 0;
    virtual IStub& withCommand(const std::string& command, const std::string& usage, const CommandHandler& handler) = 0;
    virtual IStub& withOption(const std::string& option, const std::string& usage, const OptionHandler& handler) = 0;
    virtual IStub& withBuiltins() = 0;
    virtual ExitCode main() = 0;
    // Helpers
    static int main(int argc, char* argv[], char* envp[]) noexcept;
  };
}
