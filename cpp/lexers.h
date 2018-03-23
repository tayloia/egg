// This is a context-free lexical analyser.
// It is usually necessary to wrapper this in a tokenizer
// to handle disambiguition such as "a--b"
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
      uint64_t i;
      double f;
    };
    std::u32string s;
  };

  struct LexerItem : public ExceptionLocation {
    LexerKind kind;
    LexerValue value;
    std::string verbatim;
  };

  class ILexer {
  public:
    virtual LexerKind next(LexerItem& item) = 0;
    virtual std::string resource() const = 0;
  };

  class LexerFactory {
  public:
    static std::shared_ptr<ILexer> createFromPath(const std::string& path, bool swallowBOM = true);
    static std::shared_ptr<ILexer> createFromString(const std::string& text);
    static std::shared_ptr<ILexer> createFromTextStream(TextStream& stream);
  };
}
