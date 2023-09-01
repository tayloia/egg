#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  template<typename T, typename... ARGS>
  HardPtr<T> makeHardVMImpl(IVM& vm, ARGS&&... args) {
    // Use perfect forwarding
    return HardPtr<T>(vm.getAllocator().makeRaw<T>(vm, std::forward<ARGS>(args)...));
  }

  HardPtr<IValue> makeHardValue(const Value& value) {
    assert(value.validate());
    return HardPtr<IValue>(&value.get());
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
    explicit VMProgram(IVM& vm) : VMImpl(vm) {
    }
  };

  class VMProgramBuilder : public VMImpl<IVMProgramBuilder> {
    VMProgramBuilder(const VMProgramBuilder&) = delete;
    VMProgramBuilder& operator=(const VMProgramBuilder&) = delete;
  public:
    explicit VMProgramBuilder(IVM& vm) : VMImpl(vm) {
    }
    virtual HardPtr<IVMProgram> build() override {
      return this->makeHardVM<VMProgram>();
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
    virtual HardPtr<IValue> createValueVoid() override {
      return makeHardValue(Value::Void);
    }
    virtual HardPtr<IValue> createValueNull() override {
      return makeHardValue(Value::Null);
    }
    virtual HardPtr<IValue> createValueBool(Bool value) override {
      return makeHardValue(value ? Value::True : Value::False);
    }
    virtual HardPtr<IValue> createValueInt(Int value) override {
      return makeHardValue(ValueFactory::createInt(this->allocator, value));
    }
    virtual HardPtr<IValue> createValueFloat(Float value) override {
      return makeHardValue(ValueFactory::createFloat(this->allocator, value));
    }
    virtual HardPtr<IValue> createValueString(const String& value) override {
      return makeHardValue(ValueFactory::createString(this->allocator, value));
    }
  };
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createDefault(IAllocator& allocator) {
  return allocator.makeHard<VMDefault>();
}

egg::ovum::HardPtr<IVM> egg::ovum::VMFactory::createTest(IAllocator& allocator) {
  return allocator.makeHard<VMDefault>();
}
