namespace egg::yolk {
  enum class LexerKind {
    Whitespace,
    Comment,
    Int,
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
      bool valid;
    };
    std::string s;
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
    static std::shared_ptr<ILexer> createFromTextStream(TextStream& stream);
  };
}
