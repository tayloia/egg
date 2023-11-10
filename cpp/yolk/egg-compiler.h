namespace egg::yolk {
  class IEggParser;
  class IEggModule;

  class IEggCompiler {
  public:
    virtual ~IEggCompiler() {}
    virtual std::shared_ptr<IEggModule> compile(IEggParser& parser) = 0;
  };

  class EggCompilerFactory {
  public:
    static std::shared_ptr<IEggCompiler> createFromProgramBuilder(const egg::ovum::HardPtr<egg::ovum::IVMProgramBuilder>& builder);
  };
}
