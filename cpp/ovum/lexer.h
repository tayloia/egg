// This is a context-free lexical analyser
// It is usually necessary to wrap this in a tokenizer
// to handle disambiguation such as "a--b"
namespace egg::ovum {
  class TextStream;

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

  struct LexerItem : public SourceLocation {
    LexerKind kind;
    LexerValue value;
    std::string verbatim;
  };

  class ILexer {
  public:
    virtual ~ILexer() {}
    virtual LexerKind next(LexerItem& item) = 0;
    virtual std::string getResourceName() const = 0;
  };

  class LexerFactory {
  public:
    static std::shared_ptr<ILexer> createFromPath(const std::filesystem::path& path, bool swallowBOM = true);
    static std::shared_ptr<ILexer> createFromString(const std::string& text, const std::string& resource = std::string());
    static std::shared_ptr<ILexer> createFromTextStream(TextStream& stream);
  };
}
