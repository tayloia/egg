namespace egg::yolk {
  class IEggTokenizer;

  class IEggParser {
  public:
    struct Location {
      size_t line = 0;
      size_t column = 0;
    };
    struct Token {
      enum class Kind {
        None,
        Integer,
        Float,
        String,
        Keyword,
        Operator,
        Identifier,
        Attribute,
        EndOfFile
      };
      Kind kind = Kind::None;
      union {
        int64_t ivalue;
        double fvalue;
      };
      egg::ovum::String svalue; // verbatim for integers and floats
      Location begin;
      Location end;
      bool contiguous = false; // true iff no whitespace/comments before previous
    };
    struct Issue {
      enum class Kind {
        Error,
        Warning,
        Information
      };
      egg::ovum::String message;
      Location begin;
      Location end;
    };
    struct Node {
      enum class Kind {
        ModuleRoot
      } kind;
      std::vector<Node> children;
      Token token;
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
