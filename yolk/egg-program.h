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

  class EggProgramSymbol {
  public:
    enum class Kind { Builtin, Readonly, ReadWrite };
  private:
    Kind kind;
    egg::ovum::String name;
    egg::ovum::Type type;
    egg::ovum::Value value;
  public:
    EggProgramSymbol(Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type, const egg::ovum::Value& value)
      : kind(kind), name(name), type(type), value(value) {
    }
    const egg::ovum::String& getName() const { return this->name; }
    const egg::ovum::IType& getType() const { return *this->type; }
    egg::ovum::Value& getValue() { return this->value; }
    void setInferredType(const egg::ovum::Type& inferred);
    egg::ovum::Value assign(EggProgramContext& context, const egg::ovum::Value& rhs);
  };

  class EggProgramSymbolTable : public egg::ovum::SoftReferenceCounted<egg::ovum::ICollectable> {
    EGG_NO_COPY(EggProgramSymbolTable);
  private:
    std::map<egg::ovum::String, std::shared_ptr<EggProgramSymbol>> map;
    egg::ovum::SoftPtr<EggProgramSymbolTable> parent;
  public:
    explicit EggProgramSymbolTable(egg::ovum::IAllocator& allocator, EggProgramSymbolTable* parent = nullptr)
      : SoftReferenceCounted(allocator) {
      this->parent.set(*this, parent);
    }
    virtual void softVisitLinks(const Visitor& visitor) const override;
    void addBuiltins();
    void addBuiltin(const std::string& name, const egg::ovum::Value& value);
    std::shared_ptr<EggProgramSymbol> addSymbol(EggProgramSymbol::Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type, const egg::ovum::Value& value = egg::ovum::Value::Void);
    std::shared_ptr<EggProgramSymbol> findSymbol(const egg::ovum::String& name, bool includeParents = true) const;
  };

  class EggProgram {
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
      switch (egg::ovum::Bits::mask(type->getFlags(), egg::ovum::ValueFlags::Arithmetic)) {
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

  class EggProgramExpression {
  private:
    EggProgramContext* context;
    egg::ovum::LocationRuntime before;
  public:
    EggProgramExpression(EggProgramContext& context, const IEggProgramNode& node);
    ~EggProgramExpression();
  };

  class EggProgramContext : public egg::ovum::SoftReferenceCounted<egg::ovum::ICollectable>, public egg::ovum::IExecution {
    EGG_NO_COPY(EggProgramContext);
  public:
    struct ScopeFunction {
      const egg::ovum::IType* rettype;
      bool generator;
    };
  private:
    egg::ovum::LocationRuntime location;
    egg::ovum::ILogger* logger;
    egg::ovum::SoftPtr<EggProgramSymbolTable> symtable;
    egg::ovum::ILogger::Severity* maximumSeverity;
    const egg::ovum::IType* scopeDeclare; // Only used in prepare phase
    ScopeFunction* scopeFunction; // Only used in prepare phase
    const egg::ovum::Value* scopeValue; // Only used in execute phase
    EggProgramContext(egg::ovum::IAllocator& allocator, const egg::ovum::LocationRuntime& location, egg::ovum::ILogger* logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity* maximumSeverity, ScopeFunction* scopeFunction)
      : SoftReferenceCounted(allocator),
        location(location),
        logger(logger),
        maximumSeverity(maximumSeverity),
        scopeDeclare(nullptr),
        scopeFunction(scopeFunction),
        scopeValue(nullptr) {
      this->symtable.set(*this, &symtable);
    }
  public:
    EggProgramContext(egg::ovum::IAllocator& allocator, EggProgramContext& parent, EggProgramSymbolTable& symtable, ScopeFunction* scopeFunction)
      : EggProgramContext(allocator, parent.location, parent.logger, symtable, parent.maximumSeverity, scopeFunction) {
    }
    EggProgramContext(egg::ovum::IAllocator& allocator, const egg::ovum::LocationRuntime& location, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity& maximumSeverity)
      : EggProgramContext(allocator, location, &logger, symtable, &maximumSeverity, nullptr) {
    }
    virtual void softVisitLinks(const Visitor& visitor) const override;
    egg::ovum::HardPtr<EggProgramContext> createNestedContext(EggProgramSymbolTable& symtable, ScopeFunction* prepareFunction = nullptr);
    void log(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, const std::string& message);
    template<typename... ARGS>
    void problem(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, ARGS&&... args) {
      auto message = egg::ovum::StringBuilder().add(std::forward<ARGS>(args)...).toUTF8();
      this->log(source, severity, message);
    }
    template<typename... ARGS>
    void compiler(egg::ovum::ILogger::Severity severity, const egg::ovum::LocationSource& location, ARGS&&... args) {
      this->problem(egg::ovum::ILogger::Source::Compiler, severity, location.toSourceString(), ": ", std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    void compilerWarning(const egg::ovum::LocationSource& location, ARGS&&... args) {
      this->compiler(egg::ovum::ILogger::Severity::Warning, location, std::forward<ARGS>(args)...);
    }
    template<typename... ARGS>
    EggProgramNodeFlags compilerError(const egg::ovum::LocationSource& location, ARGS&&... args) {
      this->compiler(egg::ovum::ILogger::Severity::Error, location, std::forward<ARGS>(args)...);
      return EggProgramNodeFlags::Abandon;
    }
    void statement(const IEggProgramNode& node); // TODO remove?
    egg::ovum::LocationRuntime swapLocation(const egg::ovum::LocationRuntime& loc); // TODO remove?
    egg::ovum::Value get(const egg::ovum::String& name);
    egg::ovum::Value set(const egg::ovum::String& name, const egg::ovum::Value& rvalue);
    egg::ovum::Value guard(const egg::ovum::String& name, const egg::ovum::Value& rvalue);
    egg::ovum::Value assign(EggProgramAssign op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::ovum::Value mutate(EggProgramMutate op, const IEggProgramNode& lhs);
    egg::ovum::Value condition(const IEggProgramNode& expr);
    egg::ovum::Value unary(EggProgramUnary op, const IEggProgramNode& expr, egg::ovum::Value& value);
    egg::ovum::Value binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs, egg::ovum::Value& left, egg::ovum::Value& right);
    egg::ovum::Value call(const egg::ovum::Value& callee, const egg::ovum::IParameters& parameters);
    egg::ovum::Value dotGet(const egg::ovum::Value& instance, const egg::ovum::String& property);
    egg::ovum::Value dotSet(const egg::ovum::Value& instance, const egg::ovum::String& property, const egg::ovum::Value& value);
    egg::ovum::Value bracketsGet(const egg::ovum::Value& instance, const egg::ovum::Value& index);
    egg::ovum::Value bracketsSet(const egg::ovum::Value& instance, const egg::ovum::Value& index, const egg::ovum::Value& value);

    // Inherited via IExecution
    virtual egg::ovum::IAllocator& getAllocator() const override;
    virtual egg::ovum::IBasket& getBasket() const override;
    virtual egg::ovum::Value raise(const egg::ovum::String& message) override;
    virtual egg::ovum::Value assertion(const egg::ovum::Value& predicate) override;
    virtual void print(const std::string& utf8) override;
    // Implemented in egg-execute.cpp
    egg::ovum::Value executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::ovum::Value executeBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::ovum::Value executeDeclare(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const IEggProgramNode* rvalue);
    egg::ovum::Value executeGuard(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const IEggProgramNode& rvalue);
    egg::ovum::Value executeAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue);
    egg::ovum::Value executeMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue);
    egg::ovum::Value executeBreak(const IEggProgramNode& self);
    egg::ovum::Value executeCatch(const IEggProgramNode& self, const egg::ovum::String& name, const IEggProgramNode& type, const IEggProgramNode& block); // exception in 'scopeValue'
    egg::ovum::Value executeContinue(const IEggProgramNode& self);
    egg::ovum::Value executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::ovum::Value executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock);
    egg::ovum::Value executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block);
    egg::ovum::Value executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block);
    egg::ovum::Value executeFunctionDefinition(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Value executeFunctionCall(const egg::ovum::Type& type, const egg::ovum::IParameters& parameters, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Value executeGeneratorDefinition(const IEggProgramNode& self, const egg::ovum::Type& gentype, const egg::ovum::Type& rettype, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Value executeReturn(const IEggProgramNode& self, const IEggProgramNode* value);
    egg::ovum::Value executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block); // against in 'scopeValue'
    egg::ovum::Value executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases);
    egg::ovum::Value executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception);
    egg::ovum::Value executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final);
    egg::ovum::Value executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::ovum::Value executeYield(const IEggProgramNode& self, const IEggProgramNode& value);
    egg::ovum::Value executeArray(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::ovum::Value executeObject(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::ovum::Value executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    egg::ovum::Value executeIdentifier(const IEggProgramNode& self, const egg::ovum::String& name);
    egg::ovum::Value executeLiteral(const IEggProgramNode& self, const egg::ovum::Value& value);
    egg::ovum::Value executeBrackets(const IEggProgramNode& self, const IEggProgramNode& instance, const IEggProgramNode& index);
    egg::ovum::Value executeDot(const IEggProgramNode& self, const IEggProgramNode& instance, const egg::ovum::String& property);
    egg::ovum::Value executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& expr);
    egg::ovum::Value executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::ovum::Value executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse);
    egg::ovum::Value executePredicate(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    // Implemented in function.cpp
    egg::ovum::Value coexecuteBlock(EggProgramStackless& stackless, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::ovum::Value coexecuteDo(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Value coexecuteFor(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& pre, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& post, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Value coexecuteForeach(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& lvalue, const std::shared_ptr<IEggProgramNode>& rvalue, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Value coexecuteWhile(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Value coexecuteYield(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& value);
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
    egg::ovum::Value executeWithValue(const IEggProgramNode& node, const egg::ovum::Value& value);
  private:
    bool findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareScope(const IEggProgramNode* node, std::function<EggProgramNodeFlags(EggProgramContext&)> action);
    EggProgramNodeFlags prepareStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags typeCheck(const egg::ovum::LocationSource& where, egg::ovum::Type& ltype, const egg::ovum::Type& rtype, const egg::ovum::String& name, bool guard);
    bool operand(egg::ovum::Value& dst, const IEggProgramNode& src, egg::ovum::ValueFlags expected, const char* expectation);
    typedef egg::ovum::Value(*ArithmeticBool)(bool lhs, bool rhs);
    typedef egg::ovum::Value(*ArithmeticInt)(int64_t lhs, int64_t rhs);
    typedef egg::ovum::Value(*ArithmeticFloat)(double lhs, double rhs);
    egg::ovum::Value coalesceNull(const egg::ovum::Value& left, egg::ovum::Value& right, const IEggProgramNode& rhs);
    egg::ovum::Value logicalBool(const egg::ovum::Value& left, egg::ovum::Value& right, const IEggProgramNode& rhs, const char* operation, EggProgramBinary binary);
    egg::ovum::Value arithmeticBool(const egg::ovum::Value& left, egg::ovum::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools);
    egg::ovum::Value arithmeticInt(const egg::ovum::Value& left, egg::ovum::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints);
    egg::ovum::Value arithmeticBoolInt(const egg::ovum::Value& left, egg::ovum::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools, ArithmeticInt ints);
    egg::ovum::Value arithmeticIntFloat(const egg::ovum::Value& left, egg::ovum::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints, ArithmeticFloat floats);
    egg::ovum::Value unexpected(const std::string& expectation, const egg::ovum::Value& value);
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
