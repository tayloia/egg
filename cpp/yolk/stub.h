namespace egg::yolk {
  class IStub : public egg::ovum::ILogger {
  public:
    enum class ExitCode : int {
      // See https://stackoverflow.com/a/40484670
      OK = 0,
      Error = 1,
      Usage = 2
    };
    // Interface
    virtual ~IStub() = default;
    virtual IStub& withArguments(int argc, char* argv[]) = 0;
    virtual IStub& withEnvironment(char* envp[]) = 0;
    virtual ExitCode main() = 0;
    // Helpers
    static int main(int argc, char* argv[], char* envp[]) noexcept;
  };
}
