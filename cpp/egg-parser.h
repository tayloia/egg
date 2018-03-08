namespace egg::yolk {
  class IEggParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  class EggParserFactory {
  public:
    static std::shared_ptr<IEggParser> create(EggSyntaxNodeKind kind);
  };
}
