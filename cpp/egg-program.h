namespace egg::yolk {
  class IEggEngineExecutionContext;
  class EggProgramContext;

  class IEggProgramAssignee {
  public:
    virtual ~IEggProgramAssignee() {}
    virtual egg::lang::Value get() const = 0;
    virtual egg::lang::Value set(const egg::lang::Value& value) = 0;
  };

  class IEggProgramNode {
  public:
    virtual ~IEggProgramNode() {}
    virtual egg::lang::ITypeRef getType() const = 0;
    virtual egg::lang::LocationSource location() const = 0;
    virtual bool symbol(egg::lang::String& nameOut, egg::lang::ITypeRef& typeOut) const = 0;
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

  class EggProgram {
  public:
    class SymbolTable;
  private:
    std::shared_ptr<IEggProgramNode> root;
  public:
    explicit EggProgram(const std::shared_ptr<IEggProgramNode>& root)
      : root(root) {
      assert(root != nullptr);
    }
    egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution);
    egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution, SymbolTable& symtable);

    static std::string unaryToString(EggProgramUnary op);
    static std::string binaryToString(EggProgramBinary op);
    static std::string assignToString(EggProgramAssign op);
    static std::string mutateToString(EggProgramMutate op);
  };

  class EggProgramContext : public egg::lang::IExecution {
    EGG_NO_COPY(EggProgramContext);
  private:
    egg::lang::LocationRuntime location;
    IEggEngineExecutionContext* engine;
    EggProgram::SymbolTable* symtable;
    egg::lang::LogSeverity* maximumSeverity;
  public:
    EggProgramContext(EggProgramContext& parent, EggProgram::SymbolTable& symtable)
      : location(parent.location), engine(parent.engine), symtable(&symtable), maximumSeverity(parent.maximumSeverity) {
    }
    EggProgramContext(IEggEngineExecutionContext& execution, EggProgram::SymbolTable& symtable, egg::lang::LogSeverity& maximumSeverity)
      : location(), engine(&execution), symtable(&symtable), maximumSeverity(&maximumSeverity) {
    }
    void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message);
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
    egg::lang::Value executeCall(const IEggProgramNode& self, const IEggProgramNode& callee, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    egg::lang::Value executeCast(const IEggProgramNode& self, egg::lang::Discriminator tag, const std::vector<std::shared_ptr<IEggProgramNode>>& parameters);
    egg::lang::Value executeIdentifier(const IEggProgramNode& self, const egg::lang::String& name);
    egg::lang::Value executeLiteral(const IEggProgramNode& self, const egg::lang::Value& value);
    egg::lang::Value executeUnary(const IEggProgramNode& self, EggProgramUnary op, const IEggProgramNode& value);
    egg::lang::Value executeBinary(const IEggProgramNode& self, EggProgramBinary op, const IEggProgramNode& lhs, const IEggProgramNode& rhs);
    egg::lang::Value executeTernary(const IEggProgramNode& self, const IEggProgramNode& cond, const IEggProgramNode& whenTrue, const IEggProgramNode& whenFalse);
    std::unique_ptr<IEggProgramAssignee> assigneeIdentifier(const IEggProgramNode& self, const egg::lang::String& name);
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
    // Inherited via IExecution
    virtual egg::lang::Value raise(const egg::lang::String& message) override;
    virtual egg::lang::Value assertion(const egg::lang::Value& predicate) override;
    virtual void print(const std::string& utf8) override;
  private:
    bool findDuplicateSymbols(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeStatements(const std::vector<std::shared_ptr<IEggProgramNode>>& statements);
    egg::lang::Value executeFinally(const egg::lang::Value& retval, const IEggProgramNode* block);
    egg::lang::Value executeForeachString(IEggProgramAssignee& target, const egg::lang::String& source, const IEggProgramNode& block);
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
