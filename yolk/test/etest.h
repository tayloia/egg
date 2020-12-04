namespace egg::test {
  class EggEngineContextFromFactory : public egg::yolk::IEggEngineContext {
    EggEngineContextFromFactory(const EggEngineContextFromFactory&) = delete;
    EggEngineContextFromFactory& operator=(const EggEngineContextFromFactory&) = delete;
  private:
    egg::ovum::TypeFactory& factory;
    Logger logger;
  public:
    explicit EggEngineContextFromFactory(egg::ovum::TypeFactory& factory) : factory(factory), logger() {}
    virtual egg::ovum::IAllocator& getAllocator() override {
      return this->factory.allocator;
    }
    virtual egg::ovum::TypeFactory& getTypeFactory() override {
      return this->factory;
    }
    virtual void log(Source source, Severity severity, const std::string& message) override {
      this->logger.log(source, severity, message);
    }
    std::string logged() const {
      return this->logger.logged.str();
    }
  };

  class EggEngineContext final : public EggEngineContextFromFactory {
    EggEngineContext(const EggEngineContext&) = delete;
    EggEngineContext& operator=(const EggEngineContext&) = delete;
  private:
    egg::test::Allocator allocator;
    egg::ovum::TypeFactory factory;
  public:
    explicit EggEngineContext(Allocator::Expectation expectation = Allocator::Expectation::AtLeastOneAllocation)
      : EggEngineContextFromFactory(factory),
        allocator(expectation),
        factory(allocator) {
    }
  };
}
