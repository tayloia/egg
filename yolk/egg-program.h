namespace egg::yolk {
  class EggProgramContext;
  class EggProgramStackless;
  class EggProgramSymbolTable;

  class IEggProgramAssignee {
  public:
    virtual ~IEggProgramAssignee() {}
    virtual egg::lang::Value get() const = 0;
    virtual egg::lang::Value set(const egg::lang::Value& value) = 0;
  };

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
    virtual egg::lang::ITypeRef getType() const = 0;
    virtual egg::lang::LocationSource location() const = 0;
    virtual bool symbol(egg::ovum::String& nameOut, egg::lang::ITypeRef& typeOut) const = 0;
    virtual void empredicate(EggProgramContext& context, std::shared_ptr<IEggProgramNode>& ptr) = 0;
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) = 0;
    virtual EggProgramNodeFlags addressable(EggProgramContext& context) = 0;
    virtual egg::lang::Value execute(EggProgramContext& context) const = 0;
    virtual egg::lang::Value coexecute(EggProgramContext& context, EggProgramStackless& stackless) const = 0;
    virtual std::unique_ptr<IEggProgramAssignee> assignee(EggProgramContext& context) const = 0;
    virtual void dump(std::ostream& os) const = 0;
  };

  enum class EggProgramUnary {
    EGG_PROGRAM_UNARY_OPERATORS(EGG_PROGRAM_UNARY_OPERATOR_DECLARE)
  };

  enum class EggProgramBinary {
    EGG_PROGRAM_BINARY_OPERATORS(EGG_PROGRAM_BINARY_OPERATOR_DECLARE)
  };

  enum class EggProgramAssign {
    EGG_PROGRAM_ASSIGN_OPERATORS(EGG_PROGRAM_ASSIGN_OPERATOR_DECLARE)
  };

  enum class EggProgramMutate {
    EGG_PROGRAM_MUTATE_OPERATORS(EGG_PROGRAM_ASSIGN_OPERATOR_DECLARE)
  };

  class EggProgramSymbol {
  public:
    enum Kind { Builtin, Readonly, ReadWrite };
  private:
    Kind kind;
    egg::ovum::String name;
    egg::lang::ITypeRef type;
    egg::lang::Value value;
  public:
    EggProgramSymbol(Kind kind, const egg::ovum::String& name, const egg::lang::ITypeRef& type, const egg::lang::Value& value)
      : kind(kind), name(name), type(type), value(value) {
    }
    const egg::ovum::String& getName() const { return this->name; }
    const egg::lang::IType& getType() const { return *this->type; }
    egg::lang::Value& getValue() { return this->value; }
    void setInferredType(const egg::lang::ITypeRef& inferred);
    egg::lang::Value assign(EggProgramSymbolTable& symtable, egg::ovum::IExecution& execution, const egg::lang::Value& rhs);
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
    void addBuiltin(const std::string& name, const egg::lang::Value& value);
    std::shared_ptr<EggProgramSymbol> addSymbol(EggProgramSymbol::Kind kind, const egg::ovum::String& name, const egg::lang::ITypeRef& type, const egg::lang::Value& value = egg::lang::Value::Void);
    std::shared_ptr<EggProgramSymbol> findSymbol(const egg::ovum::String& name, bool includeParents = true) const;
  };

  class EggProgram {
    EGG_NO_COPY(EggProgram);
  private:
    egg::ovum::HardPtr<egg::ovum::IBasket> basket;
    std::shared_ptr<IEggProgramNode> root;
  public:
    EggProgram(egg::ovum::IAllocator& allocator, const std::shared_ptr<IEggProgramNode>& root)
      : basket(egg::ovum::BasketFactory::createBasket(allocator)),
        root(root) {
      assert(root != nullptr);
    }
    ~EggProgram() {
      this->root.reset();
      (void)this->basket->collect();
      // The destructor for 'basket' will assert if this collection doesn't free up everything in the basket
    }
    egg::ovum::HardPtr<EggProgramContext> createRootContext(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::lang::LogSeverity& maximumSeverity);
    egg::lang::LogSeverity prepare(IEggEnginePreparationContext& preparation);
    egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution);
    static std::string unaryToString(EggProgramUnary op);
    static std::string binaryToString(EggProgramBinary op);
    static std::string assignToString(EggProgramAssign op);
    static std::string mutateToString(EggProgramMutate op);
    static const egg::lang::IType& VanillaArray;
    static const egg::lang::IType& VanillaObject;
  };

  class EggProgramExpression {
  private:
    EggProgramContext* context;
    egg::lang::LocationRuntime before;
  public:
    EggProgramExpression(EggProgramContext& context, const IEggProgramNode& node);
    ~EggProgramExpression();
  };

  class EggProgramContext : public egg::ovum::SoftReferenceCounted<egg::ovum::ICollectable>, public egg::ovum::IExecution {
    EGG_NO_COPY(EggProgramContext);
  public:
    struct ScopeFunction {
      const egg::lang::IType* rettype;
      bool generator;
    };
  private:
    egg::lang::LocationRuntime location;
    egg::ovum::ILogger* logger;
    egg::ovum::SoftPtr<EggProgramSymbolTable> symtable;
    egg::lang::LogSeverity* maximumSeverity;
    const egg::lang::IType* scopeDeclare; // Only used in prepare phase
    ScopeFunction* scopeFunction; // Only used in prepare phase
    const egg::lang::Value* scopeValue; // Only used in execute phase
    EggProgramContext(egg::ovum::IAllocator& allocator, const egg::lang::LocationRuntime& location, egg::ovum::ILogger* logger, EggProgramSymbolTable& symtable, egg::lang::LogSeverity* maximumSeverity, ScopeFunction* scopeFunction)
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
    EggProgramContext(egg::ovum::IAllocator& allocator, const egg::lang::LocationRuntime& location, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::lang::LogSeverity& maximumSeverity)
      : EggProgramContext(allocator, location, &logger, symtable, &maximumSeverity, nullptr) {
    }
    virtual void softVisitLinks(const Visitor& visitor) const override;
    egg::ovum::HardPtr<EggProgramContext> createNestedContext(EggProgramSymbolTable& symtable, ScopeFunction* prepareFunction = nullptr);
    void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message);
    template<typename... ARGS>
    void problem(egg::lang::LogSource source, egg::lang::LogSeverity severity, ARGS... args) {
      auto message = egg::ovum::StringBuilder().add(args...).toUTF8();
      this->log(source, severity, message);
    }
    template<typename... ARGS>
    void compiler(egg::lang::LogSeverity severity, const egg::lang::LocationSource& location, ARGS... args) {
      this->problem(egg::lang::LogSource::Compiler, severity, location.toSourceString(), ": ", args...);
    }
    template<typename... ARGS>
    void compilerWarning(const egg::lang::LocationSource& location, ARGS... args) {
      this->compiler(egg::lang::LogSeverity::Warning, location, args...);
    }
    template<typename... ARGS>
    EggProgramNodeFlags compilerError(const egg::lang::LocationSource& location, ARGS... args) {
      this->compiler(egg::lang::LogSeverity::Error, location, args...);
      return EggProgramNodeFlags::Abandon;
    }
    std::unique_ptr<IEggProgramAssignee> assigneeIdentifier(const IEggProgramNode& self, const egg::ovum::String& name);
    std::unique_ptr<IEggProgramAssignee> assigneeBrackets(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const std::shared_ptr<IEggProgramNode>& index);
    std::unique_ptr<IEggProgramAssignee> assigneeDot(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const egg::ovum::String& property);
    std::unique_ptr<IEggProgramAssignee> assigneeDeref(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance);
    void statement(const IEggProgramNode& node); // TODO remove?
    egg::lang::LocationRuntime swapLocation(const egg::lang::LocationRuntime& loc); // TODO remove?
    egg::lang::Value get(const egg::ovum::String& name, bool byref);
    egg::lang::Value set(const egg::ovum::String& name, const egg::lang::Value& rvalue);
    egg::lang::Value guard(const egg::ovum::String& name, const egg::lang::Value& rvalue);
    egg::lang::Value assign(EggProgramAssign op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::lang::Value mutate(EggProgramMutate op, const IEggProgramNode& lhs);
    egg::lang::Value condition(const IEggProgramNode& expr);
    egg::lang::Value unary(EggProgramUnary op, const IEggProgramNode& expr, egg::lang::Value& value);
    egg::lang::Value binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs, egg::lang::Value& left, egg::lang::Value& right);
    egg::lang::Value call(const egg::lang::Value& callee, const egg::lang::IParameters& parameters);
    egg::lang::Value dotGet(const egg::lang::Value& instance, const egg::ovum::String& property);
    egg::lang::Value dotSet(const egg::lang::Value& instance, const egg::ovum::String& property, const egg::lang::Value& value);
    egg::lang::Value bracketsGet(const egg::lang::Value& instance, const egg::lang::Value& index);
    egg::lang::Value bracketsSet(const egg::lang::Value& instance, const egg::lang::Value& index, const egg::lang::Value& value);
    egg::lang::Value createVanillaArray();
    egg::lang::Value createVanillaObject();
    egg::lang::Value createVanillaFunction(const egg::lang::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value createVanillaGenerator(const egg::lang::ITypeRef& itertype, const egg::lang::ITypeRef& rettype, const std::shared_ptr<IEggProgramNode>& block);
    // Inherited via IExecution
    virtual egg::ovum::IAllocator& getAllocator() const override; // TODO needed?
    virtual egg::lang::Value raise(const egg::ovum::String& message) override;
    virtual egg::lang::Value assertion(const egg::lang::Value& predicate) override;
    virtual void print(const std::string& utf8) override;
    // Implemented in egg-execute.cpp
    egg::lang::Value executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeDeclare(const IEggProgramNode& self, const egg::ovum::String& name, const egg::lang::ITypeRef& type, const IEggProgramNode* rvalue);
    egg::lang::Value executeGuard(const IEggProgramNode& self, const egg::ovum::String& name, const egg::lang::ITypeRef& type, const IEggProgramNode& rvalue);
    egg::lang::Value executeAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue);
    egg::lang::Value executeMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue);
    egg::lang::Value executeBreak(const IEggProgramNode& self);
    egg::lang::Value executeCatch(const IEggProgramNode& self, const egg::ovum::String& name, const IEggProgramNode& type, const IEggProgramNode& block); // exception in 'scopeValue'
    egg::lang::Value executeContinue(const IEggProgramNode& self);
    egg::lang::Value executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::lang::Value executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock);
    egg::lang::Value executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block);
    egg::lang::Value executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block);
    egg::lang::Value executeFunctionDefinition(const IEggProgramNode& self, const egg::ovum::String& name, const egg::lang::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value executeFunctionCall(const egg::lang::ITypeRef& type, const egg::lang::IParameters& parameters, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value executeGeneratorDefinition(const IEggProgramNode& self, const egg::lang::ITypeRef& gentype, const egg::lang::ITypeRef& rettype, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value executeReturn(const IEggProgramNode& self, const IEggProgramNode* value);
    egg::lang::Value executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block); // against in 'scopeValue'
    egg::lang::Value executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases);
    egg::lang::Value executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception);
    egg::lang::Value executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final);
    egg::lang::Value executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::lang::Value executeYield(const IEggProgramNode& self, const IEggProgramNode& value);
    egg::lang::Value executeArray(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::lang::Value executeObject(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::lang::Value executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    egg::lang::Value executeIdentifier(const IEggProgramNode& self, const egg::ovum::String& name, bool byref);
    egg::lang::Value executeLiteral(const IEggProgramNode& self, const egg::lang::Value& value);
    egg::lang::Value executeBrackets(const IEggProgramNode& self, const IEggProgramNode& instance, const IEggProgramNode& index);
    egg::lang::Value executeDot(const IEggProgramNode& self, const IEggProgramNode& instance, const egg::ovum::String& property);
    egg::lang::Value executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& expr);
    egg::lang::Value executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::lang::Value executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse);
    egg::lang::Value executePredicate(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    // Implemented in function.cpp
    egg::lang::Value coexecuteBlock(EggProgramStackless& stackless, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value coexecuteDo(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value coexecuteFor(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& pre, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& post, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value coexecuteForeach(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& lvalue, const std::shared_ptr<IEggProgramNode>& rvalue, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value coexecuteWhile(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value coexecuteYield(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& value);
    // Implemented in egg-prepare.cpp
    std::shared_ptr<IEggProgramNode> empredicateBinary(const std::shared_ptr<IEggProgramNode>& node, EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs);
    EggProgramNodeFlags prepareModule(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareBlock(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareDeclare(const egg::lang::LocationSource& where, const egg::ovum::String& name, egg::lang::ITypeRef& ltype, IEggProgramNode* rvalue);
    EggProgramNodeFlags prepareGuard(const egg::lang::LocationSource& where, const egg::ovum::String& name, egg::lang::ITypeRef& ltype, IEggProgramNode& rvalue);
    EggProgramNodeFlags prepareAssign(const egg::lang::LocationSource& where, EggProgramAssign op, IEggProgramNode& lvalue, IEggProgramNode& rvalue);
    EggProgramNodeFlags prepareMutate(const egg::lang::LocationSource& where, EggProgramMutate op, IEggProgramNode& lvalue);
    EggProgramNodeFlags prepareCatch(const egg::ovum::String& name, IEggProgramNode& type, IEggProgramNode& block);
    EggProgramNodeFlags prepareDo(IEggProgramNode& cond, IEggProgramNode& block);
    EggProgramNodeFlags prepareIf(IEggProgramNode& cond, IEggProgramNode& trueBlock, IEggProgramNode* falseBlock);
    EggProgramNodeFlags prepareFor(IEggProgramNode* pre, IEggProgramNode* cond, IEggProgramNode* post, IEggProgramNode& block);
    EggProgramNodeFlags prepareForeach(IEggProgramNode& lvalue, IEggProgramNode& rvalue, IEggProgramNode& block);
    EggProgramNodeFlags prepareFunctionDefinition(const egg::ovum::String& name, const egg::lang::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& block);
    EggProgramNodeFlags prepareGeneratorDefinition(const egg::lang::ITypeRef& rettype, const std::shared_ptr<IEggProgramNode>& block);
    EggProgramNodeFlags prepareReturn(const egg::lang::LocationSource& where, IEggProgramNode* value);
    EggProgramNodeFlags prepareCase(const std::vector<std::shared_ptr<IEggProgramNode>>& values, IEggProgramNode& block);
    EggProgramNodeFlags prepareSwitch(IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases);
    EggProgramNodeFlags prepareThrow(IEggProgramNode* exception);
    EggProgramNodeFlags prepareTry(IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, IEggProgramNode* final);
    EggProgramNodeFlags prepareWhile(IEggProgramNode& cond, IEggProgramNode& block);
    EggProgramNodeFlags prepareYield(const egg::lang::LocationSource& where, IEggProgramNode& value);
    EggProgramNodeFlags prepareArray(const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    EggProgramNodeFlags prepareObject(const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    EggProgramNodeFlags prepareCall(IEggProgramNode& callee, std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    EggProgramNodeFlags prepareIdentifier(const egg::lang::LocationSource& where, const egg::ovum::String& name, egg::lang::ITypeRef& type);
    EggProgramNodeFlags prepareBrackets(const egg::lang::LocationSource& where, IEggProgramNode& instance, IEggProgramNode& index);
    EggProgramNodeFlags prepareDot(const egg::lang::LocationSource& where, IEggProgramNode& instance, const egg::ovum::String& property);
    EggProgramNodeFlags prepareUnary(const egg::lang::LocationSource& where, EggProgramUnary op, IEggProgramNode& value);
    EggProgramNodeFlags prepareBinary(const egg::lang::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs);
    EggProgramNodeFlags prepareTernary(const egg::lang::LocationSource& where, IEggProgramNode& cond, IEggProgramNode& whenTrue, IEggProgramNode& whenFalse);
    EggProgramNodeFlags preparePredicate(const egg::lang::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs);
    // Temporary scope modifiers
    EggProgramNodeFlags prepareWithType(IEggProgramNode& node, const egg::lang::ITypeRef& type);
    egg::lang::Value executeWithValue(const IEggProgramNode& node, const egg::lang::Value& value);
  private:
    bool findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareScope(const IEggProgramNode* node, std::function<EggProgramNodeFlags(EggProgramContext&)> action);
    EggProgramNodeFlags prepareStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags typeCheck(const egg::lang::LocationSource& where, egg::lang::ITypeRef& ltype, const egg::lang::ITypeRef& rtype, const egg::ovum::String& name, bool guard);
    typedef std::function<egg::lang::Value(EggProgramContext&)> ScopeAction;
    egg::lang::Value executeScope(const IEggProgramNode* node, ScopeAction action);
    egg::lang::Value executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeFinally(const egg::lang::Value& retval, const IEggProgramNode* block);
    egg::lang::Value executeForeachString(IEggProgramAssignee& target, const egg::ovum::String& source, const IEggProgramNode& block);
    egg::lang::Value executeForeachIterate(IEggProgramAssignee& target, egg::lang::IObject& source, const IEggProgramNode& block);
    bool operand(egg::lang::Value& dst, const IEggProgramNode& src, egg::lang::Discriminator expected, const char* expectation);
    typedef egg::lang::Value(*ArithmeticBool)(bool lhs, bool rhs);
    typedef egg::lang::Value(*ArithmeticInt)(int64_t lhs, int64_t rhs);
    typedef egg::lang::Value(*ArithmeticFloat)(double lhs, double rhs);
    egg::lang::Value coalesceNull(const egg::lang::Value& left, egg::lang::Value& right, const IEggProgramNode& rhs);
    egg::lang::Value logicalBool(const egg::lang::Value& left, egg::lang::Value& right, const IEggProgramNode& rhs, const char* operation, EggProgramBinary binary);
    egg::lang::Value arithmeticBool(const egg::lang::Value& left, egg::lang::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools);
    egg::lang::Value arithmeticInt(const egg::lang::Value& left, egg::lang::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints);
    egg::lang::Value arithmeticBoolInt(const egg::lang::Value& left, egg::lang::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools, ArithmeticInt ints);
    egg::lang::Value arithmeticIntFloat(const egg::lang::Value& left, egg::lang::Value& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints, ArithmeticFloat floats);
    egg::lang::Value unexpected(const std::string& expectation, const egg::lang::Value& value);
  };
}
