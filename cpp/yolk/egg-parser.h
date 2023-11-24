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
        StmtDeclareVariable,
        StmtDefineVariable,
        StmtCall,
        ExprVariable,
        ExprUnary,
        ExprBinary,
        ExprTernary,
        ExprCall,
        TypeInfer,
        TypeInferQ,
        TypeVoid,
        TypeBool,
        TypeInt,
        TypeFloat,
        TypeString,
        TypeObject,
        TypeAny,
        TypeUnary,
        TypeBinary,
        Literal
      };
      Kind kind;
      std::vector<std::unique_ptr<Node>> children;
      egg::ovum::HardValue value;
      union {
        egg::ovum::ValueUnaryOp valueUnaryOp;
        egg::ovum::ValueBinaryOp valueBinaryOp;
        egg::ovum::ValueTernaryOp valueTernaryOp;
        egg::ovum::ValueMutationOp valueMutationOp;
        egg::ovum::TypeUnaryOp typeUnaryOp;
        egg::ovum::TypeBinaryOp typeBinaryOp;
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
