namespace egg::yolk {
  class EggSyntaxNode {

  };

  class IEggParser {
  public:
    virtual std::shared_ptr<EggSyntaxNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  class EggParserFactory {
  public:
    static std::shared_ptr<IEggParser> create();
  };
}
