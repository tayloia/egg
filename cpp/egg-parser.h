namespace egg::yolk {
  class IEggParserNode;

  class IEggParserNode {
  public:
    virtual void dump(std::ostream& os) const = 0;
  };

  // Program parser
  class IEggParser {
  public:
    virtual std::shared_ptr<IEggParserNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  // Syntax parser (used internally and for testing)
  class IEggSyntaxParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  class EggParserFactory {
  public:
    // Syntax parser factories (used internally and for testing)
    static std::shared_ptr<IEggSyntaxParser> createModuleSyntaxParser();
    static std::shared_ptr<IEggSyntaxParser> createStatementSyntaxParser();
    static std::shared_ptr<IEggSyntaxParser> createExpressionSyntaxParser();

    // AST parsers
    static std::shared_ptr<IEggParser> createModuleParser();
  };
}
