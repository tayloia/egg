namespace egg::yolk {
  enum class EggSyntaxNodeKind {
    Module
  };

  class IEggSyntaxNode {
  public:
    virtual EggSyntaxNodeKind getKind() const = 0;
    virtual size_t getChildren() const = 0;
    virtual IEggSyntaxNode& getChild(size_t index) const = 0;
  };

  class IEggParser {
  public:
    virtual std::shared_ptr<const IEggSyntaxNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  class EggParserFactory {
  public:
    static std::shared_ptr<IEggParser> create(EggSyntaxNodeKind kind);
  };
}
