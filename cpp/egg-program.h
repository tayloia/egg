namespace egg::yolk {
  class EggProgramContext;

  class IEggProgramAssignee {
  public:
    virtual ~IEggProgramAssignee() {}
    virtual egg::lang::Value get() const = 0;
    virtual egg::lang::Value set(const egg::lang::Value& value) = 0;
  };

  enum class EggProgramNodeFlags {
    None = 0x00,
    Constant = 0x01,
    Deferred = 0x02,
    Error = 0x80
  };

  class IEggProgramNode {
  public:
    virtual ~IEggProgramNode() {}
    virtual egg::lang::ITypeRef getType() const = 0;
    virtual egg::lang::LocationSource location() const = 0;
    virtual bool symbol(egg::lang::String& nameOut, egg::lang::ITypeRef& typeOut) const = 0;
    virtual EggProgramNodeFlags prepare(EggProgramContext& context) = 0;
    virtual egg::lang::Value execute(EggProgramContext& context) const = 0;
    virtual egg::lang::Value executeWithExpression(EggProgramContext& context, const egg::lang::Value& expression) const = 0;
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

  class EggProgramSymbolTable {
    EGG_NO_COPY(EggProgramSymbolTable);
  public:
    struct Symbol {
      egg::lang::String name;
      egg::lang::ITypeRef type;
      egg::lang::Value value;
      Symbol(const egg::lang::String& name, const egg::lang::IType& type)
        : name(name), type(&type), value(egg::lang::Value::Void) {
      }
      egg::lang::Value assign(egg::lang::IExecution& execution, const egg::lang::Value& rhs);
    };
  private:
    std::map<egg::lang::String, std::shared_ptr<Symbol>> map;
    const EggProgramSymbolTable* parent;
  public:
    explicit EggProgramSymbolTable(const EggProgramSymbolTable* parent = nullptr)
      : parent(parent) {
    }
    void addBuiltins();
    void addBuiltin(const std::string& name, const egg::lang::Value& value);
    std::shared_ptr<Symbol> addSymbol(const egg::lang::String& name, const egg::lang::IType& type);
    std::shared_ptr<Symbol> findSymbol(const egg::lang::String& name, bool includeParents = true) const;
  };

  class EggProgram {
  public:
  private:
    std::shared_ptr<IEggProgramNode> root;
  public:
    explicit EggProgram(const std::shared_ptr<IEggProgramNode>& root)
      : root(root) {
      assert(root != nullptr);
    }
    egg::lang::LogSeverity prepare(IEggEnginePreparationContext& preparation);
    egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution);
    static std::string unaryToString(EggProgramUnary op);
    static std::string binaryToString(EggProgramBinary op);
    static std::string assignToString(EggProgramAssign op);
    static std::string mutateToString(EggProgramMutate op);
    static const egg::lang::IType& VanillaArray;
    static const egg::lang::IType& VanillaObject;
    static const egg::lang::IType& VanillaException;
  };

  class EggProgramContext : public egg::lang::IExecution {
    EGG_NO_COPY(EggProgramContext);
  private:
    egg::lang::LocationRuntime location;
    IEggEngineLogger* logger;
    EggProgramSymbolTable* symtable;
    egg::lang::LogSeverity* maximumSeverity;
  public:
    EggProgramContext(EggProgramContext& parent, EggProgramSymbolTable& symtable)
      : location(parent.location), logger(parent.logger), symtable(&symtable), maximumSeverity(parent.maximumSeverity) {
    }
    EggProgramContext(IEggEngineLogger& logger, EggProgramSymbolTable& symtable, egg::lang::LogSeverity& maximumSeverity)
      : location(), logger(&logger), symtable(&symtable), maximumSeverity(&maximumSeverity) {
    }
    void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message);
    std::unique_ptr<IEggProgramAssignee> assigneeIdentifier(const IEggProgramNode& self, const egg::lang::String& name);
    std::unique_ptr<IEggProgramAssignee> assigneeBrackets(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const std::shared_ptr<IEggProgramNode>& index);
    std::unique_ptr<IEggProgramAssignee> assigneeDot(const IEggProgramNode& self, const std::shared_ptr<IEggProgramNode>& instance, const std::shared_ptr<IEggProgramNode>& property);
    void statement(const IEggProgramNode& node); // TODO remove?
    egg::lang::LocationRuntime swapLocation(const egg::lang::LocationRuntime& loc); // TODO remove?
    egg::lang::Value get(const egg::lang::String& name);
    egg::lang::Value set(const egg::lang::String& name, const egg::lang::Value& rvalue);
    egg::lang::Value assign(EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue);
    egg::lang::Value mutate(EggProgramMutate op, const IEggProgramNode& lvalue);
    egg::lang::Value condition(const IEggProgramNode& expression);
    egg::lang::Value unary(EggProgramUnary op, const IEggProgramNode& value);
    egg::lang::Value binary(EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::lang::Value call(const egg::lang::Value& callee, const egg::lang::IParameters& parameters);
    egg::lang::Value cast(egg::lang::Discriminator tag, const egg::lang::IParameters& parameters);
    egg::lang::Value createVanillaArray();
    egg::lang::Value createVanillaObject();
    // Inherited via IExecution
    virtual egg::lang::Value raise(const egg::lang::String& message) override;
    virtual egg::lang::Value assertion(const egg::lang::Value& predicate) override;
    virtual void print(const std::string& utf8) override;
    // Implemented in egg-execute.cpp
    egg::lang::Value executeModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeDeclare(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::IType& type, const IEggProgramNode* rvalue);
    egg::lang::Value executeAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue);
    egg::lang::Value executeMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue);
    egg::lang::Value executeBreak(const IEggProgramNode& self);
    egg::lang::Value executeCatch(const IEggProgramNode& self, const egg::lang::String& name, const IEggProgramNode& type, const IEggProgramNode& block, const egg::lang::Value& exception);
    egg::lang::Value executeContinue(const IEggProgramNode& self);
    egg::lang::Value executeDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::lang::Value executeIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock);
    egg::lang::Value executeFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block);
    egg::lang::Value executeForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block);
    egg::lang::Value executeFunctionDefinition(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::IType& type, const std::shared_ptr<IEggProgramNode>& block);
    egg::lang::Value executeFunctionCall(const egg::lang::IType& type, const egg::lang::IParameters& parameters, const IEggProgramNode& block);
    egg::lang::Value executeReturn(const IEggProgramNode& self, const IEggProgramNode* value);
    egg::lang::Value executeCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block, const egg::lang::Value* against);
    egg::lang::Value executeSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases);
    egg::lang::Value executeThrow(const IEggProgramNode& self, const IEggProgramNode* exception);
    egg::lang::Value executeTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final);
    egg::lang::Value executeUsing(const IEggProgramNode& self, const IEggProgramNode& value, const IEggProgramNode& block);
    egg::lang::Value executeWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    egg::lang::Value executeYield(const IEggProgramNode& self, const IEggProgramNode& value);
    egg::lang::Value executeArray(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::lang::Value executeObject(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    egg::lang::Value executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    egg::lang::Value executeCast(const IEggProgramNode& self, egg::lang::Discriminator tag, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    egg::lang::Value executeIdentifier(const IEggProgramNode& self, const egg::lang::String& name);
    egg::lang::Value executeLiteral(const IEggProgramNode& self, const egg::lang::Value& value);
    egg::lang::Value executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& value);
    egg::lang::Value executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::lang::Value executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse);
    // Implemented in egg-prepare.cpp
    EggProgramNodeFlags unimplemented(const std::string& function); // WIBBLE
    EggProgramNodeFlags prepareModule(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareBlock(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    EggProgramNodeFlags prepareDeclare(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::IType& type, const IEggProgramNode* rvalue);
    EggProgramNodeFlags prepareAssign(const IEggProgramNode& self, EggProgramAssign op, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue);
    EggProgramNodeFlags prepareMutate(const IEggProgramNode& self, EggProgramMutate op, const IEggProgramNode& lvalue);
    EggProgramNodeFlags prepareBreak(const IEggProgramNode& self);
    EggProgramNodeFlags prepareCatch(const IEggProgramNode& self, const egg::lang::String& name, const IEggProgramNode& type, const IEggProgramNode& block, const egg::lang::Value& exception);
    EggProgramNodeFlags prepareContinue(const IEggProgramNode& self);
    EggProgramNodeFlags prepareDo(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    EggProgramNodeFlags prepareIf(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& trueBlock, const IEggProgramNode* falseBlock);
    EggProgramNodeFlags prepareFor(const IEggProgramNode& self, const IEggProgramNode* pre, const IEggProgramNode* cond, const IEggProgramNode* post, const IEggProgramNode& block);
    EggProgramNodeFlags prepareForeach(const IEggProgramNode& self, const IEggProgramNode& lvalue, const IEggProgramNode& rvalue, const IEggProgramNode& block);
    EggProgramNodeFlags prepareFunctionDefinition(const IEggProgramNode& self, const egg::lang::String& name, const egg::lang::IType& type, const std::shared_ptr<IEggProgramNode>& block);
    EggProgramNodeFlags prepareFunctionCall(const egg::lang::IType& type, const egg::lang::IParameters& parameters, const IEggProgramNode& block);
    EggProgramNodeFlags prepareReturn(const IEggProgramNode& self, const IEggProgramNode* value);
    EggProgramNodeFlags prepareCase(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values, const IEggProgramNode& block, const egg::lang::Value* against);
    EggProgramNodeFlags prepareSwitch(const IEggProgramNode& self, const IEggProgramNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggProgramNode>>& cases);
    EggProgramNodeFlags prepareThrow(const IEggProgramNode& self, const IEggProgramNode* exception);
    EggProgramNodeFlags prepareTry(const IEggProgramNode& self, const IEggProgramNode& block, const std::vector<std::shared_ptr<IEggProgramNode>>& catches, const IEggProgramNode* final);
    EggProgramNodeFlags prepareUsing(const IEggProgramNode& self, const IEggProgramNode& value, const IEggProgramNode& block);
    EggProgramNodeFlags prepareWhile(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& block);
    EggProgramNodeFlags prepareYield(const IEggProgramNode& self, const IEggProgramNode& value);
    EggProgramNodeFlags prepareArray(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    EggProgramNodeFlags prepareObject(const IEggProgramNode& self, const std::vector<std::shared_ptr<IEggProgramNode>>& values);
    EggProgramNodeFlags prepareCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    EggProgramNodeFlags prepareCast(const IEggProgramNode& self, egg::lang::Discriminator tag, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    EggProgramNodeFlags prepareIdentifier(const IEggProgramNode& self, const egg::lang::String& name);
    EggProgramNodeFlags prepareLiteral(const IEggProgramNode& self, const egg::lang::Value& value);
    EggProgramNodeFlags prepareUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& value);
    EggProgramNodeFlags prepareBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    EggProgramNodeFlags prepareTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse);
  private:
    bool findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeFinally(const egg::lang::Value& retval, const IEggProgramNode* block);
    egg::lang::Value executeForeachString(IEggProgramAssignee& target, const egg::lang::String& source, const IEggProgramNode& block);
    egg::lang::Value executeForeachIterate(IEggProgramAssignee& target, egg::lang::IObject& source, const IEggProgramNode& block);
    bool operand(egg::lang::Value& dst, const IEggProgramNode& src, egg::lang::Discriminator expected, const char* expectation);
    typedef egg::lang::Value(*ArithmeticInt)(int64_t lhs, int64_t rhs);
    typedef egg::lang::Value (*ArithmeticFloat)(double lhs, double rhs);
    egg::lang::Value operatorDot(const egg::lang::Value& lhs, const egg::yolk::IEggProgramNode& rhs);
    egg::lang::Value operatorBrackets(const egg::lang::Value& lhs, const egg::yolk::IEggProgramNode& rhs);
    egg::lang::Value arithmeticIntFloat(const egg::lang::Value& lhs, const egg::yolk::IEggProgramNode& rvalue, const char* operation, ArithmeticInt ints, ArithmeticFloat floats);
    egg::lang::Value arithmeticInt(const egg::lang::Value& lhs, const egg::yolk::IEggProgramNode& rvalue, const char* operation, ArithmeticInt ints);
    egg::lang::Value unexpected(const std::string& expectation, const egg::lang::Value& value);
  };
}
