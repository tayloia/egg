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
    virtual HardValue createHardValueVoid() override {
      return this->vm.createHardValueVoid();
    }
    virtual HardValue createHardValueNull() override {
      return this->vm.createHardValueNull();
    }
    virtual HardValue createHardValueBool(Bool value) override {
      return this->vm.createHardValueBool(value);
    }
    virtual HardValue createHardValueInt(Int value) override {
      return this->vm.createHardValueInt(value);
    }
    virtual HardValue createHardValueFloat(Float value) override {
      return this->vm.createHardValueFloat(value);
    }
    virtual HardValue createHardValueString(const String& value) override {
      return this->vm.createHardValueString(value);
    }
    virtual HardValue createHardValueObject(const HardObject& value) override {
      return this->vm.createHardValueObject(value);
    }
  };

  template<typename T>
  class VMCollectable : public VMCommon<SoftReferenceCounted<T>> {
    VMCollectable(const VMCollectable&) = delete;
    VMCollectable& operator=(const VMCollectable&) = delete;
  protected:
    explicit VMCollectable(IVM& vm)
      : VMCommon<SoftReferenceCounted<T>>(vm) {
    }
    virtual void hardDestroy() const override {
      this->vm.getAllocator().destroy(this);
    }
  public:
    virtual IAllocator& getAllocator() const override {
      return this->vm.getAllocator();
    }
  };

  class VMProgram : public VMCollectable<IVMProgram> {
    VMProgram(const VMProgram&) = delete;
    VMProgram& operator=(const VMProgram&) = delete;
  private:
    HardPtr<Node> root;
  public:
    VMProgram(IVM& vm, Node& root)
      : VMCollectable(vm), root(&root) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // TODO any soft links?
    }
    virtual void print(Printer& printer) const override {
      printer << "[VMProgram]";
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

  // Only instantiated by composition within 'VMProgramRunner' etc.
  struct VMSymbolTable {
  public:
    enum class Kind {
      Unknown,
      Builtin,
      Unset,
      Variable
    };
  private:
    struct Entry {
      Kind kind;
      HardValue value;
    };
    std::map<String, Entry> entries;
  public:
    Kind add(Kind kind, const String& name, const HardValue& value) {
      // Returns the old kind before this request
      assert(kind != Kind::Unknown);
      auto result = this->entries.emplace(name, Entry(kind, value));
      if (result.second) {
        return Kind::Unknown;
      }
      assert(result.first->second.kind != Kind::Unknown);
      return result.first->second.kind;
    }
    Kind set(const String& name, const HardValue& value) {
      // Returns the new kind but only updates if a variable (or unset)
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return Kind::Unknown;
      }
      switch (result->second.kind) {
      case Kind::Unknown:
        return Kind::Unknown;
      case Kind::Builtin:
        // Can reset a builtin
        return Kind::Builtin;
      case Kind::Unset:
        result->second.kind = Kind::Variable;
        break;
      case Kind::Variable:
        break;
      }
      result->second.value = value;
      return Kind::Variable;
    }
    Kind remove(const String& name) {
      // Returns the old kind but only removes if variable or unset
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return Kind::Unknown;
      }
      auto kind = result->second.kind;
      if (kind != Kind::Builtin) {
        this->entries.erase(result);
      }
      return kind;
    }
    Kind lookup(const String& name) {
      // Returns the current kind
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return Kind::Unknown;
      }
      return result->second.kind;
    }
    Kind lookup(const String& name, HardValue& value) {
      // Returns the current kind and current value
      auto result = this->entries.find(name);
      if (result == this->entries.end()) {
        return Kind::Unknown;
      }
      value = result->second.value;
      return result->second.kind;
    }
  };

  // Only instantiated by composition within 'VMProgramRunner' etc.
  class VMExecution : public VMCommon<IVMExecution> {
    VMExecution(const VMExecution&) = delete;
    VMExecution& operator=(const VMExecution&) = delete;
  public:
    explicit VMExecution(IVM& vm)
      : VMCommon<IVMExecution>(vm) {
    }
    virtual HardValue raiseException(const String& message) override {
      // TODO augment with runtime metadata
      auto& allocator = this->vm.getAllocator();
      auto inner = ValueFactory::createString(allocator, message);
      return ValueFactory::createHardFlowControl(allocator, ValueFlags::Throw, inner);
    }
    virtual HardValue evaluateBinaryOp(BinaryOp op, const HardValue& lhs, const HardValue& rhs) override {
      BinaryValues binaryValues;
      switch (op) {
      case BinaryOp::Add:
        switch (promoteBinary(binaryValues, lhs, rhs)) {
        case BinaryResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] + binaryValues.i[1]);
        case BinaryResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] + binaryValues.f[1]);
        case BinaryResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '+' addition binary operator");
        case BinaryResult::BadRight:
          return this->raise("TODO: Invalid right-hand value in '+' addition binary operator");
        }
        break;
      case BinaryOp::Sub:
        switch (promoteBinary(binaryValues, lhs, rhs)) {
        case BinaryResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] - binaryValues.i[1]);
        case BinaryResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] - binaryValues.f[1]);
        case BinaryResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '-' subtraction binary operator");
        case BinaryResult::BadRight:
          return this->raise("TODO: Invalid right-hand value in '-' subtraction binary operator");
        }
        break;
      case BinaryOp::Mul:
        switch (promoteBinary(binaryValues, lhs, rhs)) {
        case BinaryResult::Ints:
          return this->createHardValueInt(binaryValues.i[0] * binaryValues.i[1]);
        case BinaryResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] * binaryValues.f[1]);
        case BinaryResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '*' multiplication binary operator");
        case BinaryResult::BadRight:
          return this->raise("TODO: Invalid right-hand value in '*' multiplication binary operator");
        }
        break;
      case BinaryOp::Div:
        switch (promoteBinary(binaryValues, lhs, rhs)) {
        case BinaryResult::Ints:
          if (binaryValues.i[1] == 0) {
            return this->raise("TODO: Integer division by zero in '/' division binary operator");
          }
          return this->createHardValueInt(binaryValues.i[0] / binaryValues.i[1]);
        case BinaryResult::Floats:
          return this->createHardValueFloat(binaryValues.f[0] / binaryValues.f[1]);
        case BinaryResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '/' division binary operator");
        case BinaryResult::BadRight:
          return this->raise("TODO: Invalid right-hand value in '/' division binary operator");
        }
        break;
      case BinaryOp::Rem:
        switch (promoteBinary(binaryValues, lhs, rhs)) {
        case BinaryResult::Ints:
          if (binaryValues.i[1] == 0) {
            return this->raise("TODO: Integer division by zero in '%' remainder binary operator");
          }
          return this->createHardValueInt(binaryValues.i[0] % binaryValues.i[1]);
        case BinaryResult::Floats:
          return this->createHardValueFloat(std::fmod(binaryValues.f[0], binaryValues.f[1]));
        case BinaryResult::BadLeft:
          return this->raise("TODO: Invalid left-hand value for '%' remainder binary operator");
        case BinaryResult::BadRight:
          return this->raise("TODO: Invalid right-hand value in '%' remainder binary operator");
        }
        break;
      }
      return this->raise("TODO: Unknown binary operator");
    }
  private:
    union BinaryValues {
      Int i[2];
      Float f[2];
    };
    enum class BinaryResult {
      Ints,
      Floats,
      BadLeft,
      BadRight
    };
    static BinaryResult promoteBinary(BinaryValues& values, const HardValue& lhs, const HardValue& rhs) {
      Int i;
      if (lhs->getFloat(values.f[0])) {
        // Need to promote rhs to float
        if (rhs->getFloat(values.f[1])) {
          // Both values are already floats
          return BinaryResult::Floats;
        }
        if (rhs->getInt(i)) {
          // Left is float, right is int
          values.f[1] = Float(i);
          return BinaryResult::Floats;
        }
        return BinaryResult::BadRight;
      }
      if (lhs->getInt(i)) {
        // May need to promote lhs to float
        if (rhs->getFloat(values.f[1])) {
          // Left is int, right is float
          values.f[0] = Float(i);
          return BinaryResult::Floats;
        }
        if (rhs->getInt(values.i[1])) {
          // Both values are already ints
          values.i[0] = i;
          return BinaryResult::Ints;
        }
        return BinaryResult::BadRight;
      }
      return BinaryResult::BadLeft;
    }
  };
}

