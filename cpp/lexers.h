namespace egg::yolk {
  enum class LexerKind {
    Whitespace,
    Comment,
    Integer,
    Float,
    String,
    Operator,
    Identifier,
    EndOfFile
  };

  struct LexerValue {
    union {
      int64_t i;
      double f;
    };
    std::u32string s;
  };

  struct LexerItem {
    LexerKind kind;
    LexerValue value;
    size_t line;
    size_t column;
    std::string verbatim;
  };

  class ILexer {
  public:
    virtual LexerKind next(LexerItem& item) = 0;
  };

  class LexerFactory {
  public:
    static std::shared_ptr<ILexer> createFromPath(const std::string& path, bool swallowBOM = true);
    static std::shared_ptr<ILexer> createFromString(const std::string& text);
    static std::shared_ptr<ILexer> createFromTextStream(TextStream& stream);
  };
}
