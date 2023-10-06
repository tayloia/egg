#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  template<typename T, typename RETTYPE = HardObject, typename... ARGS>
  RETTYPE makeHardObject(IVM& vm, ARGS&&... args) {
    // Use perfect forwarding
    return RETTYPE(vm.getAllocator().makeRaw<T>(vm, std::forward<ARGS>(args)...));
  }

  class VMObjectBase : public SoftReferenceCounted<IObject> {
    VMObjectBase(const VMObjectBase&) = delete;
    VMObjectBase& operator=(const VMObjectBase&) = delete;
  protected:
    HardPtr<IVM> vm;
    template<typename... ARGS>
    inline HardValue raise(IVMExecution& execution, ARGS&&... args) {
      // TODO: Non-string exception?
      auto message = StringBuilder::concat(this->vm->getAllocator(), std::forward<ARGS>(args)...);
      return execution.raiseException(execution.createHardValueString(message));
    }
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
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual void print(Printer& printer) const override {
      printer << "[builtin assert]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 1) {
        return this->raise(execution, "Builtin 'assert()' expects exactly one argument");
      }
      HardValue value;
      Bool success = false;
      if (!arguments.getArgumentByIndex(0, value) || !value->getBool(success)) {
        return this->raise(execution, "Builtin 'assert()' expects a 'bool' argument");
      }
      if (!success) {
        return this->raise(execution, "Assertion failure");
      }
      return HardValue::Void;
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raise(execution, "Builtin 'assert()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raise(execution, "Builtin 'assert()' does not support properties");
    }
  };

  class VMObjectBuiltinPrint : public VMObjectBase {
    VMObjectBuiltinPrint(const VMObjectBuiltinPrint&) = delete;
    VMObjectBuiltinPrint& operator=(const VMObjectBuiltinPrint&) = delete;
  public:
    explicit VMObjectBuiltinPrint(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual void print(Printer& printer) const override {
      printer << "[builtin print]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      StringBuilder sb;
      size_t n = arguments.getArgumentCount();
      String name;
      HardValue value;
      for (size_t i = 0; i < n; ++i) {
        if (!arguments.getArgumentByIndex(i, value, &name) || !name.empty()) {
          return this->raise(execution, "Builtin 'print()' expects unnamed arguments");
        }
        sb.add(value);
      }
      execution.log(ILogger::Source::User, ILogger::Severity::None, sb.build(this->vm->getAllocator()));
      return HardValue::Void;
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raise(execution, "Builtin 'print()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raise(execution, "Builtin 'print()' does not support properties");
    }
  };

  class VMObjectExpando : public VMObjectBase {
    VMObjectExpando(const VMObjectExpando&) = delete;
    VMObjectExpando& operator=(const VMObjectExpando&) = delete;
  private:
    std::map<SoftKey, SoftValue> properties;
  public:
    explicit VMObjectExpando(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      for (const auto& property : this->properties) {
        property.first.visit(visitor);
        property.second.visit(visitor);
      }
    }
    virtual void print(Printer& printer) const override {
      printer << "[expando]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments&) override {
      return this->raise(execution, "TODO: Expando objects do not yet support function call semantics");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) override {
      SoftKey pname(*this->vm, property.get());
      auto pfound = this->properties.find(pname);
      if (pfound == this->properties.end()) {
        return this->raise(execution, "TODO: Cannot find property '", pname, "' in expando object");
      }
      return this->vm->getSoftValue(pfound->second);
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue& property, const HardValue& value) override {
      SoftKey pname(*this->vm, property.get());
      auto pfound = this->properties.find(pname);
      if (pfound == this->properties.end()) {
        pfound = this->properties.emplace_hint(pfound, std::piecewise_construct, std::forward_as_tuple(pname), std::forward_as_tuple(*this->vm));
      }
      if (!this->vm->setSoftValue(pfound->second, value)) {
        return this->raise(execution, "TODO: Cannot modify property '", pname, "'");
      }
      return HardValue::Void;
    }
  };

  class VMObjectBuiltinExpando : public VMObjectBase {
    VMObjectBuiltinExpando(const VMObjectBuiltinExpando&) = delete;
    VMObjectBuiltinExpando& operator=(const VMObjectBuiltinExpando&) = delete;
  public:
    explicit VMObjectBuiltinExpando(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual void print(Printer& printer) const override {
      printer << "[builtin expando]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raise(execution, "Builtin 'expando()' expects no arguments");
      }
      auto instance = makeHardObject<VMObjectExpando>(*this->vm);
      return execution.createHardValueObject(instance);
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raise(execution, "Builtin 'expando()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raise(execution, "Builtin 'expando()' does not support properties");
    }
  };

  class VMObjectBuiltinCollector : public VMObjectBase {
    VMObjectBuiltinCollector(const VMObjectBuiltinCollector&) = delete;
    VMObjectBuiltinCollector& operator=(const VMObjectBuiltinCollector&) = delete;
  public:
    explicit VMObjectBuiltinCollector(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual void print(Printer& printer) const override {
      printer << "[builtin collector]";
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raise(execution, "Builtin 'collector()' expects no arguments");
      }
      auto collected = this->vm->getBasket().collect();
      return execution.createHardValueInt(Int(collected));
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raise(execution, "Builtin 'collector()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raise(execution, "Builtin 'collector()' does not support properties");
    }
  };
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createBuiltinAssert(IVM& vm) {
  return makeHardObject<VMObjectBuiltinAssert>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createBuiltinPrint(IVM& vm) {
  return makeHardObject<VMObjectBuiltinPrint>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createBuiltinExpando(IVM& vm) {
  return makeHardObject<VMObjectBuiltinExpando>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createBuiltinCollector(IVM& vm) {
  return makeHardObject<VMObjectBuiltinCollector>(vm);
}
