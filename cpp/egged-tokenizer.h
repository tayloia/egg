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

  struct EggedTokenizerValue {
    union {
      bool b;
      int64_t i;
      double f;
    };
    std::string s;
  };

  struct EggedTokenizerItem {
    EggedTokenizerKind kind;
    EggedTokenizerValue value;
    size_t line;
    size_t column;
    bool contiguous;
  };

  class IEggedTokenizer {
  public:
    virtual EggedTokenizerKind next(EggedTokenizerItem& item) = 0;
  };

  class EggedTokenizerFactory {
  public:
    static std::shared_ptr<IEggedTokenizer> createFromLexer(const std::shared_ptr<ILexer>& lexer);
  };
}