class egg::ovum::IVMProgram::Node : public HardReferenceCounted<IHardAcquireRelease> {
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
private:
  IVM& vm;
  HardPtr<Node> chain; // A linked list of all known nodes in this program
public:
  enum class Kind {
    Root,
    ExprBinaryOp,
    ExprVariable,
    ExprLiteral,
    ExprPropertyGet,
    ExprFunctionCall,
    StmtVariableDeclare,
    StmtVariableSet,
    StmtPropertySet,
    StmtFunctionCall
  };
  Kind kind;
  HardValue literal; // Only stores simple literals
  std::vector<Node*> children; // Reference-counting hard pointers are stored in the chain
  Node(IVM& vm, Node* parent, Kind kind)
    : HardReferenceCounted<IHardAcquireRelease>(),
      vm(vm),
      chain(nullptr),
      kind(kind) {
    if (parent != nullptr) {
      this->chain = parent->chain;
      parent->chain.set(this);
    }
  }
  void addChild(Node& child) {
    this->children.push_back(&child);
  }
  void addChildren() {
    // Do nothing
  }
  template<typename... ARGS>
  void addChildren(Node& head, ARGS&&... tail) {
    this->addChild(head);
    this->addChildren(std::forward<ARGS>(tail)...);
  }
protected:
  virtual void hardDestroy() const override {
    this->vm.getAllocator().destroy(this);
  }
};

