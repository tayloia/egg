namespace egg::yolk {
  enum class EggedTokenizerKind {
    ObjectStart,
    ObjectEnd,
    ArrayStart,
    ArrayEnd,
    Null,
    Boolean,
    Integer,
    Float,
    String,
    Identifier,
    Colon,
    Comma,
    EndOfFile
  };

  struct EggedTokenizerItem {
    EggedTokenizerKind kind;
    egg::lang::Value value;
    size_t line;
    size_t column;
    bool contiguous;
  };

  class IEggedTokenizer {
  public:
    virtual ~IEggedTokenizer() {}
    virtual EggedTokenizerKind next(EggedTokenizerItem& item) = 0;
  };

  class EggedTokenizerFactory {
  public:
    static std::shared_ptr<IEggedTokenizer> createFromLexer(const std::shared_ptr<ILexer>& lexer);
  };
}
