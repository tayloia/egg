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
  macro(Function, "function") \
  macro(If, "if") \
  macro(Int, "int") \
  macro(Null, "null") \
  macro(Object, "object") \
  macro(Return, "return") \
  macro(String, "string") \
  macro(Switch, "switch") \
  macro(True, "true") \
  macro(Try, "try") \
  macro(Type, "type") \
  macro(Typedef, "typedef") \
  macro(Var, "var") \
  macro(Void, "void") \
  macro(While, "while") \
  macro(Yield, "yield")
#define EGG_TOKENIZER_KEYWORD_DECLARE(key, text) key,

#define EGG_TOKENIZER_OPERATORS(macro) \
  macro(Bang, "!") \
  macro(BangEqual, "!=") \
  macro(Percent, "%") \
  macro(PercentEqual, "%=") \
  macro(Ampersand, "&") \
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
  macro(SlahEqual, "/=") \
  macro(Colon, ":") \
  macro(Semicolon, ";") \
  macro(Less, "<") \
  macro(ShiftLeft, "<<") \
  macro(ShiftLeftEqual, "<<=") \
  macro(LessEqual, "<=") \
  macro(Equal, "=") \
  macro(EqualEqual, "==") \
  macro(Greater, ">") \
  macro(GreaterEqual, ">=") \
  macro(ShiftRight, ">>") \
  macro(ShiftRightEqual, ">>=") \
  macro(ShiftRightUnsigned, ">>>") \
  macro(ShiftRightUnsignedEqual, ">>>=") \
  macro(Query, "?") \
  macro(QueryQuery, "??") \
  macro(BracketLeft, "[") \
  macro(BracketRight, "]") \
  macro(Caret, "^") \
  macro(CaretEqual, "^=") \
  macro(CurlyLeft, "{") \
  macro(Bar, "|") \
  macro(BarEqual, "|=") \
  macro(CurlyRight, "}") \
  macro(Tilde, "~")
#define EGG_TOKENIZER_OPERATOR_DECLARE(key, text) key,

namespace egg::yolk {
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
      int64_t i;
      double f;
      EggTokenizerKeyword k;
      EggTokenizerOperator o;
    };
    std::string s;
  };

  struct EggTokenizerItem {
    EggTokenizerKind kind;
    EggTokenizerValue value;
    size_t line;
    size_t column;
    bool contiguous;
  };

  struct EggTokenizerState {
    bool expectIncrement : 1;
    bool expectNumberLiteral : 1;

    static const EggTokenizerState ExpectNone;
    static const EggTokenizerState ExpectIncrement;
    static const EggTokenizerState ExpectNumberLiteral;
  };

  class IEggTokenizer {
  public:
    virtual EggTokenizerKind next(EggTokenizerItem& item, const EggTokenizerState& state) = 0;
  };

  class EggTokenizerFactory {
  public:
    static std::shared_ptr<IEggTokenizer> createFromLexer(const std::shared_ptr<ILexer>& lexer);
    static std::shared_ptr<IEggTokenizer> createFromPath(const std::string& path, bool swallowBOM = true) {
      return EggTokenizerFactory::createFromLexer(LexerFactory::createFromPath(path, swallowBOM));
    }
    static std::shared_ptr<IEggTokenizer> createFromString(const std::string& text) {
      return EggTokenizerFactory::createFromLexer(LexerFactory::createFromString(text));
    }
    static std::shared_ptr<IEggTokenizer> createFromTextStream(TextStream& stream) {
      return EggTokenizerFactory::createFromLexer(LexerFactory::createFromTextStream(stream));
    }
  };
}
