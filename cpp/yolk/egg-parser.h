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
        StmtDefineType,
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
        ExprUnary,
        ExprBinary,
        ExprTernary,
        ExprCall,
        ExprIndex,
        ExprProperty,
        ExprReference,
        ExprDereference,
        ExprArray,
        ExprEon,
        ExprObject,
        ExprEllipsis,
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
        TypeSpecification,
        TypeSpecificationStaticData,
        TypeSpecificationStaticFunction,
        TypeSpecificationInstanceData,
        TypeSpecificationInstanceFunction,
        TypeSpecificationAccess,
        ObjectSpecification,
        ObjectSpecificationData,
        ObjectSpecificationFunction,
        Literal,
        Variable,
        Named,
        Missing
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
        egg::ovum::Accessability accessability;
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
