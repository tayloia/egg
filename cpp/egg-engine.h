namespace egg::yolk {
  class IEggEngineLogger {
  public:
    using LogSource = egg::lang::LogSource;
    using LogSeverity = egg::lang::LogSeverity;
    virtual void log(LogSource source, LogSeverity severity, const std::string& message) = 0;
  };

  class IEggEnginePreparationContext : public IEggEngineLogger {
  public:
    virtual LogSeverity getMaximumSeverity() const = 0;
  };

  class IEggEngineExecutionContext : public IEggEngineLogger {
  public:
    virtual LogSeverity getMaximumSeverity() const = 0;

    // TODO
    virtual void print(const std::string& text) = 0;

    // Decoupled entry points
    egg::lang::ExecutionResult executeModule(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& statements);
    egg::lang::ExecutionResult executeBlock(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& statements);
    egg::lang::ExecutionResult executeType(const IEggParserNode& self, const IEggParserType& type);
    egg::lang::ExecutionResult executeDeclare(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode* rvalue);
    egg::lang::ExecutionResult executeAssign(const IEggParserNode& self, EggParserAssign op, const IEggParserNode& lvalue, const IEggParserNode& rvalue);
    egg::lang::ExecutionResult executeMutate(const IEggParserNode& self, EggParserMutate op, const IEggParserNode& lvalue);
    egg::lang::ExecutionResult executeBreak(const IEggParserNode& self);
    egg::lang::ExecutionResult executeCatch(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode& block);
    egg::lang::ExecutionResult executeContinue(const IEggParserNode& self);
    egg::lang::ExecutionResult executeDo(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block);
    egg::lang::ExecutionResult executeIf(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& trueBlock, const IEggParserNode* falseBlock);
    egg::lang::ExecutionResult executeFor(const IEggParserNode& self, const IEggParserNode* pre, const IEggParserNode* cond, const IEggParserNode* post, const IEggParserNode& block);
    egg::lang::ExecutionResult executeForeach(const IEggParserNode& self, const IEggParserNode& lvalue, const IEggParserNode& rvalue, const IEggParserNode& block);
    egg::lang::ExecutionResult executeReturn(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values);
    egg::lang::ExecutionResult executeCase(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values, const IEggParserNode& block);
    egg::lang::ExecutionResult executeSwitch(const IEggParserNode& self, const IEggParserNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggParserNode>>& cases);
    egg::lang::ExecutionResult executeThrow(const IEggParserNode& self, const IEggParserNode* exception);
    egg::lang::ExecutionResult executeTry(const IEggParserNode& self, const IEggParserNode& block, const std::vector<std::shared_ptr<IEggParserNode>>& catches, const IEggParserNode* final);
    egg::lang::ExecutionResult executeUsing(const IEggParserNode& self, const IEggParserNode& value, const IEggParserNode& block);
    egg::lang::ExecutionResult executeWhile(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block);
    egg::lang::ExecutionResult executeYield(const IEggParserNode& self, const IEggParserNode& value);
    egg::lang::ExecutionResult executeCall(const IEggParserNode& self, const IEggParserNode& callee, const std::vector<std::shared_ptr<IEggParserNode>>& parameters);
    egg::lang::ExecutionResult executeIdentifier(const IEggParserNode& self, const std::string& name);
    egg::lang::ExecutionResult executeLiteral(const IEggParserNode& self, ...);
    egg::lang::ExecutionResult executeLiteral(const IEggParserNode& self, const std::string& value);
    egg::lang::ExecutionResult executeUnary(const IEggParserNode& self, EggParserUnary op, const IEggParserNode& value);
    egg::lang::ExecutionResult executeBinary(const IEggParserNode& self, EggParserBinary op, const IEggParserNode& lhs, const IEggParserNode& rhs);
    egg::lang::ExecutionResult executeTernary(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& whenTrue, const IEggParserNode& whenFalse);
  };

  class IEggEngine {
  public:
    using Severity = egg::lang::LogSeverity;
    virtual Severity prepare(IEggEnginePreparationContext& preparation) = 0;
    virtual Severity execute(IEggEngineExecutionContext& execution) = 0;
  };

  class EggEngineFactory {
  public:
    static std::shared_ptr<IEggEnginePreparationContext> createPreparationContext(const std::shared_ptr<IEggEngineLogger>& logger);
    static std::shared_ptr<IEggEngineExecutionContext> createExecutionContext(const std::shared_ptr<IEggEngineLogger>& logger);
    static std::shared_ptr<IEggEngine> createEngineFromTextStream(TextStream& stream);
    static std::shared_ptr<IEggEngine> createEngineFromParsed(const std::shared_ptr<IEggParserNode>& root);
  };
}
