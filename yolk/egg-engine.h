namespace egg::yolk {
  class IEggEngineLogger {
  public:
    virtual ~IEggEngineLogger() {}
    virtual void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message) = 0;
  };

  class IEggEnginePreparationContext : public IEggEngineLogger {
  public:
    virtual ~IEggEnginePreparationContext() {}
    virtual egg::ovum::IAllocator& allocator() const = 0;
    };

  class IEggEngineExecutionContext : public IEggEngineLogger {
  public:
    virtual ~IEggEngineExecutionContext() {}
    virtual egg::ovum::IAllocator& allocator() const = 0;
  };

  class IEggEngine {
  public:
    virtual ~IEggEngine() {}
    virtual egg::lang::LogSeverity prepare(IEggEnginePreparationContext& preparation) = 0;
    virtual egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution) = 0;
  };

  class EggEngineFactory {
  public:
    static std::shared_ptr<IEggEnginePreparationContext> createPreparationContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggEngineLogger>& logger);
    static std::shared_ptr<IEggEngineExecutionContext> createExecutionContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggEngineLogger>& logger);
    static std::shared_ptr<IEggEngine> createEngineFromParsed(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggProgramNode>& root);
    static std::shared_ptr<IEggEngine> createEngineFromTextStream(TextStream& stream);
  };
}
