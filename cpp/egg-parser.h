#define EGG_PARSER_UNARY_OPERATORS(macro) \
  macro(LogicalNot, "!") \
  macro(Ref, "&") \
  macro(Deref, "*") \
  macro(Negate, "-") \
  macro(Ellipsis, "...") \
  macro(BitwiseNot, "~")

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

namespace egg::yolk {
  class EggParserType {
  public:
    enum class Simple {
      Void = 0x00,
      Null = 0x01,
      Bool = 0x02,
      Int = 0x04,
      Float = 0x08,
      String = 0x10,
      Object = 0x20
    };
    class Complex;
  private:
    Simple simple;
  public:
    explicit EggParserType(Simple simple) : simple(simple) {
    }
  };

  class IEggParserNode {
  public:
    virtual const EggParserType& getType() const = 0;
    virtual void dump(std::ostream& os) const = 0;
  };

  enum class EggParserAllowed {
    Empty = 0x01,
    Block = 0x02,
    Break = 0x04,
    Continue = 0x08,
    Default = 0x10
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
