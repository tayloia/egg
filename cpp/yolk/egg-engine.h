namespace egg::yolk {
  class IEggProgramNode;

  class IEggEngineContext : public egg::ovum::ILogger {
  public:
    virtual egg::ovum::IAllocator& getAllocator() = 0;
    virtual egg::ovum::ITypeFactory& getTypeFactory() = 0;
  };

  class IEggEngine {
  public:
    virtual ~IEggEngine() {}
    virtual egg::ovum::ILogger::Severity prepare(IEggEngineContext& context) = 0;
    virtual egg::ovum::ILogger::Severity compile(IEggEngineContext& context, egg::ovum::Module& out) = 0;
    virtual egg::ovum::ILogger::Severity execute(IEggEngineContext& context, const egg::ovum::Module& module) = 0;

    // Helper for compile-and-execute
    egg::ovum::ILogger::Severity execute(IEggEngineContext& context);

    // Helper for prepare-compile-and-execute
    egg::ovum::ILogger::Severity run(IEggEngineContext& context);
  };

  class EggEngineFactory {
  public:
    static std::shared_ptr<IEggEngineContext> createContext(egg::ovum::ITypeFactory& factory, egg::ovum::IBasket& basket, const std::shared_ptr<egg::ovum::ILogger>& logger);
    static std::shared_ptr<IEggEngine> createEngineFromParsed(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const std::shared_ptr<IEggProgramNode>& root);
    static std::shared_ptr<IEggEngine> createEngineFromTextStream(TextStream& stream);
  };
}
