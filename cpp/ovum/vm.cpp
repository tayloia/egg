#include "ovum/ovum.h"

#include <stack>

namespace {
  using namespace egg::ovum;

  class VMProgramRunner;

  template<typename T>
  class VMCommon : public T {
    VMCommon(const VMCommon&) = delete;
    VMCommon& operator=(const VMCommon&) = delete;
  protected:
    IVM& vm;
  public:
    explicit VMCommon(IVM& vm)
      : vm(vm) {
    }
    virtual void log(ILogger::Source source, ILogger::Severity severity, const String& message) {
      this->vm.getLogger().log(source, severity, message);
    }
    virtual String createStringUTF8(const void* utf8, size_t bytes, size_t codepoints) override {
      return this->vm.createStringUTF8(utf8, bytes, codepoints);
    }
    virtual String createStringUTF32(const void* utf32, size_t codepoints) override {
      return this->vm.createStringUTF32(utf32, codepoints);
    }
    virtual Value createValueVoid() override {
      return this->vm.createValueVoid();
    }
    virtual Value createValueNull() override {
      return this->vm.createValueNull();
    }
    virtual Value createValueBool(Bool value) override {
      return this->vm.createValueBool(value);
    }
    virtual Value createValueInt(Int value) override {
      return this->vm.createValueInt(value);
    }
    virtual Value createValueFloat(Float value) override {
      return this->vm.createValueFloat(value);
    }
    virtual Value createValueString(const String& value) override {
      return this->vm.createValueString(value);
    }
    virtual Value createValueObject(const Object& value) override {
      return this->vm.createValueObject(value);
    }
  };

  template<typename T>
  class VMImpl : public VMCommon<T> {
    VMImpl(const VMImpl&) = delete;
    VMImpl& operator=(const VMImpl&) = delete;
  protected:
    mutable Atomic<int64_t> atomic; // signed so we can detect underflows
    explicit VMImpl(IVM& vm)
      : VMCommon<T>(vm), atomic(0) {
    }
  public:
    virtual ~VMImpl() override {
      // Make sure our reference count reached zero
      assert(this->atomic.get() == 0);
    }
    virtual T* hardAcquire() const override {
      auto count = this->atomic.increment();
      assert(count > 0);
      (void)count;
      return const_cast<T*>(static_cast<const T*>(this));
    }
    virtual void hardRelease() const override {
      auto count = this->atomic.decrement();
      assert(count >= 0);
      if (count == 0) {
        this->vm.getAllocator().destroy(this);
      }
    }
    virtual IAllocator& getAllocator() const override {
      return this->vm.getAllocator();
    }
  };

  class VMProgram : public VMImpl<IVMProgram> {
    VMProgram(const VMProgram&) = delete;
    VMProgram& operator=(const VMProgram&) = delete;
  private:
    HardPtr<Node> root;
  public:
    VMProgram(IVM& vm, Node& root)
      : VMImpl(vm), root(&root) {
    }
    virtual HardPtr<IVMProgramRunner> createRunner() override {
      return VMProgram::makeHardVM<VMProgramRunner>(this->vm, *this);
    }
    const Node& getRunnableRoot() const {
      return *this->root;
    }
    template<typename TYPE, typename... ARGS>
    static HardPtr<TYPE> makeHardVM(IVM& vm, ARGS&&... args) {
      // Use perfect forwarding
      return HardPtr<TYPE>(vm.getAllocator().makeRaw<TYPE>(vm, std::forward<ARGS>(args)...));
    }
  };
}

class egg::ovum::IVMProgram::Node : public VMImpl<IVMBase> {
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
private:
  HardPtr<Node> chain; // A linked list of all known nodes in this program
public:
  enum class Kind {
    Root,
    ExprVariable,
    ExprLiteral,
    StmtVariableDeclare,
    StmtVariableDefine,
    StmtVariableSet,
    StmtVariableUndeclare,
    StmtFunctionCall,
  };
  Kind kind;
  Value value;
  std::vector<Node*> children;
  Node(IVM& vm, Node* parent, Kind kind)
    : VMImpl(vm), chain(nullptr), kind(kind) {
    if (parent != nullptr) {
      this->chain = parent->chain;
      parent->chain.set(this);
    }
  }
  void addChild(Node& child) {
    this->children.push_back(&child);
  }
  Node& build() {
    // TODO
    return *this;
  }
};

namespace {
  using namespace egg::ovum;

