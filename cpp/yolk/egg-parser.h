namespace egg::yolk {
  class IEggTokenizer;

  class IEggParser {
  public:
    struct Location {
      size_t line = 0;
      size_t column = 0;
    };
    struct Issue {
      enum class Severity {
        Error,
        Warning,
        Information
      };
      Severity severity;
      egg::ovum::String message;
      Location begin;
      Location end;
    };
    struct Node {
      enum class Kind {
        ModuleRoot,
        StmtDeclareVar,
        StmtDefineVar,
        StmtCall,
        ExprVar,
        ExprUnary,
        ExprBinary,
        ExprTernary,
        ExprCall,
        ExprTypePrimitive,
        Literal
      };
      Kind kind;
      std::vector<std::unique_ptr<Node>> children;
      egg::ovum::HardValue value;
      union {
        egg::ovum::ValueUnaryOp unary;
        egg::ovum::ValueBinaryOp binary;
        egg::ovum::ValueTernaryOp ternary;
        egg::ovum::ValueMutationOp mutation;
        egg::ovum::TypeBinaryOp tbinary;
        egg::ovum::ValueFlags primitive;
      } op;
      Location begin;
      Location end;
    };
    struct Result {
      std::shared_ptr<Node> root;
      std::vector<Issue> issues;
    };
  public:
    virtual ~IEggParser() {}
    virtual Result parse() = 0;
    virtual egg::ovum::String resource() const = 0;
  };

  class EggParserFactory {
  public:
    static std::shared_ptr<IEggParser> createFromTokenizer(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggTokenizer>& tokenizer);
  };
}
