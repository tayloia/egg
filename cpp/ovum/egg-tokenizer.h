#define EGG_TOKENIZER_KEYWORDS(macro) \
  macro(Any, "any") \
  macro(Bool, "bool") \
  macro(Break, "break") \
  macro(Case, "case") \
  macro(Catch, "catch") \
  macro(Continue, "continue") \
  macro(Default, "default") \
  macro(Do, "do") \
  macro(Else, "else") \
  macro(False, "false") \
  macro(Finally, "finally") \
  macro(Float, "float") \
  macro(For, "for") \
  macro(If, "if") \
  macro(Int, "int") \
  macro(Null, "null") \
  macro(Object, "object") \
  macro(Return, "return") \
  macro(Static, "static") \
  macro(String, "string") \
  macro(Switch, "switch") \
  macro(Throw, "throw") \
  macro(True, "true") \
  macro(Try, "try") \
  macro(Type, "type") \
  macro(Var, "var") \
  macro(Void, "void") \
  macro(While, "while") \
  macro(Yield, "yield")
#define EGG_TOKENIZER_KEYWORD_DECLARE(key, text) key,

#define EGG_TOKENIZER_OPERATORS(macro) \
  macro(Bang, "!") \
  macro(BangBang, "!!") \
  macro(BangBangEqual, "!!=") \
  macro(BangEqual, "!=") \
  macro(Percent, "%") \
  macro(PercentEqual, "%=") \
  macro(Ampersand, "&") \
  macro(AmpersandAmpersand, "&&") \
  macro(AmpersandAmpersandEqual, "&&=") \
  macro(AmpersandEqual, "&=") \
  macro(ParenthesisLeft, "(") \
  macro(ParenthesisRight, ")") \
  macro(Star, "*") \
  macro(StarEqual, "*=") \
  macro(Plus, "+") \
  macro(PlusPlus, "++") \
  macro(PlusEqual, "+=") \
  macro(Comma, ",") \
  macro(Minus, "-") \
  macro(MinusMinus, "--") \
  macro(MinusEqual, "-=") \
  macro(Lambda, "->") \
  macro(Dot, ".") \
  macro(Ellipsis, "...") \
  macro(Slash, "/") \
  macro(SlashEqual, "/=") \
  macro(Colon, ":") \
  macro(Semicolon, ";") \
  macro(Less, "<") \
  macro(ShiftLeft, "<<") \
  macro(ShiftLeftEqual, "<<=") \
  macro(LessEqual, "<=") \
  macro(LessBar, "<|") \
  macro(LessBarEqual, "<|=") \
  macro(Equal, "=") \
  macro(EqualEqual, "==") \
  macro(Greater, ">") \
  macro(GreaterEqual, ">=") \
  macro(ShiftRight, ">>") \
  macro(ShiftRightEqual, ">>=") \
  macro(ShiftRightUnsigned, ">>>") \
  macro(ShiftRightUnsignedEqual, ">>>=") \
  macro(GreaterBar, ">|") \
  macro(GreaterBarEqual, ">|=") \
  macro(Query, "?") \
  macro(QueryQuery, "??") \
  macro(QueryQueryEqual, "??=") \
  macro(BracketLeft, "[") \
  macro(BracketRight, "]") \
  macro(Caret, "^") \
  macro(CaretEqual, "^=") \
  macro(CurlyLeft, "{") \
  macro(Bar, "|") \
  macro(BarEqual, "|=") \
  macro(BarBar, "||") \
  macro(BarBarEqual, "||=") \
  macro(CurlyRight, "}") \
  macro(Tilde, "~")
#define EGG_TOKENIZER_OPERATOR_DECLARE(key, text) key,

namespace egg::ovum {
  class ILexer;

  enum class EggTokenizerKeyword {
    EGG_TOKENIZER_KEYWORDS(EGG_TOKENIZER_KEYWORD_DECLARE)
  };
  enum class EggTokenizerOperator {
    EGG_TOKENIZER_OPERATORS(EGG_TOKENIZER_OPERATOR_DECLARE)
  };

  enum class EggTokenizerKind {
    Integer,
    Float,
    String,
    Keyword,
    Operator,
    Identifier,
    Attribute,
    EndOfFile
  };

  struct EggTokenizerValue {
    union {
      Int i;
      Float f;
      EggTokenizerKeyword k;
      EggTokenizerOperator o;
    };
    String s; // verbatim for integers and floats

    static std::string getKeywordString(EggTokenizerKeyword value);
    static std::string getOperatorString(EggTokenizerOperator value);
    static bool tryParseKeyword(const std::string& text, EggTokenizerKeyword& value);
    static bool tryParseOperator(const std::string& text, EggTokenizerOperator& value, size_t& length);
  };

  struct EggTokenizerItem {
    EggTokenizerKind kind;
    EggTokenizerValue value;
    size_t line;
    size_t column;
    size_t width;
    bool contiguous;

    bool isKeyword(EggTokenizerKeyword keyword) const {
      return (this->kind == EggTokenizerKind::Keyword) && (this->value.k == keyword);
    }
    bool isOperator(EggTokenizerOperator op) const {
      return (this->kind == EggTokenizerKind::Operator) && (this->value.o == op);
    }
    bool isPropertyName() const {
      // Note we allow identifiers AND keywords
      return (this->kind == EggTokenizerKind::Identifier) || (this->kind == EggTokenizerKind::Keyword);
    }
    std::string toString() const;
  };

  class IEggTokenizer {
  public:
    virtual ~IEggTokenizer() {}
    virtual EggTokenizerKind next(EggTokenizerItem& item) = 0;
    virtual String resource() const = 0;
  };

  class EggTokenizerFactory {
  public:
    static std::shared_ptr<IEggTokenizer> createFromLexer(IAllocator& allocator, const std::shared_ptr<ILexer>& lexer);
  };
}
