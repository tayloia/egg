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
    std::string s;
  };

  struct JsonTokenizerItem {
    JsonTokenizerKind kind;
    JsonTokenizerValue value;
    size_t line;
    size_t column;
  };

  class IJsonTokenizer {
  public:
    virtual JsonTokenizerKind next(JsonTokenizerItem& item) = 0;
  };

  class JsonTokenizerFactory {
  public:
    static std::shared_ptr<IJsonTokenizer> createFromLexer(const std::shared_ptr<ILexer>& lexer);
    static std::shared_ptr<IJsonTokenizer> createFromPath(const std::string& path, bool swallowBOM = true) {
      return JsonTokenizerFactory::createFromLexer(LexerFactory::createFromPath(path, swallowBOM));
    }
    static std::shared_ptr<IJsonTokenizer> createFromString(const std::string& text) {
      return JsonTokenizerFactory::createFromLexer(LexerFactory::createFromString(text));
    }
    static std::shared_ptr<IJsonTokenizer> createFromTextStream(TextStream& stream) {
      return JsonTokenizerFactory::createFromLexer(LexerFactory::createFromTextStream(stream));
    }
  };
}
