#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

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
    virtual Value call(IVMExecution& execution, const ICallArguments& arguments) override {
      StringBuilder sb(&this->vm->getAllocator());
      size_t n = arguments.getArgumentCount();
      String name;
      Value value;
      for (size_t i = 0; i < n; ++i) {
        auto fetched = arguments.getArgument(i, name, value);
        assert(fetched == true);
        (void)fetched;
        assert(name.empty());
        sb.add(value);
      }
      execution.log(ILogger::Source::User, ILogger::Severity::None, sb.build());
      return Value::Void;
    }
  };

  template<typename T, typename RETTYPE = Object, typename... ARGS>
  inline RETTYPE makeObject(IVM& vm, ARGS&&... args) {
    // Use perfect forwarding
    return RETTYPE(vm.getAllocator().makeRaw<T>(vm, std::forward<ARGS>(args)...));
  }
}

egg::ovum::Object egg::ovum::ObjectFactory::createBuiltinPrint(IVM& vm) {
  return makeObject<VMObjectBuiltinPrint>(vm);
}