  class VMProgramBuilder : public VMImpl<IVMProgramBuilder> {
    VMProgramBuilder(const VMProgramBuilder&) = delete;
    VMProgramBuilder& operator=(const VMProgramBuilder&) = delete;
  private:
    HardPtr<Node> root;
  public:
    explicit VMProgramBuilder(IVM& vm)
      : VMImpl(vm) {
      this->root = VMProgram::makeHardVM<Node>(this->vm, nullptr, Node::Kind::Root);
    }
    virtual void addStatement(Node& statement) override {
      assert(this->root != nullptr);
      this->root->addChild(statement.build());
    }
    virtual HardPtr<IVMProgram> build() override {
      assert(this->root != nullptr);
      auto program = VMProgram::makeHardVM<VMProgram>(this->vm, *this->root);
      this->root = nullptr;
      return program;
    }
    virtual Node& exprVariable(const String& name) override {
      auto& node = this->makeNode(Node::Kind::ExprVariable);
      node.value = this->createValueString(name);
      return node;
    }
    virtual Node& exprLiteral(const Value& literal) override {
      auto& node = this->makeNode(Node::Kind::ExprLiteral);
      node.value = literal;
      return node;
    }
    virtual Node& stmtVariableDeclare(const String& name) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableDeclare);
      node.value = this->createValueString(name);
      return node;
    }
    virtual Node& stmtVariableDefine(const String& name) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableDefine);
      node.value = this->createValueString(name);
      return node;
    }
    virtual Node& stmtVariableSet(const String& name) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableSet);
      node.value = this->createValueString(name);
      return node;
    }
    virtual Node& stmtVariableUndeclare(const String& name) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableUndeclare);
      node.value = this->createValueString(name);
      return node;
    }
    virtual Node& stmtFunctionCall(Node& function) override {
      auto& node = this->makeNode(Node::Kind::StmtFunctionCall);
      node.addChild(function);
      return node;
    }
  private:
    virtual void appendChild(Node& parent, Node& child) override {
      parent.addChild(child);
    }
    Node& makeNode(Node::Kind kind) {
      assert(this->root != nullptr);
      auto node = VMProgram::makeHardVM<Node>(this->vm, this->root.get(), kind);
      return *node;
    }
  };

  class VMProgramRunner : public VMImpl<IVMProgramRunner> {
    VMProgramRunner(const VMProgramRunner&) = delete;
    VMProgramRunner& operator=(const VMProgramRunner&) = delete;
  private:
    struct NodeStack {
      const IVMProgram::Node* node;
      size_t index;
      std::deque<Value> deque;
    };
    enum class SymbolKind {
      Unknown,
      Builtin,
      Unset,
      Variable
    };
    class SymbolTable {
      SymbolTable(const SymbolTable&) = delete;
      SymbolTable& operator=(const SymbolTable&) = delete;
    private:
      struct Entry {
        SymbolKind kind;
        Value value;
      };
      std::map<String, Entry> entries;
    public:
      SymbolTable() = default;
      SymbolKind add(SymbolKind kind, const String& name, const Value& value) {
        // Returns the old kind before this request
        assert(kind != SymbolKind::Unknown);
        auto result = this->entries.emplace(name, Entry(kind, value));
        if (result.second) {
          return SymbolKind::Unknown;
        }
        assert(result.first->second.kind != SymbolKind::Unknown);
        return result.first->second.kind;
      }
      SymbolKind set(const String& name, const Value& value) {
        // Returns the new kind but only updates if a variable (or unset)
        auto result = this->entries.find(name);
        if (result == this->entries.end()) {
          return SymbolKind::Unknown;
        }
        switch (result->second.kind) {
        case SymbolKind::Unknown:
          return SymbolKind::Unknown;
        case SymbolKind::Builtin:
          // Can reset a builtin
          return SymbolKind::Builtin;
        case SymbolKind::Unset:
          result->second.kind = SymbolKind::Variable;
          break;
        case SymbolKind::Variable:
          break;
        }
        result->second.value = value;
        return SymbolKind::Variable;
      }
      SymbolKind remove(const String& name) {
        // Returns the old kind but only removes if variable or unset
        auto result = this->entries.find(name);
        if (result == this->entries.end()) {
          return SymbolKind::Unknown;
        }
        auto kind = result->second.kind;
        if (kind != SymbolKind::Builtin) {
          this->entries.erase(result);
        }
        return kind;
      }
      SymbolKind lookup(const String& name) {
        // Returns the current kind
        auto result = this->entries.find(name);
        if (result == this->entries.end()) {
          return SymbolKind::Unknown;
        }
        return result->second.kind;
      }
      SymbolKind lookup(const String& name, Value& value) {
        // Returns the current kind and current value
        auto result = this->entries.find(name);
        if (result == this->entries.end()) {
          return SymbolKind::Unknown;
        }
        value = result->second.value;
        return result->second.kind;
      }
    };
    HardPtr<VMProgram> program;
    std::stack<NodeStack> stack;
    SymbolTable symtable;
    VMCommon<IVMExecution> execution;
  public:
    VMProgramRunner(IVM& vm, VMProgram& program)
      : VMImpl(vm),
        program(&program),
        execution(vm) {
      this->push(program.getRunnableRoot());
    }
    virtual void addBuiltin(const String& name, const Value& value) override {
      auto added = this->symtable.add(SymbolKind::Builtin, name, value);
      assert(added == SymbolKind::Unknown);
      (void)added;
    }
    virtual RunOutcome run(Value& retval, RunFlags flags) override {
      if (flags == RunFlags::Step) {
        return this->step(retval);
      }
      if (flags != RunFlags::None) {
        return this->createFault(retval, "TODO: Run flags not yet supported in program runner");
      }
      for (;;) {
        auto outcome = this->step(retval);
        if (outcome != RunOutcome::Stepped) {
          return outcome;
        }
      }
    }
  private:
    RunOutcome step(Value& retval);
    void push(const IVMProgram::Node& node, size_t index = 0) {
      this->stack.emplace(&node, index);
    }
    void pop(const Value& value) {
      assert(!this->stack.empty());
      this->stack.pop();
      assert(!this->stack.empty());
      this->stack.top().deque.push_back(value);
    }
    Value createThrow(const Value& value) {
      return ValueFactory::createFlowControl(this->getAllocator(), ValueFlags::Throw, value);
    }
    template<typename... ARGS>
    RunOutcome createFault(Value& retval, ARGS&&... args) {
      auto reason = this->createStringConcat(std::forward<ARGS>(args)...);
      retval = this->createThrow(ValueFactory::createString(this->getAllocator(), reason));
      return RunOutcome::Faulted;
    }
    template<typename... ARGS>
    String createStringConcat(ARGS&&... args) {
      StringBuilder sb(&this->getAllocator());
      return sb.add(std::forward<ARGS>(args)...).build();
    }
  };

  class VMDefault : public HardReferenceCountedAllocator<IVM> {
    VMDefault(const VMDefault&) = delete;
    VMDefault& operator=(const VMDefault&) = delete;
  private:
    HardPtr<IBasket> basket;
    ILogger& logger;
  public:
    VMDefault(IAllocator& allocator, ILogger& logger)
      : HardReferenceCountedAllocator<IVM>(allocator),
        basket(BasketFactory::createBasket(allocator)),
        logger(logger) {
    }
    virtual IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual ILogger& getLogger() const override {
      return this->logger;
    }
    virtual String createStringUTF8(const void* utf8, size_t bytes, size_t codepoints) override {
      return String::fromUTF8(&this->allocator, utf8, bytes, codepoints);
    }
    virtual String createStringUTF32(const void* utf32, size_t codepoints) override {
      return String::fromUTF32(&this->allocator, utf32, codepoints);
    }
    virtual HardPtr<IVMProgramBuilder> createProgramBuilder() override {
      return HardPtr<IVMProgramBuilder>(this->allocator.makeRaw<VMProgramBuilder>(*this));
    }
    virtual Value createValueVoid() override {
      return Value::Void;
    }
    virtual Value createValueNull() override {
      return Value::Null;
    }
    virtual Value createValueBool(Bool value) override {
      return value ? Value::True : Value::False;
    }
    virtual Value createValueInt(Int value) override {
      return ValueFactory::createInt(this->allocator, value);
    }
    virtual Value createValueFloat(Float value) override {
      return ValueFactory::createFloat(this->allocator, value);
    }
    virtual Value createValueString(const String& value) override {
      return ValueFactory::createString(this->allocator, value);
    }
    virtual Value createValueObject(const Object& value) override {
      return ValueFactory::createObject(this->allocator, value);
    }
    virtual Object createBuiltinPrint() override {
      return ObjectFactory::createBuiltinPrint(*this);
    }
  };
}

