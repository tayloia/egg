namespace egg::yolk {
  class IEggEngineLogger {
  public:
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) = 0;
  };

  class IEggEngineExecution {
  public:
    virtual void print(const std::string& text) = 0;
  };

  class IEggEngine {
  public:
    virtual std::shared_ptr<IEggEngineExecution> createExecutionFromLogger(const std::shared_ptr<IEggEngineLogger>& logger) = 0;
    virtual void execute(IEggEngineExecution& execution) = 0;
  };

  class EggEngineFactory {
  public:
    static std::shared_ptr<IEggEngine> createEngineFromParsed(const std::shared_ptr<IEggParserNode>& root);
  };
}
