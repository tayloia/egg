namespace egg::test {
  class EggEngineContextAllocator : public egg::yolk::IEggEngineContext {
    EggEngineContextAllocator(const EggEngineContextAllocator&) = delete;
    EggEngineContextAllocator& operator=(const EggEngineContextAllocator&) = delete;
  private:
    Allocator& allocator;
    Logger logger;
  public:
    explicit EggEngineContextAllocator(Allocator& allocator) : allocator(allocator), logger() {}
    virtual egg::ovum::IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual void log(Source source, Severity severity, const std::string& message) override {
      this->logger.log(source, severity, message);
    }
    std::string logged() const {
      return this->logger.logged.str();
    }
  };

  class EggEngineContext final : public EggEngineContextAllocator {
    EggEngineContext(const EggEngineContext&) = delete;
    EggEngineContext& operator=(const EggEngineContext&) = delete;
  private:
    egg::test::Allocator allocator;
  public:
    explicit EggEngineContext(Allocator::Expectation expectation = Allocator::Expectation::AtLeastOneAllocation)
      : EggEngineContextAllocator(allocator),
        allocator(expectation) {
    }
  };
}
