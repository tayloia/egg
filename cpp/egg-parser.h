namespace egg::yolk {
  class IEggSyntaxParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  class EggParserFactory {
  public:
    static std::shared_ptr<IEggSyntaxParser> createModuleSyntaxParser();
    static std::shared_ptr<IEggSyntaxParser> createStatementSyntaxParser();
    static std::shared_ptr<IEggSyntaxParser> createExpressionSyntaxParser();
  };
}
