namespace egg::yolk {
  enum class EonTokenizerKind {
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

  struct EonTokenizerItem {
    EonTokenizerKind kind;
    egg::ovum::HardValue value;
    size_t line;
    size_t column;
    bool contiguous;
  };

  class IEonTokenizer {
  public:
    virtual ~IEonTokenizer() {}
    virtual EonTokenizerKind next(EonTokenizerItem& item) = 0;
  };

  class EonTokenizerFactory {
  public:
    static std::shared_ptr<IEonTokenizer> createFromLexer(egg::ovum::IAllocator& allocator, const std::shared_ptr<ILexer>& lexer);
  };
}
