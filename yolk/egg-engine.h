namespace egg::yolk {
  class IEggProgramNode;

  class IEggEnginePreparationContext : public egg::ovum::ILogger {
  public:
    virtual egg::ovum::IAllocator& allocator() const = 0;
    };

  class IEggEngineExecutionContext : public egg::ovum::ILogger {
  public:
    virtual egg::ovum::IAllocator& allocator() const = 0;
  };

  class IEggEngineCompilationContext : public egg::ovum::ILogger {
  public:
    virtual egg::ovum::IAllocator& allocator() const = 0;
  };

  class IEggEngine {
  public:
    virtual ~IEggEngine() {}
    virtual egg::ovum::ILogger::Severity prepare(IEggEnginePreparationContext& preparation) = 0;
    virtual egg::ovum::ILogger::Severity execute(IEggEngineExecutionContext& execution) = 0;
    virtual egg::ovum::ILogger::Severity compile(IEggEngineCompilationContext& compilation, egg::ovum::Module& out) = 0;
  };

  class EggEngineFactory {
  public:
    static std::shared_ptr<IEggEnginePreparationContext> createPreparationContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger);
    static std::shared_ptr<IEggEngineExecutionContext> createExecutionContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger);
    static std::shared_ptr<IEggEngineCompilationContext> createCompilationContext(egg::ovum::IAllocator& allocator, const std::shared_ptr<egg::ovum::ILogger>& logger);
    static std::shared_ptr<IEggEngine> createEngineFromParsed(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const std::shared_ptr<IEggProgramNode>& root);
    static std::shared_ptr<IEggEngine> createEngineFromTextStream(TextStream& stream);
  };
}