egg::ovum::IVMProgramRunner::RunOutcome VMProgramRunner::step(Value& retval) {
  auto& top = this->stack.top();
  assert(top.index <= top.node->children.size());
  switch (top.node->kind) {
  case IVMProgram::Node::Kind::Root:
    assert(top.node->value.isVoid());
    if (top.index < top.node->children.size()) {
      // Execute all the statements
      this->push(*top.node->children[top.index++]);
    } else {
      // Reached the end of the list of statements in the program
      retval = Value::Void;
      return RunOutcome::Completed;
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableDeclare:
    assert(top.node->children.empty());
    assert(top.deque.empty());
    {
      String symbol;
      if (!top.node->value->getString(symbol)) {
        return this->createFault(retval, "Invalid program node value for variable symbol");
      }
      switch (this->symtable.add(SymbolKind::Unset, symbol, Value::Void)) {
      case SymbolKind::Unknown:
        break;
      case SymbolKind::Builtin:
        return this->createFault(retval, "Variable symbol already declared as a builtin: '", symbol, "'");
      case SymbolKind::Unset:
      case SymbolKind::Variable:
        return this->createFault(retval, "Variable symbol already declared: '", symbol, "'");
      }
      this->pop(Value::Void);
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableDefine:
    assert(top.node->children.size() == 1);
    if (top.index == 0) {
      // Evaluate the expression
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      String symbol;
      if (!top.node->value->getString(symbol)) {
        return this->createFault(retval, "Invalid program node value for variable symbol");
      }
      switch (this->symtable.add(SymbolKind::Variable, symbol, top.deque.front())) {
      case SymbolKind::Unknown:
        break;
      case SymbolKind::Builtin:
        return this->createFault(retval, "Variable symbol already declared as a builtin: '", symbol, "'");
      case SymbolKind::Unset:
      case SymbolKind::Variable:
        return this->createFault(retval, "Variable symbol already declared: '", symbol, "'");
      }
      this->pop(Value::Void);
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableSet:
    assert(top.node->children.size() == 1);
    if (top.index == 0) {
      // Evaluate the expression
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      String symbol;
      if (!top.node->value->getString(symbol)) {
        return this->createFault(retval, "Invalid program node value for variable symbol");
      }
      switch (this->symtable.set(symbol, top.deque.front())) {
      case SymbolKind::Unknown:
        return this->createFault(retval, "Unknown variable symbol: '", symbol, "'");
      case SymbolKind::Builtin:
        return this->createFault(retval, "Cannot modify builtin symbol: '", symbol, "'");
      case SymbolKind::Variable:
      case SymbolKind::Unset:
        break;
      }
      this->pop(Value::Void);
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableUndeclare:
    assert(top.node->children.empty());
    assert(top.deque.empty());
    {
      String symbol;
      if (!top.node->value->getString(symbol)) {
        return this->createFault(retval, "Invalid program node value for variable symbol");
      }
      switch (this->symtable.remove(symbol)) {
      case SymbolKind::Unknown:
        return this->createFault(retval, "Unknown variable symbol: '", symbol, "'");
      case SymbolKind::Builtin:
        return this->createFault(retval, "Cannot undeclare builtin symbol: '", symbol, "'");
      case SymbolKind::Unset:
      case SymbolKind::Variable:
        break;
      }
      this->pop(Value::Void);
    }
    break;
  case IVMProgram::Node::Kind::StmtFunctionCall:
    assert(top.node->value.isVoid());
    if (top.index < top.node->children.size()) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the function call
      assert(top.deque.size() >= 1);
      Object function;
      if (!top.deque.front()->getObject(function)) {
        return this->createFault(retval, "Invalid initial program node value for function call");
      }
      top.deque.pop_front();
      CallArguments arguments;
      for (auto& argument : top.deque) {
        // TODO support named arguments
        arguments.addUnnamed(argument);
      }
      this->pop(function->call(this->execution, arguments));
    }
    break;
  case IVMProgram::Node::Kind::ExprVariable:
    assert(top.node->children.empty());
    {
      String symbol;
      if (!top.node->value->getString(symbol)) {
        return this->createFault(retval, "Invalid program node value for variable symbol");
      }
      Value value;
      switch (this->symtable.lookup(symbol, value)) {
      case SymbolKind::Unknown:
        return this->createFault(retval, "Unknown variable symbol: '", symbol, "'");
      case SymbolKind::Unset:
        return this->createFault(retval, "Variable uninitialized: '", symbol, "'");
      case SymbolKind::Builtin:
      case SymbolKind::Variable:
        break;
      }
      this->pop(value);
    }
    break;
  case IVMProgram::Node::Kind::ExprLiteral:
    assert(top.node->children.empty());
    this->pop(top.node->value);
    break;
  default:
    return this->createFault(retval, "Invalid program node kind in program runner");
  }
  return RunOutcome::Stepped;
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createDefault(IAllocator& allocator, ILogger& logger) {
  return allocator.makeHard<VMDefault>(logger);
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createTest(IAllocator& allocator, ILogger& logger) {
  return allocator.makeHard<VMDefault>(logger);
}
