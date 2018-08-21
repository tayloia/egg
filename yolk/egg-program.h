namespace egg::yolk {
  class EggProgramContext;
  class EggProgramStackless;
  class EggProgramSymbolTable;

  class IEggProgramAssignee {
  public:
    virtual ~IEggProgramAssignee() {}
    virtual egg::ovum::Variant get() const = 0;
    virtual egg::ovum::Variant set(const egg::ovum::Variant& value) = 0;
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
    virtual egg::ovum::Type getType() const = 0;
    virtual egg::ovum::LocationSource location() const = 0;
    virtual bool symbol(egg::ovum::String& nameOut, egg::ovum::Type& typeOut) const = 0;
    virtual void empredicate(EggProgramContext& context, std::shared_ptr<IEggProgramNode>& ptr) = 0;
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) = 0;
    virtual EggProgramNodeFlags addressable(EggProgramContext& context) = 0;
    virtual egg::ovum::Variant execute(EggProgramContext& context) const = 0;
    virtual egg::ovum::Variant coexecute(EggProgramContext& context, EggProgramStackless& stackless) const = 0;
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
    egg::ovum::Type type;
    egg::ovum::Variant value;
  public:
    EggProgramSymbol(Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type, const egg::ovum::Variant& value)
      : kind(kind), name(name), type(type), value(value) {
    }
    const egg::ovum::String& getName() const { return this->name; }
    const egg::ovum::IType& getType() const { return *this->type; }
    egg::ovum::Variant& getValue() { return this->value; }
    void setInferredType(const egg::ovum::Type& inferred);
    egg::ovum::Variant assign(EggProgramContext& context, const egg::ovum::Variant& rhs);
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
    void addBuiltin(const std::string& name, const egg::ovum::Variant& value);
    std::shared_ptr<EggProgramSymbol> addSymbol(EggProgramSymbol::Kind kind, const egg::ovum::String& name, const egg::ovum::Type& type, const egg::ovum::Variant& value = egg::ovum::Variant::Void);
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
    const egg::ovum::Variant* scopeValue; // Only used in execute phase
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
    egg::ovum::Variant get(const egg::ovum::String& name, bool byref);
    egg::ovum::Variant set(const egg::ovum::String& name, const egg::ovum::Variant& rvalue);
    egg::ovum::Variant guard(const egg::ovum::String& name, const egg::ovum::Variant& rvalue);
    egg::ovum::Variant assign(EggProgramAssign op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::ovum::Variant mutate(EggProgramMutate op, const IEggProgramNode& lhs);
    egg::ovum::Variant condition(const IEggProgramNode& expr);
    egg::ovum::Variant unary(EggProgramUnary op, const IEggProgramNode& expr, egg::ovum::Variant& value);
    egg::ovum::Variant binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs, egg::ovum::Variant& left, egg::ovum::Variant& right);
    egg::ovum::Variant call(const egg::ovum::Variant& callee, const egg::ovum::IParameters& parameters);
    egg::ovum::Variant dotGet(const egg::ovum::Variant& instance, const egg::ovum::String& property);
    egg::ovum::Variant dotSet(const egg::ovum::Variant& instance, const egg::ovum::String& property, const egg::ovum::Variant& value);
    egg::ovum::Variant bracketsGet(const egg::ovum::Variant& instance, const egg::ovum::Variant& index);
    egg::ovum::Variant bracketsSet(const egg::ovum::Variant& instance, const egg::ovum::Variant& index, const egg::ovum::Variant& value);
    egg::ovum::Variant createVanillaArray();
    egg::ovum::Variant createVanillaObject();
    egg::ovum::Variant createVanillaFunction(const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Variant createVanillaGenerator(const egg::ovum::Type& itertype, const egg::ovum::Type& rettype, const std::shared_ptr<IEggProgramNode>& block);
    // Inherited via IExecution
    virtual egg::ovum::IAllocator& getAllocator() const override; // TODO needed?
    virtual egg::ovum::Variant raise(const egg::ovum::String& message) override;
    virtual egg::ovum::Variant assertion(const egg::ovum::Variant& predicate) override;
    virtual void print(const std::string& utf8) override;
    // Implemented in egg-execute.cpp
    egg::ovum::Variant executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::ovum::Variant executeBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::ovum::Variant executeDeclare(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const IEggProgramNode* rvalue);
    egg::ovum::Variant executeGuard(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const IEggProgramNode& rvalue);
    egg::ovum::Variant executeAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue);
    egg::ovum::Variant executeMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue);
    egg::ovum::Variant executeBreak(const IEggProgramNode& self);
    egg::ovum::Variant executeCatch(const IEggProgramNode& self, const egg::ovum::String& name, const IEggProgramNode& type, const IEggProgramNode& block); // exception in 'scopeValue'
    egg::ovum::Variant executeContinue(const IEggProgramNode& self);
    egg::ovum::Variant executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::ovum::Variant executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock);
    egg::ovum::Variant executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block);
    egg::ovum::Variant executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block);
    egg::ovum::Variant executeFunctionDefinition(const IEggProgramNode& self, const egg::ovum::String& name, const egg::ovum::Type& type, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Variant executeFunctionCall(const egg::ovum::Type& type, const egg::ovum::IParameters& parameters, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Variant executeGeneratorDefinition(const IEggProgramNode& self, const egg::ovum::Type& gentype, const egg::ovum::Type& rettype, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Variant executeReturn(const IEggProgramNode& self, const IEggProgramNode* value);
    egg::ovum::Variant executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block); // against in 'scopeValue'
    egg::ovum::Variant executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases);
    egg::ovum::Variant executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception);
    egg::ovum::Variant executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final);
    egg::ovum::Variant executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::ovum::Variant executeYield(const IEggProgramNode& self, const IEggProgramNode& value);
    egg::ovum::Variant executeArray(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::ovum::Variant executeObject(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::ovum::Variant executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    egg::ovum::Variant executeIdentifier(const IEggProgramNode& self, const egg::ovum::String& name, bool byref);
    egg::ovum::Variant executeLiteral(const IEggProgramNode& self, const egg::ovum::Variant& value);
    egg::ovum::Variant executeBrackets(const IEggProgramNode& self, const IEggProgramNode& instance, const IEggProgramNode& index);
    egg::ovum::Variant executeDot(const IEggProgramNode& self, const IEggProgramNode& instance, const egg::ovum::String& property);
    egg::ovum::Variant executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& expr);
    egg::ovum::Variant executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::ovum::Variant executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse);
    egg::ovum::Variant executePredicate(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    // Implemented in function.cpp
    egg::ovum::Variant coexecuteBlock(EggProgramStackless& stackless, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::ovum::Variant coexecuteDo(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Variant coexecuteFor(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& pre, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& post, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Variant coexecuteForeach(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& lvalue, const std::shared_ptr<IEggProgramNode>& rvalue, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Variant coexecuteWhile(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& cond, const std::shared_ptr<IEggProgramNode>& block);
    egg::ovum::Variant coexecuteYield(EggProgramStackless& stackless, const std::shared_ptr<IEggProgramNode>& value);
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
    egg::ovum::Variant executeWithValue(const IEggProgramNode& node, const egg::ovum::Variant& value);
  private:
    bool findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareScope(const IEggProgramNode* node, std::function<EggProgramNodeFlags(EggProgramContext&)> action);
    EggProgramNodeFlags prepareStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags typeCheck(const egg::ovum::LocationSource& where, egg::ovum::Type& ltype, const egg::ovum::Type& rtype, const egg::ovum::String& name, bool guard);
    typedef std::function<egg::ovum::Variant(EggProgramContext&)> ScopeAction;
    egg::ovum::Variant executeScope(const IEggProgramNode* node, ScopeAction action);
    egg::ovum::Variant executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::ovum::Variant executeFinally(const egg::ovum::Variant& retval, const IEggProgramNode* block);
    egg::ovum::Variant executeForeachString(IEggProgramAssignee& target, const egg::ovum::String& source, const IEggProgramNode& block);
    egg::ovum::Variant executeForeachIterate(IEggProgramAssignee& target, egg::ovum::IObject& source, const IEggProgramNode& block);
    bool operand(egg::ovum::Variant& dst, const IEggProgramNode& src, egg::ovum::VariantBits expected, const char* expectation);
    typedef egg::ovum::Variant(*ArithmeticBool)(bool lhs, bool rhs);
    typedef egg::ovum::Variant(*ArithmeticInt)(int64_t lhs, int64_t rhs);
    typedef egg::ovum::Variant(*ArithmeticFloat)(double lhs, double rhs);
    egg::ovum::Variant coalesceNull(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs);
    egg::ovum::Variant logicalBool(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, EggProgramBinary binary);
    egg::ovum::Variant arithmeticBool(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools);
    egg::ovum::Variant arithmeticInt(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints);
    egg::ovum::Variant arithmeticBoolInt(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, ArithmeticBool bools, ArithmeticInt ints);
    egg::ovum::Variant arithmeticIntFloat(const egg::ovum::Variant& left, egg::ovum::Variant& right, const IEggProgramNode& rhs, const char* operation, ArithmeticInt ints, ArithmeticFloat floats);
    egg::ovum::Variant unexpected(const std::string& expectation, const egg::ovum::Variant& value);
  };
}
