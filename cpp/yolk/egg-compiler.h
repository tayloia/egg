namespace egg::yolk {
  class IEggTokenizer;

  class IEggCompiler {
  public:
    virtual ~IEggCompiler() {}
    virtual bool compile() = 0;
    virtual egg::ovum::String resource() const = 0;
  };

  class EggCompilerFactory {
  public:
    static std::shared_ptr<IEggCompiler> createFromTokenizer(egg::ovum::IVM& vm, const std::shared_ptr<IEggTokenizer>& tokenizer);
  };
}
