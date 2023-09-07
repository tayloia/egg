#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  template<typename T, typename RETTYPE = HardObject, typename... ARGS>
  RETTYPE makeObject(IVM& vm, ARGS&&... args) {
    // Use perfect forwarding
    return RETTYPE(vm.getAllocator().makeRaw<T>(vm, std::forward<ARGS>(args)...));
  }

  class VMObjectBase : public SoftReferenceCounted<IObject> {
    VMObjectBase(const VMObjectBase&) = delete;
    VMObjectBase& operator=(const VMObjectBase&) = delete;
  protected:
    HardPtr<IVM> vm;
  public:
    explicit VMObjectBase(IVM& vm)
      : SoftReferenceCounted<IObject>(),
        vm(&vm) {
      // Initially not adopted by the any basket
    }
    virtual void hardDestroy() const override {
      this->vm->getAllocator().destroy(this);
    }
  };

  class VMObjectBuiltinAssert : public VMObjectBase {
    VMObjectBuiltinAssert(const VMObjectBuiltinAssert&) = delete;
    VMObjectBuiltinAssert& operator=(const VMObjectBuiltinAssert&) = delete;
  public:
    explicit VMObjectBuiltinAssert(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(const Visitor&) const override {
      // WIBBLE
    }
    virtual void print(Printer& printer) const override {
      printer << "[builtin assert]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 1) {
        return execution.raise("Builtin 'assert()' expects exactly one argument");
      }
      HardValue value;
      Bool success = false;
      if (!arguments.getArgumentByIndex(0, value) || !value->getBool(success)) {
        return execution.raise("Builtin 'assert()' expects a 'bool' argument");
      }
      if (!success) {
        return execution.raise("Assertion failure");
      }
      return HardValue::Void;
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return execution.raise("Builtin 'assert()' does not support properties");
    }
  };

  class VMObjectBuiltinPrint : public VMObjectBase {
    VMObjectBuiltinPrint(const VMObjectBuiltinPrint&) = delete;
    VMObjectBuiltinPrint& operator=(const VMObjectBuiltinPrint&) = delete;
  public:
    explicit VMObjectBuiltinPrint(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(const Visitor&) const override {
      // WIBBLE
    }
    virtual void print(Printer& printer) const override {
      printer << "[builtin print]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      StringBuilder sb(&this->vm->getAllocator());
      size_t n = arguments.getArgumentCount();
      String name;
      HardValue value;
      for (size_t i = 0; i < n; ++i) {
        if (!arguments.getArgumentByIndex(i, value, &name) || !name.empty()) {
          return execution.raise("Builtin 'print()' expects unnamed arguments");
        }
        sb.add(value);
      }
      execution.log(ILogger::Source::User, ILogger::Severity::None, sb.build());
      return HardValue::Void;
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return execution.raise("Builtin 'print()' does not support properties");
    }
  };

  class VMObjectExpando : public VMObjectBase {
    VMObjectExpando(const VMObjectExpando&) = delete;
    VMObjectExpando& operator=(const VMObjectExpando&) = delete;
  private:
    SoftPtr<IObject> x; // WIBBLE
  public:
    explicit VMObjectExpando(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(const Visitor&) const override {
      // WIBBLE
    }
    virtual void print(Printer& printer) const override {
      printer << "[expando]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments&) override {
      return execution.raise("TODO: Expando objects do not yet support function call semantics");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue& property, const HardValue&) override {
      String pname;
      if (!property->getString(pname) || !pname.equals("x")) {
        return execution.raise("TODO: Expando objects only support property 'x'");
      }
      return execution.raise("WIBBLE");
    }
  };

  class VMObjectBuiltinExpando : public VMObjectBase {
    VMObjectBuiltinExpando(const VMObjectBuiltinExpando&) = delete;
    VMObjectBuiltinExpando& operator=(const VMObjectBuiltinExpando&) = delete;
  public:
    explicit VMObjectBuiltinExpando(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(const Visitor&) const override {
      // WIBBLE
    }
    virtual void print(Printer& printer) const override {
      printer << "[builtin expando]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return execution.raise("Builtin 'expando()' expects no arguments");
      }
      auto instance = makeObject<VMObjectExpando>(*this->vm);
      return execution.createValueObjectHard(instance);
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return execution.raise("Builtin 'expando()' does not support properties");
    }
  };
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createBuiltinAssert(IVM& vm) {
  return makeObject<VMObjectBuiltinAssert>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createBuiltinPrint(IVM& vm) {
  return makeObject<VMObjectBuiltinPrint>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createBuiltinExpando(IVM& vm) {
  return makeObject<VMObjectBuiltinExpando>(vm);
}
