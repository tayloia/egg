namespace egg::yolk {
  enum class JsonTokenizerKind {
    ObjectStart,
    ObjectEnd,
    ArrayStart,
    ArrayEnd,
    Null,
    Boolean,
    Unsigned,
    Signed,
    Float,
    String,
    Colon,
    Comma,
    EndOfFile
  };

  struct JsonTokenizerValue {
    union {
      bool b;
      uint64_t u;
      int64_t i;
      double f;
    };
    std::string s; // utf8
  };

  struct JsonTokenizerItem {
    JsonTokenizerKind kind;
    JsonTokenizerValue value;
    size_t line;
    size_t column;
  };

  class IJsonTokenizer {
  public:
    virtual ~IJsonTokenizer() {}
    virtual JsonTokenizerKind next(JsonTokenizerItem& item) = 0;
  };

  class JsonTokenizerFactory {
  public:
    static std::shared_ptr<IJsonTokenizer> createFromLexer(const std::shared_ptr<egg::ovum::ILexer>& lexer);
  };
}
