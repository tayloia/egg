namespace egg::yolk {
  class EggProgramContext;
  class EggProgramStackless;
  class EggProgramSymbolTable;

  class IEggProgramAssignee {
  public:
    virtual ~IEggProgramAssignee() {}
    virtual egg::lang::ValueLegacy get() const = 0;
    virtual egg::lang::ValueLegacy set(const egg::lang::ValueLegacy& value) = 0;
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
    virtual egg::ovum::ITypeRef getType() const = 0;
    virtual egg::ovum::LocationSource location() const = 0;
    virtual bool symbol(egg::ovum::String& nameOut, egg::ovum::ITypeRef& typeOut) const = 0;
    virtual void empredicate(EggProgramContext& context, std::shared_ptr<IEggProgramNode>& ptr) = 0;
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) = 0;
    virtual EggProgramNodeFlags addressable(EggProgramContext& context) = 0;
    virtual egg::lang::ValueLegacy execute(EggProgramContext& context) const = 0;
    virtual egg::lang::ValueLegacy coexecute(EggProgramContext& context, EggProgramStackless& stackless) const = 0;
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
    egg::ovum::ITypeRef type;
    egg::lang::ValueLegacy value;
  public:
    EggProgramSymbol(Kind kind, const egg::ovum::String& name, const egg::ovum::ITypeRef& type, const egg::lang::ValueLegacy& value)
      : kind(kind), name(name), type(type), value(value) {
    }
    const egg::ovum::String& getName() const { return this->name; }
    const egg::ovum::IType& getType() const { return *this->type; }
    egg::lang::ValueLegacy& getValue() { return this->value; }
    void setInferredType(const egg::ovum::ITypeRef& inferred);
    egg::lang::ValueLegacy assign(EggProgramContext& context, EggProgramSymbolTable& symtable, const egg::lang::ValueLegacy& rhs);
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
    void addBuiltin(const std::string& name, const egg::lang::ValueLegacy& value);
    std::shared_ptr<EggProgramSymbol> addSymbol(EggProgramSymbol::Kind kind, const egg::ovum::String& name, const egg::ovum::ITypeRef& type, const egg::lang::ValueLegacy& value = egg::lang::ValueLegacy::Void);
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
    egg::ovum::HardPtr<EggProgramContext> createRootContext(egg::ovum::IAllocator& allocator, egg::ovum::ILogger& logger, EggProgramSymbolTable& symtable, egg::ovum::ILogger::Severity& maximumSeverity);
    egg::ovum::ILogger::Severity prepare(IEggEnginePreparationContext& preparation);
    egg::ovum::ILogger::Severity execute(IEggEngineExecutionContext& execution);
    static std::string unaryToString(EggProgramUnary op);
    static std::string binaryToString(EggProgramBinary op);
    static std::string assignToString(EggProgramAssign op);
    static std::string mutateToString(EggProgramMutate op);
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
    const egg::lang::ValueLegacy* scopeValue; // Only used in execute phase
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
    void problem(egg::ovum::ILogger::Source source, egg::ovum::ILogger::Severity severity, ARGS... args) {
      auto message = egg::ovum::StringBuilder().add(args...).toUTF8();
      this->log(source, severity, message);
    }
    template<typename... ARGS>
    void compiler(egg::ovum::ILogger::Severity severity, const egg::ovum::LocationSource& location, ARGS... args) {
      this->problem(egg::ovum::ILogger::Source::Compiler, severity, location.toSourceString(), ": ", args...);
    }
    template<typename... ARGS>
    void compilerWarning(const egg::ovum::LocationSource& location, ARGS... args) {
      this->compiler(egg::ovum::ILogger::Severity::Warning, location, args...);
    }
    template<typename... ARGS>
    EggProgramNodeFlags compilerError(const egg::ovum::LocationSource& location, ARGS... args) {
      this->compiler(egg::ovum::ILogger::Severity::Error, location, args...);
      return EggProgramNodeFlags::Abandon;
    }
    std::unique_ptr<IEggProgramAssignee> assigneeIdentifier(const IEggProgramNode& self, const egg::ovum::String& name);
    std::unique_ptr<IEggProgramAssignee> assigneeBrackets(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const std::shared_ptr<IEggProgramNode>& index);
    std::unique_ptr<IEggProgramAssignee> assigneeDot(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const egg::ovum::String& property);
    std::unique_ptr<IEggProgramAssignee> assigneeDeref(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance);
    void statement(const IEggProgramNode& node); // TODO remove?
    egg::ovum::LocationRuntime swapLocation(const egg::ovum::LocationRuntime& loc); // TODO remove?
    egg::lang::ValueLegacy get(const egg::ovum::String& name, bool byref);
    egg::lang::ValueLegacy set(const egg::ovum::String& name, const egg::lang::ValueLegacy& rvalue);
    egg::lang::ValueLegacy guard(const egg::ovum::String& name, const egg::lang::ValueLegacy& rvalue);
    egg::lang::ValueLegacy assign(EggProgramAssign op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::lang::ValueLegacy mutate(EggProgramMutate op, const IEggProgramNode& lhs);
    egg::lang::ValueLegacy condition(const IEggProgramNode& expr);
    egg::lang::ValueLegacy unary(EggProgramUnary op, const IEggProgramNode& expr, egg::lang::ValueLegacy& value);
    egg::lang::ValueLegacy binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs, egg::lang::ValueLegacy& left, egg::lang::ValueLegacy& right);
    egg::lang::ValueLegacy call(const egg::lang::ValueLegacy& callee, const egg::ovum::IParameters& parameters);
    egg::lang::ValueLegacy dotGet(const egg::lang::ValueLegacy& instance, const egg::ovum::String& property);
    egg::lang::ValueLegacy dotSet(const egg::lang::ValueLegacy& instance, const egg::ovum::String& property, const egg::lang::ValueLegacy& value);
    egg::lang::ValueLegacy bracketsGet(const egg::lang::ValueLegacy& instance, const egg::lang::ValueLegacy& index);
    egg::lang::ValueLegacy bracketsSet(const egg::lang::ValueLegacy& instance, const egg::lang::ValueLegacy& index, const egg::lang::ValueLegacy& value);
    egg::lang::ValueLegacy createVanillaArray();
    egg::lang::ValueLegacy createVanillaObject();
    egg::lang::ValueLegacy createVanillaFunction(const egg::ovum::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::ValueLegacy createVanillaGenerator(const egg::ovum::ITypeRef& itertype, const egg::ovum::ITypeRef& rettype, const std::shared_ptr<IEggProgramNode>& block);
    // Inherited via IExecution
    virtual egg::ovum::IAllocator& getAllocator() const override; // TODO needed?
    virtual egg::lang::ValueLegacy raise(const egg::ovum::String& message) override;
    virtual egg::lang::ValueLegacy assertion(const egg::lang::ValueLegacy& predicate) override;
    virtual void print(const std::string& utf8) override;
    // Implemented in egg-execute.cpp
    egg::lang::ValueLegacy executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::ValueLegacy executeBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::ValueLegacy executeDeclare(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::ITypeRef& type, const IEggProgramNode* rvalue);
    egg::lang::ValueLegacy executeGuard(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::ITypeRef& type, const IEggProgramNode& rvalue);
    egg::lang::ValueLegacy executeAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue);
    egg::lang::ValueLegacy executeMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue);
    egg::lang::ValueLegacy executeBreak(const IEggProgramNode& self);
    egg::lang::ValueLegacy executeCatch(const IEggProgramNode& self, const egg::ovum::String& name, const IEggProgramNode& type, const IEggProgramNode& block); // exception in 'scopeValue'
    egg::lang::ValueLegacy executeContinue(const IEggProgramNode& self);
    egg::lang::ValueLegacy executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::lang::ValueLegacy executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock);
    egg::lang::ValueLegacy executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block);
    egg::lang::ValueLegacy executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block);
    egg::lang::ValueLegacy executeFunctionDefinition(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::ValueLegacy executeFunctionCall(const egg::ovum::ITypeRef& type, const egg::ovum::IParameters& parameters, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::ValueLegacy executeGeneratorDefinition(const IEggProgramNode& self, const egg::ovum::ITypeRef& gentype, const egg::ovum::ITypeRef& rettype, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::ValueLegacy executeReturn(const IEggProgramNode& self, const IEggProgramNode* value);
    egg::lang::ValueLegacy executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block); // against in 'scopeValue'
    egg::lang::ValueLegacy executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases);
    egg::lang::ValueLegacy executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception);
    egg::lang::ValueLegacy executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final);
    egg::lang::ValueLegacy executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::lang::ValueLegacy executeYield(const IEggProgramNode& self, const IEggProgramNode& value);
    egg::lang::ValueLegacy executeArray(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::lang::ValueLegacy executeObject(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::lang::ValueLegacy executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    egg::lang::ValueLegacy executeIdentifier(const IEggProgramNode& self, const egg::ovum::String& name, bool byref);
    egg::lang::ValueLegacy executeLiteral(const IEggProgramNode& self, const egg::lang::ValueLegacy& value);
    egg::lang::ValueLegacy executeBrackets(const IEggProgramNode& self, const IEggProgramNode& instance, const IEggProgramNode& index);
    egg::lang::ValueLegacy executeDot(const IEggProgramNode& self, const IEggProgramNode& instance, const egg::ovum::String& property);
    egg::lang::ValueLegacy executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& expr);
    egg::lang::ValueLegacy executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::lang::ValueLegacy executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse);
    egg::lang::ValueLegacy executePredicate(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    // Implemented in function.cpp
    egg::lang::ValueLegacy coexecuteBlock(EggProgramStackless& stackless, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::ValueLegacy coexecuteDo(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::ValueLegacy coexecuteFor(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& pre, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& post, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::ValueLegacy coexecuteForeach(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& lvalue, const std::shared_ptr<IEggProgramNode>& rvalue, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::ValueLegacy coexecuteWhile(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::ValueLegacy coexecuteYield(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& value);
    // Implemented in egg-prepare.cpp
    std::shared_ptr<IEggProgramNode> empredicateBinary(const std::shared_ptr<IEggProgramNode>& node, EggProgramBinary op, const std::shared_ptr<IEggProgramNode>& lhs, const std::shared_ptr<IEggProgramNode>& rhs);
    EggProgramNodeFlags prepareModule(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareBlock(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareDeclare(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::ITypeRef& ltype, IEggProgramNode* rvalue);
    EggProgramNodeFlags prepareGuard(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::ITypeRef& ltype, IEggProgramNode& rvalue);
    EggProgramNodeFlags prepareAssign(const egg::ovum::LocationSource& where, EggProgramAssign op, IEggProgramNode& lvalue, IEggProgramNode& rvalue);
    EggProgramNodeFlags prepareMutate(const egg::ovum::LocationSource& where, EggProgramMutate op, IEggProgramNode& lvalue);
    EggProgramNodeFlags prepareCatch(const egg::ovum::String& name, IEggProgramNode& type, IEggProgramNode& block);
    EggProgramNodeFlags prepareDo(IEggProgramNode& cond, IEggProgramNode& block);
    EggProgramNodeFlags prepareIf(IEggProgramNode& cond, IEggProgramNode& trueBlock, IEggProgramNode* falseBlock);
    EggProgramNodeFlags prepareFor(IEggProgramNode* pre, IEggProgramNode* cond, IEggProgramNode* post, IEggProgramNode& block);
    EggProgramNodeFlags prepareForeach(IEggProgramNode& lvalue, IEggProgramNode& rvalue, IEggProgramNode& block);
    EggProgramNodeFlags prepareFunctionDefinition(const egg::ovum::String& name, const egg::ovum::ITypeRef& type, const std::shared_ptr<IEggProgramNode>& block);
    EggProgramNodeFlags prepareGeneratorDefinition(const egg::ovum::ITypeRef& rettype, const std::shared_ptr<IEggProgramNode>& block);
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
    EggProgramNodeFlags prepareIdentifier(const egg::ovum::LocationSource& where, const egg::ovum::String& name, egg::ovum::ITypeRef& type);
    EggProgramNodeFlags prepareBrackets(const egg::ovum::LocationSource& where, IEggProgramNode& instance, IEggProgramNode& index);
    EggProgramNodeFlags prepareDot(const egg::ovum::LocationSource& where, IEggProgramNode& instance, const egg::ovum::String& property);
    EggProgramNodeFlags prepareUnary(const egg::ovum::LocationSource& where, EggProgramUnary op, IEggProgramNode& value);
    EggProgramNodeFlags prepareBinary(const egg::ovum::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs);
    EggProgramNodeFlags prepareTernary(const egg::ovum::LocationSource& where, IEggProgramNode& cond, IEggProgramNode& whenTrue, IEggProgramNode& whenFalse);
    EggProgramNodeFlags preparePredicate(const egg::ovum::LocationSource& where, EggProgramBinary op, IEggProgramNode& lhs, IEggProgramNode& rhs);
    // Temporary scope modifiers
    EggProgramNodeFlags prepareWithType(IEggProgramNode& node, const egg::ovum::ITypeRef& type);
    egg::lang::ValueLegacy executeWithValue(const IEggProgramNode& node, const egg::lang::ValueLegacy& value);
  private:
    bool findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareScope(const IEggProgramNode* node, std::function<EggProgramNodeFlags(EggProgramContext&)> action);
    EggProgramNodeFlags prepareStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags typeCheck(const egg::ovum::LocationSource& where, egg::ovum::ITypeRef& ltype, const egg::ovum::ITypeRef& rtype, const egg::ovum::String& name, bool guard);
    typedef std::function<egg::lang::ValueLegacy(EggProgramContext&)> ScopeAction;
    egg::lang::ValueLegacy executeScope(const IEggProgramNode* node, ScopeAction action);
    egg::lang::ValueLegacy executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::ValueLegacy executeFinally(const egg::lang::ValueLegacy& retval, const IEggProgramNode* block);
    egg::lang::ValueLegacy executeForeachString(IEggProgramAssignee& target, const egg::ovum::String& source, const IEggProgramNode& block);
    egg::lang::ValueLegacy executeForeachIterate(IEggProgramAssignee& target, egg::ovum::IObject& source, const IEggProgramNode& block);
    bool operand(egg::lang::ValueLegacy& dst, const IEggProgramNode& src, egg::ovum::Basal expected, const char* expectation);
    typedef egg::lang::ValueLegacy(*ArithmeticBool)(bool lhs, bool rhs);
    typedef egg::lang::ValueLegacy(*ArithmeticInt)(int64_t lhs, int64_t rhs);
    typedef egg::lang::ValueLegacy(*ArithmeticFloat)(double lhs, double rhs);
    egg::lang::ValueLegacy coalesceNull(const egg::lang::ValueLegacy& left, egg::lang::ValueLegacy& right, const IEggProgramNode& rhs);
    egg::lang::ValueLegacy logicalBool(const egg::lang::ValueLegacy& left, egg::lang::ValueLegacy& right, const IEggProgramNode& rhs, const char* operation, EggProgramBinary binary);
    egg::lang::ValueLegacy arithmeticBool(const egg::lang::ValueLegacy& left, egg::lang::ValueLegacy& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools);
    egg::lang::ValueLegacy arithmeticInt(const egg::lang::ValueLegacy& left, egg::lang::ValueLegacy& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints);
    egg::lang::ValueLegacy arithmeticBoolInt(const egg::lang::ValueLegacy& left, egg::lang::ValueLegacy& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools, ArithmeticInt ints);
    egg::lang::ValueLegacy arithmeticIntFloat(const egg::lang::ValueLegacy& left, egg::lang::ValueLegacy& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints, ArithmeticFloat floats);
    egg::lang::ValueLegacy unexpected(const std::string& expectation, const egg::lang::ValueLegacy& value);
  };
}
