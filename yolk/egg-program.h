#include <map>

namespace egg::yolk {
  class EggProgramContext;
  class EggProgramStackless;
  class EggProgramSymbolTable;
  class EggProgramCompiler;

  enum class EggProgramNodeFlags {
    None = 0x00,
    Constant = 0x01,
    Predicate = 0x02,
    Variadic = 0x04,
    Fallthrough = 0x08,
    Abandon = 0x80
  };

  class IEggProgramNode {
  public:
    virtual ~IEggProgramNode() {}
    virtual egg::ovum::Type getType() const = 0;
    virtual egg::ovum::LocationSource location() const = 0;
    virtual bool symbol(egg::ovum::String& nameOut, egg::ovum::Type& typeOut) const = 0;
    virtual void empredicate(EggProgramContext& context, std::shared_ptr<IEggProgramNode>& ptr) = 0;
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) = 0;
    virtual void dump(std::ostream& os) const = 0;
    virtual egg::ovum::Node compile(EggProgramCompiler& compiler) const = 0;
  };

  enum class EggProgramUnary {
    EGG_PROGRAM_UNARY_OPERATORS(EGG_PROGRAM_UNARY_OPERATOR_DECLARE)
  };

  enum class EggProgramBinary {
    EGG_PROGRAM_BINARY_OPERATORS(EGG_PROGRAM_BINARY_OPERATOR_DECLARE)
  };

  enum class EggProgramTernary {
    EGG_PROGRAM_TERNARY_OPERATORS(EGG_PROGRAM_TERNARY_OPERATOR_DECLARE)
  };

  enum class EggProgramAssign {
    EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PROGRAM_ASSIGN_OPERATOR_DECLARE)
  };

  enum class EggProgramMutate {
    EGG_PROGRAM_MUTATE_OPERATORS(EGG_PROGRAM_ASSIGN_OPERATOR_DECLARE)
  };

  class EggProgramSymbol final {
  public:
    enum class Kind { Builtin, Readonly, ReadWrite };
  private:
    Kind kind;
    egg::ovum::String name;
    egg::ovum::Type type;
  public:
    EggProgramSymbol(Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type)
      : kind(kind), name(name), type(type) {
    }
    const egg::ovum::String& getName() const { return this->name; }
    const egg::ovum::IType& getType() const { return *this->type; }
    void setInferredType(const egg::ovum::Type& inferred);
  };

  class EggProgramSymbolTable final : public egg::ovum::HardReferenceCounted<egg::ovum::IHardAcquireRelease> {
    EGG_NO_COPY(EggProgramSymbolTable);
  private:
    std::map<egg::ovum::String, std::shared_ptr<EggProgramSymbol>> map;
    egg::ovum::HardPtr<EggProgramSymbolTable> parent;
  public:
    explicit EggProgramSymbolTable(egg::ovum::IAllocator& allocator, EggProgramSymbolTable* parent = nullptr)
      : HardReferenceCounted(allocator, 0),
        map(),
        parent(parent) {
    }
    void addBuiltins();
    void addBuiltin(const std::string& name, const egg::ovum::Value& value);
    std::shared_ptr<EggProgramSymbol> addSymbol(EggProgramSymbol::Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type);
    std::shared_ptr<EggProgramSymbol> findSymbol(const egg::ovum::String& name, bool includeParents = true) const;
  };

  class EggProgram final {
    EGG_NO_COPY(EggProgram);
  private:
    egg::ovum::Basket basket;
    egg::ovum::String resource;
    std::shared_ptr<IEggProgramNode> root;
  public:
    EggProgram(egg::ovum::IAllocator& allocator, const egg::ovum::String& resource, const std::shared_ptr<IEggProgramNode>& root)
      : basket(egg::ovum::BasketFactory::createBasket(allocator)),
        resource(resource),
        root(root) {
      assert(root != nullptr);
    }
    ~EggProgram() {
      this->root.reset();
      (void)this->basket->collect();
      // The destructor for 'basket' will assert if this collection doesn't free up everything in the basket
    }
    egg::ovum::HardPtr<EggProgramContext> createRootContext(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity& maximumSeverity);
    egg::ovum::ILogger::Severity prepare(IEggEngineContext& preparation);
    egg::ovum::ILogger::Severity compile(IEggEngineContext& compilation, egg::ovum::Module& out);
    egg::ovum::ILogger::Severity execute(IEggEngineContext& execution, const egg::ovum::Module& module);
    static std::string unaryToString(EggProgramUnary op);
    static std::string binaryToString(EggProgramBinary op);
    static std::string assignToString(EggProgramAssign op);
    static std::string mutateToString(EggProgramMutate op);
    enum class ArithmeticTypes {
      None, Int, Float, Both
    };
    static ArithmeticTypes arithmeticTypes(const egg::ovum::Type& type) {
      assert(type != nullptr);
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (egg::ovum::Bits::mask(type->getPrimitiveFlags(), egg::ovum::ValueFlags::Arithmetic)) {
      case egg::ovum::ValueFlags::Int:
        return ArithmeticTypes::Int;
      case egg::ovum::ValueFlags::Float:
        return ArithmeticTypes::Float;
      case egg::ovum::ValueFlags::Arithmetic:
        return ArithmeticTypes::Both;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END();
      return ArithmeticTypes::None;
    }
    static const egg::ovum::IType& VanillaArray;
    static const egg::ovum::IType& VanillaObject;
  };

  class EggProgramContext final : public egg::ovum::HardReferenceCounted<egg::ovum::IHardAcquireRelease> {
    EGG_NO_COPY(EggProgramContext);
  public:
    struct ScopeFunction {
      const egg::ovum::IType* rettype;
      bool generator;
    };
  private:
    egg::ovum::LocationRuntime location;
    egg::ovum::ILogger* logger;
    egg::ovum::HardPtr<EggProgramSymbolTable> symtable;
    egg::ovum::ILogger::Severity* maximumSeverity;
    const egg::ovum::IType* scopeDeclare; // Only used in prepare phase
    ScopeFunction* scopeFunction; // Only used in prepare phase
    const egg::ovum::Value* scopeValue; // Only used in execute phase
    EggProgramContext(egg::ovum::IAllocator& allocator, const egg::ovum::LocationRuntime& location, egg::ovum::ILogger* logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity* maximumSeverity, ScopeFunction* scopeFunction)
      : HardReferenceCounted(allocator, 0),
        location(location),
        logger(logger),
        symtable(&symtable),
        maximumSeverity(maximumSeverity),
        scopeDeclare(nullptr),
        scopeFunction(scopeFunction),
        scopeValue(nullptr) {
    }
  public:
    EggProgramContext(egg::ovum::IAllocator& allocator, EggProgramContext& parent, EggProgramSymbolTable& symtable, ScopeFunction* scopeFunction)
      : EggProgramContext(allocator, parent.location, parent.logger, symtable, parent.maximumSeverity, scopeFunction) {
    }
    EggProgramContext(egg::ovum::IAllocator& allocator, const egg::ovum::LocationRuntime& location, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity& maximumSeverity)
      : EggProgramContext(allocator, location, &logger, symtable, &maximumSeverity, nullptr) {
    }
    egg::ovum::HardPtr<EggProgramContext> createNestedContext(EggProgramSymbolTable& symtable, ScopeFunction* prepareFunction = nullptr);
    void log(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, const std::string& message);
    template<typename... ARGS>
    void compilerProblem(egg::ovum::ILogger::Severity severity, const egg::ovum::LocationSource& location, ARGS&&... args) {
      egg::ovum::StringBuilder sb;
      if (location.printSource(sb)) {
        sb << ':' << ' ';
      }
      sb.add(std::forward<ARGS>(args)...);
      this->log(egg::ovum::ILogger::Source::Compiler, severity, sb.toUTF8());
    }
    template<typename... ARGS>
    void compilerWarning(const egg::ovum::LocationSource& location, ARGS&&... args) {
      this->compilerProblem(egg::ovum::ILogger::Severity::Warning, location, std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    EggProgramNodeFlags compilerError(const egg::ovum::LocationSource& location, ARGS&&... args) {
      this->compilerProblem(egg::ovum::ILogger::Severity::Error, location, std::forward<ARGS>(args)...);
      return EggProgramNodeFlags::Abandon;
    }

    // Implemented in egg-prepare.cpp
    std::shared_ptr<IEggProgramNode> empredicateBinary(const std::shared_ptr<IEggProgramNode>& node, EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs);
    EggProgramNodeFlags prepareModule(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareBlock(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareDeclare(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::Type& ltype, IEggProgramNode* rvalue);
    EggProgramNodeFlags prepareGuard(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::Type& ltype, IEggProgramNode& rvalue);
    EggProgramNodeFlags prepareAssign(const egg::ovum::LocationSource& where, EggProgramAssign op, IEggProgramNode& lvalue, IEggProgramNode& rvalue);
    EggProgramNodeFlags prepareMutate(const egg::ovum::LocationSource& where, EggProgramMutate op, IEggProgramNode& lvalue);
    EggProgramNodeFlags prepareCatch(const egg::ovum::String& name, IEggProgramNode& type, IEggProgramNode& block);
    EggProgramNodeFlags prepareDo(IEggProgramNode& cond, IEggProgramNode& block);
    EggProgramNodeFlags prepareIf(IEggProgramNode& cond, IEggProgramNode& trueBlock, IEggProgramNode* falseBlock);
    EggProgramNodeFlags prepareFor(IEggProgramNode* pre, IEggProgramNode* cond, IEggProgramNode* post, IEggProgramNode& block);
    EggProgramNodeFlags prepareForeach(IEggProgramNode& lvalue, IEggProgramNode& rvalue, IEggProgramNode& block);
    EggProgramNodeFlags prepareFunctionDefinition(const egg::ovum::String& name, const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& block);
    EggProgramNodeFlags prepareGeneratorDefinition(const egg::ovum::Type& rettype, const std::shared_ptr<IEggProgramNode>& block);
    EggProgramNodeFlags prepareReturn(const egg::ovum::LocationSource& where, IEggProgramNode* value);
    EggProgramNodeFlags prepareCase(const std::vector<std::shared_ptr<IEggProgramNode>>& values, IEggProgramNode& block);
    EggProgramNodeFlags prepareSwitch(IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases);
    EggProgramNodeFlags prepareThrow(IEggProgramNode* exception);
    EggProgramNodeFlags prepareTry(IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, IEggProgramNode* final);
    EggProgramNodeFlags prepareWhile(IEggProgramNode& cond, IEggProgramNode& block);
    EggProgramNodeFlags prepareYield(const egg::ovum::LocationSource& where, IEggProgramNode& value);
    EggProgramNodeFlags prepareArray(const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    EggProgramNodeFlags prepareObject(const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    EggProgramNodeFlags prepareCall(IEggProgramNode& callee, std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    EggProgramNodeFlags prepareIdentifier(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::Type& type);
    EggProgramNodeFlags prepareBrackets(const egg::ovum::LocationSource& where, IEggProgramNode& instance, IEggProgramNode& index);
    EggProgramNodeFlags prepareDot(const egg::ovum::LocationSource& where, IEggProgramNode& instance, const egg::ovum::String& property);
    EggProgramNodeFlags prepareUnary(const egg::ovum::LocationSource& where, EggProgramUnary op, IEggProgramNode& value);
    EggProgramNodeFlags prepareBinary(const egg::ovum::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs);
    EggProgramNodeFlags prepareTernary(const egg::ovum::LocationSource& where, IEggProgramNode& cond, IEggProgramNode& whenTrue, IEggProgramNode& whenFalse);
    EggProgramNodeFlags preparePredicate(const egg::ovum::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs);
    // Temporary scope modifiers
    EggProgramNodeFlags prepareWithType(IEggProgramNode& node, const egg::ovum::Type& type);
  private:
    bool findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareScope(const IEggProgramNode* node, std::function<EggProgramNodeFlags(EggProgramContext&)> action);
    EggProgramNodeFlags prepareStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags typeCheck(const egg::ovum::LocationSource& where, egg::ovum::Type& ltype, const egg::ovum::Type& rtype, const egg::ovum::String& name, bool guard);
  };

  class EggProgramCompilerNode {
    EggProgramCompilerNode(const EggProgramCompilerNode&) = delete;
    EggProgramCompilerNode& operator=(const EggProgramCompilerNode&) = delete;
  private:
    EggProgramCompiler& compiler;
    egg::ovum::NodeLocation location;
    egg::ovum::Opcode opcode;
    egg::ovum::Nodes nodes;
    bool failed;
  public:
    EggProgramCompilerNode(EggProgramCompiler& compiler, const egg::ovum::LocationSource& location, egg::ovum::Opcode opcode)
      : compiler(compiler), opcode(opcode), nodes(), failed(false) {
      this->location.line = location.line;
      this->location.column = location.column;
    }
    EggProgramCompilerNode& add(const egg::ovum::Node& child);
    EggProgramCompilerNode& add(const std::shared_ptr<IEggProgramNode>& child);
    EggProgramCompilerNode& add(const std::vector<std::shared_ptr<IEggProgramNode>>& children);
    EggProgramCompilerNode& add(const IEggProgramNode& child);
    template<typename T, typename... ARGS>
    EggProgramCompilerNode& add(const T& value, ARGS&&... args) {
      return this->add(value).add(std::forward<ARGS>(args)...);
    }
    egg::ovum::Node build();
    egg::ovum::Node build(egg::ovum::Operator operand);
  };

  class EggProgramCompiler {
    EGG_NO_COPY(EggProgramCompiler);
    friend class EggProgramCompilerNode;
  private:
    IEggEngineContext& context;
  public:
    explicit EggProgramCompiler(IEggEngineContext& context) : context(context) {
    }
    egg::ovum::Node opcode(const egg::ovum::LocationSource& location, egg::ovum::Opcode value);
    egg::ovum::Node ivalue(const egg::ovum::LocationSource& location, egg::ovum::Int value);
    egg::ovum::Node fvalue(const egg::ovum::LocationSource& location, egg::ovum::Float value);
    egg::ovum::Node svalue(const egg::ovum::LocationSource& location, const egg::ovum::String& value);
    egg::ovum::Node type(const egg::ovum::LocationSource& location, const egg::ovum::Type& type);
    egg::ovum::Node identifier(const egg::ovum::LocationSource& location, const egg::ovum::String& id);
    egg::ovum::Node unary(const egg::ovum::LocationSource& location, EggProgramUnary op, const IEggProgramNode& a);
    egg::ovum::Node binary(const egg::ovum::LocationSource& location, EggProgramBinary op, const IEggProgramNode& a, const IEggProgramNode& b);
    egg::ovum::Node ternary(const egg::ovum::LocationSource& location, EggProgramTernary op, const IEggProgramNode& a, const IEggProgramNode& b, const IEggProgramNode& c);
    egg::ovum::Node mutate(const egg::ovum::LocationSource& location, EggProgramMutate op, const IEggProgramNode& a);
    egg::ovum::Node assign(const egg::ovum::LocationSource& location, EggProgramAssign op, const IEggProgramNode& a, const IEggProgramNode& b);
    egg::ovum::Node predicate(const egg::ovum::LocationSource& location, EggProgramBinary op, const IEggProgramNode& a, const IEggProgramNode& b);
    egg::ovum::Node noop(const egg::ovum::LocationSource& location, const IEggProgramNode* node);
    template<typename... ARGS>
    egg::ovum::Node statement(const egg::ovum::LocationSource& location, egg::ovum::Opcode opcode, ARGS&&... args) {
      return EggProgramCompilerNode(*this, location, opcode).add(std::forward<ARGS>(args)...).build();
    }
    template<typename... ARGS>
    egg::ovum::Node expression(const egg::ovum::LocationSource& location, egg::ovum::Opcode opcode, ARGS&&... args) {
      return EggProgramCompilerNode(*this, location, opcode).add(std::forward<ARGS>(args)...).build();
    }
    template<typename... ARGS>
    egg::ovum::Node operation(const egg::ovum::LocationSource& location, egg::ovum::Opcode opcode, egg::ovum::Operator oper, ARGS&&... args) {
      return EggProgramCompilerNode(*this, location, opcode).add(std::forward<ARGS>(args)...).build(oper);
    }
    template<typename... ARGS>
    egg::ovum::Node create(const egg::ovum::NodeLocation& location, egg::ovum::Opcode op, ARGS&&... args) {
      return egg::ovum::NodeFactory::create(this->context.getAllocator(), location, op, std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    egg::ovum::Node raise(ARGS&&... args) {
      auto message = egg::ovum::StringBuilder().add(std::forward<ARGS>(args)...).toUTF8();
      this->context.log(egg::ovum::ILogger::Source::Compiler, egg::ovum::ILogger::Severity::Error, message);
      return nullptr;
    }
  };
}
