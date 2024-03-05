namespace egg::ovum {
  class IEggParser;

  class IEggCompiler {
  public:
    virtual ~IEggCompiler() {}
    virtual egg::ovum::HardPtr<egg::ovum::IVMModule> compile(egg::ovum::IEggParser& parser) = 0;
  };

  class EggCompilerFactory {
  public:
    static std::shared_ptr<IEggCompiler> createFromProgramBuilder(const egg::ovum::HardPtr<egg::ovum::IVMProgramBuilder>& builder);

    // Usually constructed via IEggCompiler::compile, but these are useful for testing simple modules
    static egg::ovum::HardPtr<egg::ovum::IVMProgram> compileFromStream(egg::ovum::IVM& vm, egg::ovum::TextStream& script);
    static egg::ovum::HardPtr<egg::ovum::IVMProgram> compileFromPath(egg::ovum::IVM& vm, const std::string& script, bool swallowBOM = true);
    static egg::ovum::HardPtr<egg::ovum::IVMProgram> compileFromText(egg::ovum::IVM& vm, const std::string& text, const std::string& resource = std::string());
  };
}
