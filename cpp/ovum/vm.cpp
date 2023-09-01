#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  class VMProgramRunner;

  template<typename T, typename... ARGS>
  HardPtr<T> makeHardVMImpl(IVM& vm, ARGS&&... args) {
    // Use perfect forwarding
    return HardPtr<T>(vm.getAllocator().makeRaw<T>(vm, std::forward<ARGS>(args)...));
  }

  template<typename T>
  class VMImpl : public T {
    VMImpl(const VMImpl&) = delete;
    VMImpl& operator=(const VMImpl&) = delete;
  protected:
    IVM& vm;
    mutable Atomic<int64_t> atomic; // signed so we can detect underflows
    explicit VMImpl(IVM& vm)
      : vm(vm),
        atomic(0) {
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
    template<typename TYPE, typename... ARGS>
    HardPtr<TYPE> makeHardVM(ARGS&&... args) {
      // Use perfect forwarding
      return makeHardVMImpl<TYPE>(this->vm, std::forward<ARGS>(args)...);
    }
  };

  class VMProgram : public VMImpl<IVMProgram> {
    VMProgram(const VMProgram&) = delete;
    VMProgram& operator=(const VMProgram&) = delete;
  public:
    explicit VMProgram(IVM& vm)
      : VMImpl(vm) {
    }
    virtual HardPtr<IVMProgramRunner> createRunner() override {
      return this->makeHardVM<VMProgramRunner>(*this);
    }
  };

  class VMProgramNode : public VMImpl<IVMProgramNode> {
    VMProgramNode(const VMProgramNode&) = delete;
    VMProgramNode& operator=(const VMProgramNode&) = delete;
  public:
    enum class Kind {
      ExprVariable,
      ExprLiteralString,
      StmtFunctionCall,
    };
    Kind kind;
    Value value;
    std::vector<HardPtr<IVMProgramNode>> children;
    VMProgramNode(IVM& vm, Kind kind)
      : VMImpl(vm), kind(kind) {
    }
    virtual void addChild(const HardPtr<IVMProgramNode>& child) override {
      this->children.push_back(child);
    }
    virtual HardPtr<IVMProgramNode> build() override {
      // Do nothing WIBBLE
      return HardPtr<IVMProgramNode>(this);
    }
  };

  class VMProgramBuilder : public VMImpl<IVMProgramBuilder> {
    VMProgramBuilder(const VMProgramBuilder&) = delete;
    VMProgramBuilder& operator=(const VMProgramBuilder&) = delete;
  private:
    std::vector<HardPtr<IVMProgramNode>> statements;
  public:
    explicit VMProgramBuilder(IVM& vm)
      : VMImpl(vm) {
    }
    virtual void addStatement(const HardPtr<IVMProgramNode>& statement) override {
      this->statements.push_back(statement);
    }
    virtual HardPtr<IVMProgram> build() override {
      return this->makeHardVM<VMProgram>();
    }
    virtual HardPtr<IVMProgramNode> exprVariable(const String& name) override {
      auto node = this->makeNode(VMProgramNode::Kind::ExprVariable);
      node->value = this->vm.createValueString(name);
      return node;
    }
    virtual HardPtr<IVMProgramNode> exprLiteralString(const String& literal) override {
      auto node = this->makeNode(VMProgramNode::Kind::ExprLiteralString);
      node->value = this->vm.createValueString(literal);
      return node;
    }
    virtual HardPtr<IVMProgramNode> stmtFunctionCall(const HardPtr<IVMProgramNode>& function) override {
      auto node = this->makeNode(VMProgramNode::Kind::StmtFunctionCall);
      node->addChild(function);
      return node;
    }
  private:
    HardPtr<VMProgramNode> makeNode(VMProgramNode::Kind kind) {
      return this->makeHardVM<VMProgramNode>(kind);
    }
  };

  class VMProgramRunner : public VMImpl<IVMProgramRunner> {
    VMProgramRunner(const VMProgramRunner&) = delete;
    VMProgramRunner& operator=(const VMProgramRunner&) = delete;
  private:
    HardPtr<VMProgram> program;
  public:
    VMProgramRunner(IVM& vm, VMProgram& program)
      : VMImpl(vm),
        program(&program) {
    }
    virtual Value step() override {
      return Value::Rethrow;
    }
    virtual Value run() override {
      return Value::Rethrow;
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
      return makeHardVMImpl<VMProgramBuilder>(*this);
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
