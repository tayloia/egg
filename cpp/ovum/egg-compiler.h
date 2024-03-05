namespace egg::ovum {
  class IEggParser;
  class TextStream;

  class IEggCompiler {
  public:
    virtual ~IEggCompiler() {}
    virtual HardPtr<IVMModule> compile(IEggParser& parser) = 0;
  };

  class EggCompilerFactory {
  public:
    static std::shared_ptr<IEggCompiler> createFromProgramBuilder(const HardPtr<IVMProgramBuilder>& builder);

    // Usually constructed via IEggCompiler::compile, but these are useful for testing simple modules
    static HardPtr<IVMProgram> compileFromStream(IVM& vm, TextStream& script);
    static HardPtr<IVMProgram> compileFromPath(IVM& vm, const std::string& script, bool swallowBOM = true);
    static HardPtr<IVMProgram> compileFromText(IVM& vm, const std::string& text, const std::string& resource = std::string());
  };
}
