// TODO legacy lexer
namespace egg0 {
  struct LexerLocation {
    size_t line;
    size_t column;
  };

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
    LexerLocation start;
    LexerLocation finish;
    std::string verbatim;
  };

  class ILexer {
  public:
    virtual LexerKind next(LexerItem& item) = 0;
  };

  namespace LexerFactory {
    std::shared_ptr<ILexer> createFromStream(std::istream& stream, const std::string& filename);
  };
}
