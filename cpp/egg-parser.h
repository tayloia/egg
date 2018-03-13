#define EGG_PARSER_UNARY_OPERATORS(macro) \
  macro(LogicalNot, "!") \
  macro(Ref, "&") \
  macro(Deref, "*") \
  macro(Negate, "-") \
  macro(Ellipsis, "...") \
  macro(BitwiseNot, "~")
#define EGG_PARSER_UNARY_OPERATOR_DECLARE(name, text) name,

#define EGG_PARSER_BINARY_OPERATORS(macro) \
  macro(Remainder, "%") \
  macro(BitwiseAnd, "&") \
  macro(LogicalAnd, "&&") \
  macro(Multiply, "*") \
  macro(Plus, "+") \
  macro(Minus, "-") \
  macro(Lambda, "->") \
  macro(Dot, ".") \
  macro(Divide, "/") \
  macro(Less, "<") \
  macro(ShiftLeft, "<<") \
  macro(LessEqual, "<=") \
  macro(Equal, "==") \
  macro(Greater, ">") \
  macro(GreaterEqual, ">=") \
  macro(ShiftRight, ">>") \
  macro(ShiftRightUnsigned, ">>>") \
  macro(NullCoalescing, "??") \
  macro(Brackets, "[]") \
  macro(BitwiseXor, "^") \
  macro(BitwiseOr, "|") \
  macro(LogicalOr, "||")
#define EGG_PARSER_BINARY_OPERATOR_DECLARE(name, text) name,

namespace egg::yolk {
  class EggParserType {
    using Tag = egg::lang::VariantTag;
  private:
    Tag tag;
  public:
    explicit EggParserType(Tag tag) : tag(tag) {
    }
    std::string tagToString() const {
      return EggParserType::tagToString(this->tag);
    }
    static std::string tagToString(Tag tag);
  };

  class IEggParserNode {
  public:
    virtual const EggParserType& getType() const = 0;
    virtual void dump(std::ostream& os) const = 0;
  };

  enum class EggParserUnary {
    EGG_PARSER_UNARY_OPERATORS(EGG_PARSER_UNARY_OPERATOR_DECLARE)
  };

  enum class EggParserBinary {
    EGG_PARSER_BINARY_OPERATORS(EGG_PARSER_BINARY_OPERATOR_DECLARE)
  };

  enum class EggParserAllowed {
    Empty = 0x01,
    Break = 0x02,
    Continue = 0x04,
    Default = 0x08
  };

  class IEggParserContext {
  public:
    virtual bool isAllowed(EggParserAllowed allowed) = 0;
  };

  // Program parser
  class IEggParser {
  public:
    virtual std::shared_ptr<IEggParserNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  // Syntax parser (used internally and for testing)
  class IEggSyntaxParser {
  public:
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  class EggParserFactory {
  public:
    // Syntax parser factories (used internally and for testing)
    static std::shared_ptr<IEggSyntaxParser> createModuleSyntaxParser();
    static std::shared_ptr<IEggSyntaxParser> createStatementSyntaxParser();
    static std::shared_ptr<IEggSyntaxParser> createExpressionSyntaxParser();

    // AST parsers
    static std::shared_ptr<IEggParser> createModuleParser();
  };
}
