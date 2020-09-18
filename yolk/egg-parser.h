#define EGG_PROGRAM_UNARY_OPERATORS(macro) \
  macro(LogicalNot, "!") \
  macro(Ref, "&") \
  macro(Deref, "*") \
  macro(Negate, "-") \
  macro(Ellipsis, "...") \
  macro(BitwiseNot, "~")
#define EGG_PROGRAM_UNARY_OPERATOR_DECLARE(name, text) name,

#define EGG_PROGRAM_BINARY_OPERATORS(macro) \
  macro(Unequal, "!=") \
  macro(Remainder, "%") \
  macro(BitwiseAnd, "&") \
  macro(LogicalAnd, "&&") \
  macro(Multiply, "*") \
  macro(Plus, "+") \
  macro(Minus, "-") \
  macro(Lambda, "->") \
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
  macro(BitwiseXor, "^") \
  macro(BitwiseOr, "|") \
  macro(LogicalOr, "||")
#define EGG_PROGRAM_BINARY_OPERATOR_DECLARE(name, text) name,

#define EGG_PROGRAM_TERNARY_OPERATORS(macro) \
  macro(Ternary, "?:")
#define EGG_PROGRAM_TERNARY_OPERATOR_DECLARE(name, text) name,

#define EGG_PROGRAM_ASSIGN_OPERATORS(macro) \
  macro(Remainder, "%=") \
  macro(BitwiseAnd, "&=") \
  macro(LogicalAnd, "&&=") \
  macro(Multiply, "*=") \
  macro(Plus, "+=") \
  macro(Minus, "-=") \
  macro(Divide, "/=") \
  macro(ShiftLeft, "<<=") \
  macro(Equal, "=") \
  macro(ShiftRight, ">>=") \
  macro(ShiftRightUnsigned, ">>>=") \
  macro(NullCoalescing, "??=") \
  macro(BitwiseXor, "^=") \
  macro(BitwiseOr, "|=") \
  macro(LogicalOr, "||=")
#define EGG_PROGRAM_ASSIGN_OPERATOR_DECLARE(name, text) name,

#define EGG_PROGRAM_MUTATE_OPERATORS(macro) \
  macro(Increment, "++") \
  macro(Decrement, "--")
#define EGG_PROGRAM_MUTATE_OPERATOR_DECLARE(name, text) name,

namespace egg::yolk {
  enum class EggParserAllowed {
    None = 0x00,
    Break = 0x01,
    Case = 0x02,
    Continue = 0x04,
    Empty = 0x08,
    Rethrow = 0x10,
    Return = 0x20,
    Yield = 0x40
  };

  class IEggParserContext {
  public:
    virtual ~IEggParserContext() {}
    virtual egg::ovum::IAllocator& getAllocator() const = 0;
    virtual egg::ovum::String getResourceName() const = 0;
    virtual bool isAllowed(EggParserAllowed allowed) const = 0;
    virtual EggParserAllowed inheritAllowed(EggParserAllowed allow, EggParserAllowed inherit) const = 0;
    virtual std::shared_ptr<IEggProgramNode> promote(const IEggSyntaxNode& node) = 0;
  };

  // Program parser
  class IEggParser {
  public:
    virtual ~IEggParser() {}
    virtual std::shared_ptr<IEggProgramNode> parse(egg::ovum::IAllocator& allocator, IEggTokenizer& tokenizer) = 0;
  };

  // Syntax parser (used internally and for testing)
  class IEggSyntaxParser {
  public:
    virtual ~IEggSyntaxParser() {}
    virtual std::shared_ptr<IEggSyntaxNode> parse(IEggTokenizer& tokenizer) = 0;
  };

  class EggParserFactory {
  public:
    // Syntax parser factories (used internally and for testing)
    static std::shared_ptr<IEggSyntaxParser> createModuleSyntaxParser(egg::ovum::IAllocator& allocator);
    static std::shared_ptr<IEggSyntaxParser> createStatementSyntaxParser(egg::ovum::IAllocator& allocator);
    static std::shared_ptr<IEggSyntaxParser> createExpressionSyntaxParser(egg::ovum::IAllocator& allocator);

    // All-in-one parser (used mainly for testing)
    static std::shared_ptr<IEggProgramNode> parseModule(egg::ovum::IAllocator& allocator, TextStream& stream);

    // AST parsers
    static std::shared_ptr<IEggParser> createModuleParser();
    static std::shared_ptr<IEggParser> createExpressionParser();
  };
}