namespace {
  using namespace egg::ovum;

  class VMProgramBuilder : public VMCollectable<IVMProgramBuilder> {
    VMProgramBuilder(const VMProgramBuilder&) = delete;
    VMProgramBuilder& operator=(const VMProgramBuilder&) = delete;
  private:
    HardPtr<Node> root;
  public:
    explicit VMProgramBuilder(IVM& vm)
      : VMCollectable<IVMProgramBuilder>(vm) {
      this->root = VMProgram::makeHardVM<Node>(this->vm, nullptr, Node::Kind::Root);
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // TODO any soft links?
    }
    virtual void print(Printer& printer) const override {
      printer << "[VMProgramBuilder]";
    }
    virtual void addStatement(Node& statement) override {
      assert(this->root != nullptr);
      this->root->addChild(statement);
    }
    virtual HardPtr<IVMProgram> build() override {
      assert(this->root != nullptr);
      auto program = VMProgram::makeHardVM<VMProgram>(this->vm, *this->root);
      this->root = nullptr;
      return program;
    }
    virtual Node& exprBinaryOp(IVMExecution::BinaryOp op, Node& lhs, Node& rhs) override {
      auto& node = this->makeNode(Node::Kind::ExprBinaryOp);
      node.literal = this->createHardValueInt(Int(op));
      node.addChild(lhs);
      node.addChild(rhs);
      return node;
    }
    virtual Node& exprVariable(const String& name) override {
      auto& node = this->makeNode(Node::Kind::ExprVariable);
      node.literal = this->createHardValueString(name);
      return node;
    }
    virtual Node& exprLiteral(const HardValue& literal) override {
      auto& node = this->makeNode(Node::Kind::ExprLiteral);
      node.literal = literal;
      return node;
    }
    virtual Node& exprPropertyGet(Node& instance, Node& property) override {
      auto& node = this->makeNode(Node::Kind::ExprPropertyGet);
      node.addChild(instance);
      node.addChild(property);
      return node;
    }
    virtual Node& exprFunctionCall(Node& function) override {
      auto& node = this->makeNode(Node::Kind::ExprFunctionCall);
      node.addChild(function);
      return node;
    }
    virtual Node& stmtVariableDeclare(const String& name) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableDeclare);
      node.literal = this->createHardValueString(name);
      return node;
    }
    virtual Node& stmtVariableDefine(const String& name, Node& value) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableDeclare);
      node.literal = this->createHardValueString(name);
      node.addChild(this->stmtVariableSet(name, value));
      return node;
    }
    virtual Node& stmtVariableSet(const String& name, Node& value) override {
      auto& node = this->makeNode(Node::Kind::StmtVariableSet);
      node.literal = this->createHardValueString(name);
      node.addChild(value);
      return node;
    }
    virtual Node& stmtPropertySet(Node& instance, Node& property, Node& value) override {
      auto& node = this->makeNode(Node::Kind::StmtPropertySet);
      node.addChild(instance);
      node.addChild(property);
      node.addChild(value);
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

  class VMProgramRunner : public VMCollectable<IVMProgramRunner> {
    VMProgramRunner(const VMProgramRunner&) = delete;
    VMProgramRunner& operator=(const VMProgramRunner&) = delete;
  private:
    struct NodeStack {
      const IVMProgram::Node* node;
      size_t index;
      std::deque<HardValue> deque;
    };
    HardPtr<VMProgram> program;
    std::stack<NodeStack> stack;
    VMSymbolTable symtable;
    VMExecution execution;
  public:
    VMProgramRunner(IVM& vm, VMProgram& program)
      : VMCollectable<IVMProgramRunner>(vm),
        program(&program),
        execution(vm) {
      this->push(program.getRunnableRoot());
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // TODO any soft links?
    }
    virtual void print(Printer& printer) const override {
      printer << "[VMProgramRunner]";
    }
    virtual void addBuiltin(const String& name, const HardValue& value) override {
      auto added = this->symtable.add(VMSymbolTable::Kind::Builtin, name, value);
      assert(added == VMSymbolTable::Kind::Unknown);
      (void)added;
    }
    virtual RunOutcome run(HardValue& retval, RunFlags flags) override {
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
    RunOutcome step(HardValue& retval);
    bool stepBlock(HardValue& retval);
    void push(const IVMProgram::Node& node, size_t index = 0) {
      this->stack.emplace(&node, index);
    }
    void pop(const HardValue& value) {
      assert(!this->stack.empty());
      this->stack.pop();
      assert(!this->stack.empty());
      this->stack.top().deque.push_back(value);
    }
    HardValue createThrow(const HardValue& value) {
      return ValueFactory::createHardFlowControl(this->getAllocator(), ValueFlags::Throw, value);
    }
    template<typename... ARGS>
    RunOutcome createFault(HardValue& retval, ARGS&&... args) {
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
  protected:
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
    virtual IBasket& getBasket() const override {
      return *this->basket;
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
    virtual HardValue createHardValueVoid() override {
      return HardValue::Void;
    }
    virtual HardValue createHardValueNull() override {
      return HardValue::Null;
    }
    virtual HardValue createHardValueBool(Bool value) override {
      return value ? HardValue::True : HardValue::False;
    }
    virtual HardValue createHardValueInt(Int value) override {
      return ValueFactory::createInt(this->allocator, value);
    }
    virtual HardValue createHardValueFloat(Float value) override {
      return ValueFactory::createFloat(this->allocator, value);
    }
    virtual HardValue createHardValueString(const String& value) override {
      return ValueFactory::createString(this->allocator, value);
    }
    virtual HardValue createHardValueObject(const HardObject& value) override {
      return ValueFactory::createHardObject(this->allocator, value);
    }
    virtual HardObject createBuiltinAssert() override {
      return ObjectFactory::createBuiltinAssert(*this);
    }
    virtual HardObject createBuiltinPrint() override {
      return ObjectFactory::createBuiltinPrint(*this);
    }
    virtual HardObject createBuiltinExpando() override {
      return ObjectFactory::createBuiltinExpando(*this);
    }
    virtual HardObject createBuiltinCollector() override {
      return ObjectFactory::createBuiltinCollector(*this);
    }
    virtual void softAcquire(ICollectable*& target, const ICollectable* value) override {
      // TODO: thread safety
      assert(target == nullptr);
      if (value != nullptr) {
        target = this->basket->take(*value);
      } else {
        target = nullptr;
      }
    }
    virtual IValue* softHarden(const IValue* soft) override {
      // TODO: thread safety
      return const_cast<IValue*>(soft);
    }
    virtual IValue* softCreateValue() override {
      // TODO: thread safety
      auto* created = SoftValue::createPoly(this->allocator);
      assert(created != nullptr);
      auto* taken = this->basket->take(*created);
      assert(taken == created);
      return static_cast<IValue*>(taken);
    }
    virtual IValue* softCreateAlias(const IValue& value) override {
      // TODO: thread safety
      auto* taken = this->basket->take(value);
      assert(taken == &value);
      return static_cast<IValue*>(taken);
    }
    virtual bool softSetValue(IValue*& target, const IValue& value) override {
      // TODO: thread safety
      // Note we do not currently update the target pointer but may do later for optimization reasons
      assert(target != nullptr);
      return target->set(value);
    }
  };
}

egg::ovum::IVMProgramRunner::RunOutcome VMProgramRunner::step(HardValue& retval) {
  auto& top = this->stack.top();
  assert(top.index <= top.node->children.size());
  switch (top.node->kind) {
  case IVMProgram::Node::Kind::Root:
    assert(top.node->literal->getVoid());
    if (!this->stepBlock(retval)) {
      if (retval->getFlags() == ValueFlags::Void) {
        // Fell off the end of the list of statements
        return RunOutcome::Completed;
      }
      if (retval.hasAnyFlags(ValueFlags::Return)) {
        // Premature 'return' statement
        return RunOutcome::Completed;
      }
      // Probably an exception thrown
      return RunOutcome::Faulted;
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableDeclare:
    if (top.index == 0) {
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->createFault(retval, "Invalid program node literal for variable symbol");
      }
      switch (this->symtable.add(VMSymbolTable::Kind::Unset, symbol, HardValue::Void)) {
      case VMSymbolTable::Kind::Unknown:
        break;
      case VMSymbolTable::Kind::Builtin:
        return this->createFault(retval, "Variable symbol already declared as a builtin: '", symbol, "'");
      case VMSymbolTable::Kind::Unset:
      case VMSymbolTable::Kind::Variable:
        return this->createFault(retval, "Variable symbol already declared: '", symbol, "'");
      }
    }
    if (!this->stepBlock(retval)) {
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->createFault(retval, "Invalid program node literal for variable symbol");
      }
      switch (this->symtable.remove(symbol)) {
      case VMSymbolTable::Kind::Unknown:
        return this->createFault(retval, "Unknown variable symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Builtin:
        return this->createFault(retval, "Cannot undeclare builtin symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Unset:
      case VMSymbolTable::Kind::Variable:
        break;
      }
      this->pop(retval);
    }
    break;
  case IVMProgram::Node::Kind::StmtVariableSet:
    assert(top.node->children.size() == 1);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        retval = latest;
        return RunOutcome::Faulted;
      }
    }
    if (top.index == 0) {
      // Evaluate the value
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 1);
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->createFault(retval, "Invalid program node literal for variable symbol");
      }
      switch (this->symtable.set(symbol, top.deque.front())) {
      case VMSymbolTable::Kind::Unknown:
        return this->createFault(retval, "Unknown variable symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Builtin:
        return this->createFault(retval, "Cannot modify builtin symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Variable:
      case VMSymbolTable::Kind::Unset:
        break;
      }
      this->pop(HardValue::Void);
    }
    break;
  case IVMProgram::Node::Kind::StmtPropertySet:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 3);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        retval = latest;
        return RunOutcome::Faulted;
      }
    }
    if (top.index < 3) {
      // Evaluate the expressions
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 3);
      HardObject instance;
      if (!top.deque.front()->getHardObject(instance)) {
        return this->createFault(retval, "Invalid left hand side for '.' operator");
      }
      auto& property = top.deque[1];
      auto& value = top.deque.back();
      this->pop(instance->vmPropertySet(this->execution, property, value));
    }
    break;
  case IVMProgram::Node::Kind::StmtFunctionCall:
  case IVMProgram::Node::Kind::ExprFunctionCall:
    assert(top.node->literal->getVoid());
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        retval = latest;
        return RunOutcome::Faulted;
      }
    }
    if (top.index < top.node->children.size()) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the function call
      assert(top.deque.size() >= 1);
      HardObject function;
      if (!top.deque.front()->getHardObject(function)) {
        return this->createFault(retval, "Invalid initial program node value for function call");
      }
      top.deque.pop_front();
      CallArguments arguments;
      for (auto& argument : top.deque) {
        // TODO support named arguments
        arguments.addUnnamed(argument);
      }
      this->pop(function->vmCall(this->execution, arguments));
    }
    break;
  case IVMProgram::Node::Kind::ExprBinaryOp:
    assert(top.node->children.size() == 2);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        retval = latest;
        return RunOutcome::Faulted;
      }
    }
    if (top.index < 2) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      assert(top.deque.size() == 2);
      Int literal;
      if (!top.node->literal->getInt(literal)) {
        return this->createFault(retval, "Invalid program node literal for binary operation");
      }
      auto op = IVMExecution::BinaryOp(literal);
      auto result = this->execution.evaluateBinaryOp(op, top.deque.front(), top.deque.back());
      this->pop(result);
    }
    break;
  case IVMProgram::Node::Kind::ExprVariable:
    assert(top.node->children.empty());
    assert(top.deque.empty());
    {
      String symbol;
      if (!top.node->literal->getString(symbol)) {
        return this->createFault(retval, "Invalid program node literal for variable symbol");
      }
      HardValue value;
      switch (this->symtable.lookup(symbol, value)) {
      case VMSymbolTable::Kind::Unknown:
        return this->createFault(retval, "Unknown variable symbol: '", symbol, "'");
      case VMSymbolTable::Kind::Unset:
        return this->createFault(retval, "Variable uninitialized: '", symbol, "'");
      case VMSymbolTable::Kind::Builtin:
      case VMSymbolTable::Kind::Variable:
        break;
      }
      this->pop(value);
    }
    break;
  case IVMProgram::Node::Kind::ExprLiteral:
    assert(top.node->children.empty());
    assert(top.deque.empty());
    this->pop(top.node->literal);
    break;
  case IVMProgram::Node::Kind::ExprPropertyGet:
    assert(top.node->literal->getVoid());
    assert(top.node->children.size() == 2);
    if (!top.deque.empty()) {
      // Check the last evaluation
      auto& latest = top.deque.back();
      if (latest.hasFlowControl()) {
        retval = latest;
        return RunOutcome::Faulted;
      }
    }
    if (top.index < 2) {
      // Assemble the arguments
      this->push(*top.node->children[top.index++]);
    } else {
      // Perform the property fetch
      assert(top.deque.size() == 2);
      HardObject instance;
      if (!top.deque.front()->getHardObject(instance)) {
        return this->createFault(retval, "Invalid left hand side for '.' operator");
      }
      auto& property = top.deque.back();
      this->pop(instance->vmPropertyGet(this->execution, property));
    }
    break;
  default:
    return this->createFault(retval, "Invalid program node kind in program runner");
  }
  return RunOutcome::Stepped;
}

bool VMProgramRunner::stepBlock(HardValue& retval) {
  auto& top = this->stack.top();
  assert(top.index <= top.node->children.size());
  if (top.index > 0) {
    // Check the result of the previous child statement
    assert(top.deque.size() == 1);
    auto& result = top.deque.back();
    if (result.hasFlowControl()) {
      retval = result;
      return false;
    }
    if (result->getFlags() != ValueFlags::Void) {
      this->log(ILogger::Source::Runtime, ILogger::Severity::Warning, this->createString("Discarded value in statement"));
    }
    top.deque.pop_back();
  }
  assert(top.deque.empty());
  if (top.index < top.node->children.size()) {
    // Execute all the statements
    this->push(*top.node->children[top.index++]);
  } else {
    // Reached the end of the list of statements
    retval = HardValue::Void;
    return false;
  }
  return true;
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createDefault(IAllocator& allocator, ILogger& logger) {
  return allocator.makeHard<VMDefault>(logger);
}
