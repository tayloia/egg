namespace egg::yolk {
  class IEngineScript : public egg::ovum::IHardAcquireRelease {
  public:
    // Interface
    virtual egg::ovum::HardValue run() = 0;
  };

  // Engines cannot be 'IHardAcquireRelease' because the allocator is not known at construction
  class IEngine : public egg::ovum::IVMCommon {
  public:
    // Interface
    virtual ~IEngine() = default;
    virtual IEngine& withAllocator(egg::ovum::IAllocator& allocator) = 0;
    virtual IEngine& withBasket(egg::ovum::IBasket& basket) = 0;
    virtual IEngine& withLogger(egg::ovum::ILogger& logger) = 0;
    virtual IEngine& withVM(egg::ovum::IVM& vm) = 0;
    virtual IEngine& withBuiltin(const egg::ovum::String& symbol, const egg::ovum::HardObject& instance) = 0;
    virtual egg::ovum::IAllocator& getAllocator() = 0;
    virtual egg::ovum::IBasket& getBasket() = 0;
    virtual egg::ovum::ILogger& getLogger() = 0;
    virtual egg::ovum::IVM& getVM() = 0;
    virtual egg::ovum::String getBuiltinSymbol(size_t index) = 0;
    virtual egg::ovum::HardObject getBuiltinInstance(const egg::ovum::String& symbol) = 0;
    virtual egg::ovum::HardPtr<IEngineScript> loadScriptFromString(const egg::ovum::String& script, const egg::ovum::String& resource = {}) = 0;
  };

  class EngineFactory {
  public:
    // Helpers
    static std::shared_ptr<IEngine> createDefault();
  };
}
