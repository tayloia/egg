namespace egg::yolk {
  class IEggParser;
  class IEggModule;

  class IEggCompiler {
  public:
    virtual ~IEggCompiler() {}
    virtual std::shared_ptr<IEggModule> compile() = 0;
    virtual egg::ovum::String resource() const = 0;
  };

  class EggCompilerFactory {
  public:
    static std::shared_ptr<IEggCompiler> createFromParser(egg::ovum::IVM& vm, const std::shared_ptr<IEggParser>& parser);
  };
}
