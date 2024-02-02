namespace egg::yolk {
  class IEggTokenizer;

  class IEggParser {
  public:
    struct Issue {
      enum class Severity {
        Error,
        Warning,
        Information
      };
      Severity severity;
      egg::ovum::String message;
      egg::ovum::SourceRange range;
    };
    struct Node {
      enum class Kind {
        ModuleRoot,
        StmtBlock,
        StmtDeclareVariable,
        StmtDefineVariable,
        StmtDefineFunction,
        StmtForEach,
        StmtForLoop,
        StmtIf,
        StmtReturn,
        StmtYield,
        StmtThrow,
        StmtTry,
        StmtCatch,
        StmtFinally,
        StmtWhile,
        StmtDo,
        StmtSwitch,
        StmtCase,
        StmtDefault,
        StmtBreak,
        StmtContinue,
        StmtMutate,
        ExprVariable,
        ExprUnary,
        ExprBinary,
        ExprTernary,
        ExprCall,
        ExprIndex,
        ExprProperty,
        ExprReference,
        ExprDereference,
        ExprArray,
        ExprObject,
        ExprGuard,
        TypeInfer,
        TypeInferQ,
        TypeVoid,
        TypeBool,
        TypeInt,
        TypeFloat,
        TypeString,
        TypeObject,
        TypeAny,
        TypeType,
        TypeUnary,
        TypeBinary,
        TypeFunctionSignature,
        TypeFunctionSignatureParameter,
        Literal,
        Named
      };
      enum class FunctionOp {
        Function,
        Generator
      };
      enum class ParameterOp {
        Required,
        Optional
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
        FunctionOp functionOp;
        ParameterOp parameterOp;
      } op;
      egg::ovum::SourceRange range;
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
