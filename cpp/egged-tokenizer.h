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
  };

  class IEggedTokenizer {
  public:
    virtual EggedTokenizerKind next(EggedTokenizerItem& item) = 0;
  };

  class EggedTokenizerFactory {
  public:
    static std::shared_ptr<IEggedTokenizer> createFromLexer(const std::shared_ptr<ILexer>& lexer);
    static std::shared_ptr<IEggedTokenizer> createFromPath(const std::string& path, bool swallowBOM = true) {
      return EggedTokenizerFactory::createFromLexer(LexerFactory::createFromPath(path, swallowBOM));
    }
    static std::shared_ptr<IEggedTokenizer> createFromString(const std::string& text) {
      return EggedTokenizerFactory::createFromLexer(LexerFactory::createFromString(text));
    }
    static std::shared_ptr<IEggedTokenizer> createFromTextStream(TextStream& stream) {
      return EggedTokenizerFactory::createFromLexer(LexerFactory::createFromTextStream(stream));
    }
  };
}
