#include "ovum/ovum.h"

#include <stack>

namespace {
  using namespace egg::ovum;

  class VMProgramRunner;

  template<typename T>
  class VMImpl : public T {
    VMImpl(const VMImpl&) = delete;
    VMImpl& operator=(const VMImpl&) = delete;
  protected:
    IVM& vm;
    mutable Atomic<int64_t> atomic; // signed so we can detect underflows
    explicit VMImpl(IVM& vm)
      : vm(vm), atomic(0) {
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
    virtual IBasket& getBasket() const override {
      return this->vm.getBasket();
    }
    template<typename... ARGS>
    String createStringConcat(ARGS&&... args) {
      StringBuilder sb(&this->getAllocator());
      return sb.add(std::forward<ARGS>(args)...).build();
    }
    template<typename... ARGS>
    Value createValue(ARGS&&... args) {
      return ValueFactory::create(this->getAllocator(), std::forward<ARGS>(args)...);
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
    ExprLiteralString,
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
    // Do nothing WIBBLE
    return *this;
  }
};

namespace {
  using namespace egg::ovum;

  class VMProgramBuilder : public VMImpl<IVMProgramBuilder> {
    VMProgramBuilder(const VMProgramBuilder&) = delete;
    VMProgramBuilder& operator=(const VMProgramBuilder&) = delete;
  private:
    HardPtr<IVMProgram::Node> root;
  public:
    explicit VMProgramBuilder(IVM& vm)
      : VMImpl(vm) {
      this->root = VMProgram::makeHardVM<IVMProgram::Node>(this->vm, nullptr, IVMProgram::Node::Kind::Root);
    }
    virtual void addStatement(IVMProgram::Node& statement) override {
      assert(this->root != nullptr);
      this->root->addChild(statement.build());
    }
    virtual HardPtr<IVMProgram> build() override {
      assert(this->root != nullptr);
      auto program = VMProgram::makeHardVM<VMProgram>(this->vm, *this->root);
      this->root = nullptr;
      return program;
    }
    virtual IVMProgram::Node& exprVariable(const String& name) override {
      auto& node = this->makeNode(IVMProgram::Node::Kind::ExprVariable);
      node.value = this->vm.createValueString(name);
      return node;
    }
    virtual IVMProgram::Node& exprLiteralString(const String& literal) override {
      auto& node = this->makeNode(IVMProgram::Node::Kind::ExprLiteralString);
      node.value = this->vm.createValueString(literal);
      return node;
    }
    virtual IVMProgram::Node& stmtFunctionCall(IVMProgram::Node& function) override {
      auto& node = this->makeNode(IVMProgram::Node::Kind::StmtFunctionCall);
      node.addChild(function);
      return node;
    }
  private:
    virtual void appendChild(IVMProgram::Node& parent, IVMProgram::Node& child) override {
      parent.addChild(child);
    }
    IVMProgram::Node& makeNode(IVMProgram::Node::Kind kind) {
      assert(this->root != nullptr);
      auto node = VMProgram::makeHardVM<IVMProgram::Node>(this->vm, this->root.get(), kind);
      return *node;
    }
  };

  class VMProgramRunner : public VMImpl<IVMProgramRunner> {
    VMProgramRunner(const VMProgramRunner&) = delete;
    VMProgramRunner& operator=(const VMProgramRunner&) = delete;
  private:
    struct Stack {
      const IVMProgram::Node* node;
      size_t index;
    };
    HardPtr<VMProgram> program;
    std::stack<Stack> stack;
  public:
    VMProgramRunner(IVM& vm, VMProgram& program)
      : VMImpl(vm), program(&program) {
      this->push(program.getRunnableRoot());
    }
    virtual RunOutcome run(Value& retval, RunFlags flags) override {
      if (flags != RunFlags::None) {
        return this->createFault(retval, "TODO: Run flags not yet supported in program runner");
      }
      for (;;) {
        auto& top = this->stack.top();
        assert(top.index <= top.node->children.size());
        switch (top.node->kind) {
        case IVMProgram::Node::Kind::Root:
          if (top.index < top.node->children.size()) {
            this->push(*top.node->children[top.index++]);
          } else {
            // Reached the end of the list of statements in the program
            retval = Value::Void;
            return RunOutcome::Completed;
          }
          break;
        case IVMProgram::Node::Kind::StmtFunctionCall:
          if (top.index < top.node->children.size()) {
            this->push(*top.node->children[top.index++]);
          } else {
            this->pop(Value::Void); // WIBBLE
          }
          break;
        case IVMProgram::Node::Kind::ExprVariable:
          assert(top.node->children.empty());
          this->pop(top.node->value); // WIBBLE
          break;
        case IVMProgram::Node::Kind::ExprLiteralString:
          assert(top.node->children.empty());
          assert(top.node->value->getFlags() == ValueFlags::String);
          this->pop(top.node->value);
          break;
        default:
          return this->createFault(retval, "Invalid program node kind in program runner");
        }
      }
    }
  private:
    void push(const IVMProgram::Node& node, size_t index = 0) {
      this->stack.emplace(&node, index);
    }
    void pop(const Value&) {
      this->stack.pop();
    }
    Value createThrow(const Value& value) {
      return ValueFactory::createFlowControl(this->getAllocator(), ValueFlags::Throw, value);
    }
    template<typename... ARGS>
    RunOutcome createFault(Value& retval, ARGS&&... args) {
      auto reason = this->createStringConcat(std::forward<ARGS>(args)...);
      retval = this->createThrow(this->createValue(reason));
      return RunOutcome::Fault;
    }
  };

  class VMDefault : public HardReferenceCounted<IVM> {
    VMDefault(const VMDefault&) = delete;
    VMDefault& operator=(const VMDefault&) = delete;
  private:
    HardPtr<IBasket> basket;
  public:
    explicit VMDefault(IAllocator& allocator)
      : HardReferenceCounted(allocator, 0),
        basket(BasketFactory::createBasket(allocator)) {
    }
    virtual IAllocator& getAllocator() const override {
      return this->allocator;
    }
    virtual IBasket& getBasket() const override {
      return *this->basket;
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
  };
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createDefault(IAllocator& allocator) {
  return allocator.makeHard<VMDefault>();
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createTest(IAllocator& allocator) {
  return allocator.makeHard<VMDefault>();
}
