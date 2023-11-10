namespace egg::yolk {
  class IEggModule {
  public:
    virtual ~IEggModule() {}
    virtual void execute() const = 0;
  };

  class EggModuleFactory {
  public:
    // Usually constructed via IEggCompiler::compile, but these are useful for testing simple modules
    static std::shared_ptr<IEggModule> createFromPath(egg::ovum::IVM& vm, const std::string& script, bool swallowBOM = true);
    static std::shared_ptr<IEggModule> createFromStream(egg::ovum::IVM& vm, TextStream& script);
  };
}
