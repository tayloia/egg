namespace egg::yolk {
  class EggEngineProgram {
  private:
    std::shared_ptr<IEggParserNode> root;
  public:
    explicit EggEngineProgram(const std::shared_ptr<IEggParserNode>& root)
      : root(root) {
      assert(root != nullptr);
    }
    egg::lang::LogSeverity execute(IEggEngineExecutionContext& execution);
  };

  class EggEngineProgramContext {
  public:
    class SymbolTable;
  private:
    IEggEngineExecutionContext* execution;
    SymbolTable* symtable;
    egg::lang::LogSeverity maximumSeverity;
  public:
    EggEngineProgramContext(IEggEngineExecutionContext& execution, SymbolTable& symtable)
      : execution(&execution), symtable(&symtable), maximumSeverity(egg::lang::LogSeverity::None) {
    }
    void log(egg::lang::LogSource source, egg::lang::LogSeverity severity, const std::string& message);
    egg::lang::LogSeverity getMaximumSeverity() const {
      return this->maximumSeverity;
    }
    egg::lang::Value executeModule(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& statements);
    egg::lang::Value executeBlock(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& statements);
    egg::lang::Value executeType(const IEggParserNode& self, const IEggParserType& type);
    egg::lang::Value executeDeclare(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode* rvalue);
    egg::lang::Value executeAssign(const IEggParserNode& self, EggParserAssign op, const IEggParserNode& lvalue, const IEggParserNode& rvalue);
    egg::lang::Value executeMutate(const IEggParserNode& self, EggParserMutate op, const IEggParserNode& lvalue);
    egg::lang::Value executeBreak(const IEggParserNode& self);
    egg::lang::Value executeCatch(const IEggParserNode& self, const std::string& name, const IEggParserNode& type, const IEggParserNode& block);
    egg::lang::Value executeContinue(const IEggParserNode& self);
    egg::lang::Value executeDo(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block);
    egg::lang::Value executeIf(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& trueBlock, const IEggParserNode* falseBlock);
    egg::lang::Value executeFor(const IEggParserNode& self, const IEggParserNode* pre, const IEggParserNode* cond, const IEggParserNode* post, const IEggParserNode& block);
    egg::lang::Value executeForeach(const IEggParserNode& self, const IEggParserNode& lvalue, const IEggParserNode& rvalue, const IEggParserNode& block);
    egg::lang::Value executeReturn(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values);
    egg::lang::Value executeCase(const IEggParserNode& self, const std::vector<std::shared_ptr<IEggParserNode>>& values, const IEggParserNode& block);
    egg::lang::Value executeSwitch(const IEggParserNode& self, const IEggParserNode& value, int64_t defaultIndex, const std::vector<std::shared_ptr<IEggParserNode>>& cases);
    egg::lang::Value executeThrow(const IEggParserNode& self, const IEggParserNode* exception);
    egg::lang::Value executeTry(const IEggParserNode& self, const IEggParserNode& block, const std::vector<std::shared_ptr<IEggParserNode>>& catches, const IEggParserNode* final);
    egg::lang::Value executeUsing(const IEggParserNode& self, const IEggParserNode& value, const IEggParserNode& block);
    egg::lang::Value executeWhile(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& block);
    egg::lang::Value executeYield(const IEggParserNode& self, const IEggParserNode& value);
    egg::lang::Value executeCall(const IEggParserNode& self, const IEggParserNode& callee, const std::vector<std::shared_ptr<IEggParserNode>>& parameters);
    egg::lang::Value executeIdentifier(const IEggParserNode& self, const std::string& name);
    egg::lang::Value executeLiteral(const IEggParserNode& self, ...);
    egg::lang::Value executeLiteral(const IEggParserNode& self, const std::string& value);
    egg::lang::Value executeUnary(const IEggParserNode& self, EggParserUnary op, const IEggParserNode& value);
    egg::lang::Value executeBinary(const IEggParserNode& self, EggParserBinary op, const IEggParserNode& lhs, const IEggParserNode& rhs);
    egg::lang::Value executeTernary(const IEggParserNode& self, const IEggParserNode& condition, const IEggParserNode& whenTrue, const IEggParserNode& whenFalse);
    void statement(const IEggParserNode& node);
    void expression(const IEggParserNode& node);
    void set(const std::string& name, const IEggParserNode& rvalue);
    void assign(EggParserAssign op, const IEggParserNode& lvalue, const IEggParserNode& rvalue);
  };
}
