namespace egg::yolk {
  class IEggEngineLogger {
  public:
    using LogSource = egg::lang::LogSource;
    using LogSeverity = egg::lang::LogSeverity;
    virtual void log(LogSource source, LogSeverity severity, const std::string& message) = 0;
  };

  class IEggEnginePreparationContext : public IEggEngineLogger {
  public:
    virtual LogSeverity getMaximumSeverity() const = 0;
  };

  class IEggEngineExecutionContext : public IEggEngineLogger {
  public:
    virtual LogSeverity getMaximumSeverity() const = 0;

    // TODO
    virtual void print(const std::string& text) = 0;

    // Decoupled entry points
    egg::lang::ExecutionResult executeModule(const IEggParserNode& module, const std::vector<std::shared_ptr<IEggParserNode>>& statements);
    egg::lang::ExecutionResult executeDeclare(const IEggParserNode& declare, const std::vector<std::shared_ptr<IEggParserNode>>& statements);
  };

  class IEggEngine {
  public:
    using Severity = egg::lang::LogSeverity;
    virtual Severity prepare(IEggEnginePreparationContext& preparation) = 0;
    virtual Severity execute(IEggEngineExecutionContext& execution) = 0;
  };

  class EggEngineFactory {
  public:
    static std::shared_ptr<IEggEnginePreparationContext> createPreparationContext(const std::shared_ptr<IEggEngineLogger>& logger);
    static std::shared_ptr<IEggEngineExecutionContext> createExecutionContext(const std::shared_ptr<IEggEngineLogger>& logger);
    static std::shared_ptr<IEggEngine> createEngineFromTextStream(TextStream& stream);
    static std::shared_ptr<IEggEngine> createEngineFromParsed(const std::shared_ptr<IEggParserNode>& root);
  };
}
