#include "ovum/ovum.h"
#include "ovum/operation.h"

#include <deque>

namespace {
  using namespace egg::ovum;

  template<typename T>
  std::string describe(const T& value) {
    std::stringstream ss;
    Printer printer(ss, Print::Options::DEFAULT);
    printer.describe(value);
    return ss.str();
  }

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
    HardValue raiseRuntimeError(IVMExecution& execution, ARGS&&... args) {
      // TODO: Non-string exception?
      auto message = StringBuilder::concat(this->vm->getAllocator(), std::forward<ARGS>(args)...);
      return execution.raiseRuntimeError(message);
    }
  public:
    explicit VMObjectBase(IVM& vm)
      : SoftReferenceCounted<IObject>(),
        vm(&vm) {
      // Initially not adopted by any basket
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
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 1) {
        return this->raiseRuntimeError(execution, "Builtin 'assert()' expects exactly one argument");
      }
      HardValue value;
      Bool success = false;
      if (!arguments.getArgumentByIndex(0, value) || !value->getBool(success)) {
        return this->raiseRuntimeError(execution, "Builtin 'assert()' expects a 'bool' argument, but instead got ", describe(value.get()));
      }
      if (!success) {
        return this->raiseRuntimeError(execution, "Assertion failure");
      }
      return HardValue::Void;
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support properties");
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
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      StringBuilder sb;
      size_t n = arguments.getArgumentCount();
      String name;
      HardValue value;
      for (size_t i = 0; i < n; ++i) {
        if (!arguments.getArgumentByIndex(i, value, &name) || !name.empty()) {
          return this->raiseRuntimeError(execution, "Builtin 'print()' expects unnamed arguments");
        }
        sb.add(value);
      }
      execution.log(ILogger::Source::User, ILogger::Severity::None, sb.build(this->vm->getAllocator()));
      return HardValue::Void;
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support properties");
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
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments&) override {
      return this->raiseRuntimeError(execution, "TODO: Expando objects do not yet support function call semantics");
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseRuntimeError(execution, "TODO: Expando objects do not yet support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue& index) override {
      SoftKey key(*this->vm, index.get());
      auto pfound = this->properties.find(key);
      if (pfound == this->properties.end()) {
        return this->raiseRuntimeError(execution, "TODO: Cannot find index '", key.get(), "' in expando object");
      }
      return execution.getSoftValue(pfound->second);
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue& index, const HardValue& value) override {
      SoftKey key(*this->vm, index.get());
      auto pfound = this->properties.find(key);
      if (pfound == this->properties.end()) {
        pfound = this->properties.emplace_hint(pfound, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(*this->vm));
      }
      if (!execution.setSoftValue(pfound->second, value)) {
        return this->raiseRuntimeError(execution, "TODO: Cannot modify index '", key.get(), "'");
      }
      return HardValue::Void;
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "TODO: Expando objects do not yet support index mutation");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) override {
      SoftKey pname(*this->vm, property.get());
      auto pfound = this->properties.find(pname);
      if (pfound == this->properties.end()) {
        return this->raiseRuntimeError(execution, "TODO: Cannot find property '", pname.get(), "' in expando object");
      }
      return execution.getSoftValue(pfound->second);
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue& property, const HardValue& value) override {
      SoftKey pname(*this->vm, property.get());
      auto pfound = this->properties.find(pname);
      if (pfound == this->properties.end()) {
        pfound = this->properties.emplace_hint(pfound, std::piecewise_construct, std::forward_as_tuple(pname), std::forward_as_tuple(*this->vm));
      }
      if (!execution.setSoftValue(pfound->second, value)) {
        return this->raiseRuntimeError(execution, "TODO: Cannot modify property '", pname.get(), "'");
      }
      return HardValue::Void;
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "TODO: Expando objects do not yet support property mutation");
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
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raiseRuntimeError(execution, "Builtin 'expando()' expects no arguments");
      }
      auto instance = makeHardObject<VMObjectExpando>(*this->vm);
      return execution.createHardValueObject(instance);
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support properties");
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
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raiseRuntimeError(execution, "Builtin 'collector()' expects no arguments");
      }
      auto collected = this->vm->getBasket().collect();
      return execution.createHardValueInt(Int(collected));
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support properties");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support properties");
    }
  };

  template<typename T>
  class VMObjectMemberHandler : public VMObjectBase {
    VMObjectMemberHandler(const VMObjectMemberHandler&) = delete;
    VMObjectMemberHandler& operator=(const VMObjectMemberHandler&) = delete;
  public:
    typedef HardValue(T::*Handler)(IVMExecution& execution, const ICallArguments& arguments);
  private:
    T* instance;
    Handler handler;
  public:
    VMObjectMemberHandler(IVM& vm, T& instance, Handler handler)
      : VMObjectBase(vm),
        instance(&instance),
        handler(handler) {
      this->vm->getBasket().take(*this->instance);
      assert(this->instance != nullptr);
      assert(this->handler != nullptr);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      assert(this->instance != nullptr);
      visitor.visit(*this->instance);
    }
    virtual void print(Printer& printer) const override {
      printer << "[member handler]";
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      assert(this->instance != nullptr);
      assert(this->handler != nullptr);
      return (this->instance->*(this->handler))(execution, arguments);
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseRuntimeError(execution, "Member handlers do not support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Member handlers do not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Member handlers do not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Member handlers do not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Member handlers do not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Member handlers do not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Member handlers do not support properties");
    }
  };

  class VMObjectVanillaMutex {
    VMObjectVanillaMutex(const VMObjectVanillaMutex&) = delete;
    VMObjectVanillaMutex& operator=(const VMObjectVanillaMutex&) = delete;
  public:
    class ReadLock final {
      ReadLock(const ReadLock&) = delete;
      ReadLock& operator=(const ReadLock&) = delete;
    private:
      egg::ovum::ReadLock lock;
    public:
      const uint64_t modifications;
      explicit ReadLock(VMObjectVanillaMutex& mutex);
    };
    class WriteLock final {
      WriteLock(const WriteLock&) = delete;
      WriteLock& operator=(const WriteLock&) = delete;
    private:
      VMObjectVanillaMutex& mutex;
      egg::ovum::WriteLock lock;
    public:
      bool modified;
      explicit WriteLock(VMObjectVanillaMutex& mutex);
      ~WriteLock();
    };
    friend class ReadLock;
    friend class WriteLock;
  private:
    ReadWriteMutex mutex;
    uint64_t modifications;
  public:
    VMObjectVanillaMutex()
      : modifications(0) {
    }
  };

  class VMObjectVanillaContainer : public VMObjectBase {
    VMObjectVanillaContainer(const VMObjectVanillaContainer&) = delete;
    VMObjectVanillaContainer& operator=(const VMObjectVanillaContainer&) = delete;
  protected:
    mutable VMObjectVanillaMutex mutex;
    explicit VMObjectVanillaContainer(IVM& vm)
      : VMObjectBase(vm) {
    }
  public:
    virtual const char* getName() const = 0;
    template<typename... ARGS>
    HardValue raiseContainerError(IVMExecution& execution, ARGS&&... args) {
      return this->raiseRuntimeError(execution, this->getName(), " ", std::forward<ARGS>(args)...);
    }
  };

  template<typename CONTAINER, typename STATE>
  class VMObjectVanillaIterator : public VMObjectBase {
    VMObjectVanillaIterator(const VMObjectVanillaIterator&) = delete;
    VMObjectVanillaIterator& operator=(const VMObjectVanillaIterator&) = delete;
  protected:
    using Container = CONTAINER;
    using State = STATE;
    Container& container;
    SoftObject soft; // maintains the reference to the container
    State state;
    VMObjectVanillaIterator(IVM& vm, Container& container, VMObjectVanillaMutex::ReadLock& lock)
      : VMObjectBase(vm),
        container(container),
        soft(vm, HardObject(&container)),
        state() {
      assert(this->soft != nullptr);
      state.modifications = lock.modifications;
    }
  public:
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      this->soft.visit(visitor);
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raiseIteratorError(execution, "does not support function call arguments");
      }
      return this->container.iteratorNext(execution, this->state);
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseIteratorError(execution, "does not yet support re-iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseIteratorError(execution, "does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseIteratorError(execution, "does not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseIteratorError(execution, "does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseIteratorError(execution, "does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseIteratorError(execution, "does not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseIteratorError(execution, "does not support properties");
    }
    template<typename... ARGS>
    HardValue raiseIteratorError(IVMExecution& execution, ARGS&&... args) {
      return this->container.raiseContainerError(execution, "iterator ", std::forward<ARGS>(args)...);
    }
  };

  class VMObjectVanillaArray : public VMObjectVanillaContainer {
    VMObjectVanillaArray(const VMObjectVanillaArray&) = delete;
    VMObjectVanillaArray& operator=(const VMObjectVanillaArray&) = delete;
  public:
    struct IteratorState {
      size_t index;
      uint64_t modifications;
    };
  private:
    std::deque<SoftValue> elements;
  public:
    explicit VMObjectVanillaArray(IVM& vm)
      : VMObjectVanillaContainer(vm) {
    }
    virtual const char* getName() const override {
      return "Vanilla array";
    }
    HardValue iteratorNext(IVMExecution& execution, IteratorState& state) {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      if (lock.modifications != state.modifications) {
        return this->raiseContainerError(execution, "has been modified during iteration");
      }
      if (state.index >= this->elements.size()) {
        return HardValue::Void;
      }
      return execution.getSoftValue(this->elements[state.index++]);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      for (const auto& element : this->elements) {
        element.visit(visitor);
      }
    }
    virtual void print(Printer& printer) const override {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      Print::Options options = printer.options;
      options.quote = '"';
      char separator = '[';
      for (const auto& element : this->elements) {
        printer.stream << separator;
        Print::write(printer.stream, element.get(), options);
        separator = ',';
      }
      if (separator == '[') {
        printer.stream << '[';
      }
      printer.stream << ']';
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments&) override {
      return this->raiseContainerError(execution, "does not support call semantics");
    }
    virtual HardValue vmIterate(IVMExecution& execution) override;
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue& index) override {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      Int ivalue;
      if (!index->getInt(ivalue)) {
        return this->raiseRuntimeError(execution, "Expected array index value to be an 'int', but instead got ", describe(index.get()));
      }
      auto uvalue = size_t(ivalue);
      if (uvalue >= this->elements.size()) {
        return this->raiseRuntimeError(execution, "Array index ", ivalue, " is out of range for an array of length ", this->elements.size());
      }
      return execution.getSoftValue(this->elements[uvalue]);
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue& index, const HardValue& value) override {
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      Int ivalue;
      if (!index->getInt(ivalue)) {
        return this->raiseRuntimeError(execution, "Expected array index value to be an 'int', but instead got ", describe(index.get()));
      }
      auto uvalue = size_t(ivalue);
      if (uvalue > this->elements.size()) {
        return this->raiseRuntimeError(execution, "Array index ", ivalue, " is out of range for an array of length ", this->elements.size());
      }
      if (!execution.setSoftValue(this->elements[uvalue], value)) {
        return this->raiseRuntimeError(execution, "Unable to set value at array index ", ivalue);
      }
      return HardValue::Void;
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue& index, ValueMutationOp mutation, const HardValue& value) override {
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      Int ivalue;
      if (!index->getInt(ivalue)) {
        return this->raiseRuntimeError(execution, "Expected array index value to be an 'int', but instead got ", describe(index.get()));
      }
      auto uvalue = size_t(ivalue);
      if (uvalue > this->elements.size()) {
        return this->raiseRuntimeError(execution, "Array index ", ivalue, " is out of range for an array of length ", this->elements.size());
      }
      return execution.mutateSoftValue(this->elements[uvalue], mutation, value);
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) override {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      String pname;
      if (property->getString(pname)) {
        if (pname.equals("length")) {
          return execution.createHardValueInt(Int(this->elements.size()));
        }
        if (pname.equals("push")) {
          return this->createSoftMemberHandler(&VMObjectVanillaArray::vmCallPush);
        }
        return this->raiseRuntimeError(execution, "Unknown array property name: '", pname, "'");
      }
      return this->raiseRuntimeError(execution, "Expected array property name to be a 'string', but instead got ", describe(property.get()));
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue& property, const HardValue& value) override {
      return this->vmPropertyMut(execution, property, ValueMutationOp::Assign, value);
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue& property, ValueMutationOp mutation, const HardValue& value) override {
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      String pname;
      if (property->getString(pname)) {
        if (pname.equals("length")) {
          return this->lengthMut(execution, lock, mutation, value);
        }
        return this->raiseRuntimeError(execution, "Only the 'length' property of an array may be modified, not '", pname, "'");
      }
      return this->raiseRuntimeError(execution, "Expected array property name to be a 'string', but instead got ", describe(property.get()));
    }
  private:
    typedef HardValue(VMObjectVanillaArray::*MemberHandler)(IVMExecution& execution, const ICallArguments& arguments);
    HardValue createSoftMemberHandler(MemberHandler handler) {
      auto instance = makeHardObject<VMObjectMemberHandler<VMObjectVanillaArray>>(*this->vm, *this, handler);
      return this->vm->createHardValueObject(instance);
    }
    HardValue lengthMut(IVMExecution& execution, VMObjectVanillaMutex::WriteLock& lock, ValueMutationOp mutation, const HardValue& rhs) {
      auto value = ValueFactory::createInt(this->vm->getAllocator(), Int(this->elements.size()));
      auto before = value->mutate(mutation, rhs.get());
      if (before.hasFlowControl()) {
        return before;
      }
      return this->lengthSet(execution, lock, value, HardValue::Void);
    }
    HardValue lengthSet(IVMExecution& execution, VMObjectVanillaMutex::WriteLock& lock, const HardValue& length, const HardValue& success) {
      Int ivalue;
      if (!length->getInt(ivalue)) {
        return this->raiseRuntimeError(execution, "Array 'length' property must be an 'int', but instead got ", describe(length.get()));
      }
      if (ivalue < 0) {
        return this->raiseRuntimeError(execution, "Array 'length' property must be an non-negative integer, but instead got ", ivalue);
      }
      if (ivalue > 0xFFFFFFFF) {
        return this->raiseRuntimeError(execution, "Array 'length' property too large: ", ivalue);
      }
      lock.modified = true;
      // OPTIMIZE
      auto diff = ivalue - Int(this->elements.size());
      while (diff > 0) {
        this->elements.emplace_back(*this->vm)->set(HardValue::Null.get());
        diff--;
      }
      while (diff < 0) {
        this->elements.pop_back();
        diff++;
      }
      return success;
    }
    HardValue vmCallPush(IVMExecution& execution, const ICallArguments& arguments) {
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      HardValue argument;
      for (size_t index = 0; arguments.getArgumentByIndex(index, argument); ++index) {
        auto& added = this->elements.emplace_back(*this->vm);
        if (!added->set(argument.get())) {
          return this->raiseRuntimeError(execution, "Cannot push value to the end of the array");
        }
        lock.modified = true;
      }
      return HardValue::Void;
    }
  };

  class VMObjectVanillaArrayIterator : public VMObjectVanillaIterator<VMObjectVanillaArray, VMObjectVanillaArray::IteratorState> {
    VMObjectVanillaArrayIterator(const VMObjectVanillaArrayIterator&) = delete;
    VMObjectVanillaArrayIterator& operator=(const VMObjectVanillaArrayIterator&) = delete;
  public:
    VMObjectVanillaArrayIterator(IVM& vm, Container& array, VMObjectVanillaMutex::ReadLock& lock)
      : VMObjectVanillaIterator(vm, array, lock) {
    }
    virtual void print(Printer& printer) const override {
      printer.stream << "[vanilla array iterator]";
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
  };

  class VMObjectVanillaObject : public VMObjectVanillaContainer {
    VMObjectVanillaObject(const VMObjectVanillaObject&) = delete;
    VMObjectVanillaObject& operator=(const VMObjectVanillaObject&) = delete;
  public:
    struct IteratorState {
      size_t index;
      uint64_t modifications;
    };
  private:
    std::map<SoftKey, SoftValue> properties;
    std::vector<SoftKey> keys;
  public:
    explicit VMObjectVanillaObject(IVM& vm)
      : VMObjectVanillaContainer(vm) {
    }
    virtual const char* getName() const override {
      return "Vanilla object";
    }
    HardValue iteratorNext(IVMExecution& execution, IteratorState& state) {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      if (lock.modifications != state.modifications) {
        return this->raiseContainerError(execution, "has been modified during iteration");
      }
      if (state.index >= this->keys.size()) {
        return HardValue::Void;
      }
      const auto& softkey = this->keys[state.index++];
      auto key = this->vm->getSoftKey(softkey);
      auto found = this->properties.find(softkey);
      if (found == this->properties.end()) {
        return this->raiseContainerError(execution, "is missing property '", key.get(), "' during iteration");
      }
      const auto& softvalue = found->second;
      auto value = this->vm->getSoftValue(softvalue);
      auto object = ObjectFactory::createVanillaKeyValue(*this->vm, key, value);
      return execution.createHardValueObject(object);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      // We only need to iterate around the property member elements as the key vectors are simply duplicates
      for (const auto& property : this->properties) {
        property.first.visit(visitor);
        property.second.visit(visitor);
      }
    }
    virtual void print(Printer& printer) const override {
      Print::Options options = printer.options;
      options.quote = '"';
      char separator = '{';
      for (const auto& key : this->keys) {
        printer.stream << separator;
        key->print(printer);
        printer.stream << ':';
        Print::write(printer.stream, this->properties.find(key)->second.get(), options);
        separator = ',';
      }
      if (separator == '{') {
        printer.stream << '{';
      }
      printer.stream << '}';
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments&) override {
      // TODO
      return this->raiseContainerError(execution, "does not support call semantics");
    }
    virtual HardValue vmIterate(IVMExecution& execution) override;
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue& index) override {
      return this->propertyGet(execution, index);
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue& index, const HardValue& value) override {
      return this->propertySet(execution, index, value);
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue& index, ValueMutationOp mutation, const HardValue& value) override {
      return this->propertyMut(execution, index, mutation, value);
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) override {
      return this->propertyGet(execution, property);
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue& property, const HardValue& value) override {
      return this->propertySet(execution, property, value);
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue& property, ValueMutationOp mutation, const HardValue& value) override {
      return this->propertyMut(execution, property, mutation, value);
    }
  private:
    HardValue propertyGet(IVMExecution& execution, const HardValue& property) {
      SoftKey key(*this->vm, property.get());
      auto pfound = this->properties.find(key);
      if (pfound == this->properties.end()) {
        return this->raiseContainerError(execution, "does not contain property '", key.get(), "'");
      }
      return execution.getSoftValue(pfound->second);
    }
    HardValue propertySet(IVMExecution& execution, const HardValue& property, const HardValue& value) {
      SoftKey key(*this->vm, property.get());
      auto pfound = this->properties.find(key);
      if (pfound == this->properties.end()) {
        this->propertyCreate(pfound, key);
      }
      if (!execution.setSoftValue(pfound->second, value)) {
        return this->raiseContainerError(execution, "cannot modify property '", key.get(), "'");
      }
      return HardValue::Void;
    }
    HardValue propertyMut(IVMExecution& execution, const HardValue& property, ValueMutationOp mutation, const HardValue& value) {
      SoftKey key(*this->vm, property.get());
      auto pfound = this->properties.find(key);
      if (pfound == this->properties.end()) {
        if (mutation != ValueMutationOp::Assign) {
          return this->raiseContainerError(execution, "does not contain property '", key.get(), "'");
        }
        this->propertyCreate(pfound, key);
      }
      return execution.mutateSoftValue(pfound->second, mutation, value);
    }
    void propertyCreate(std::map<SoftKey, SoftValue>::iterator& where, SoftKey& key) {
      where = this->properties.emplace_hint(where, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(*this->vm));
      assert(where != this->properties.end());
      this->keys.emplace_back(key);
    }
  protected:
    HardValue propertyFind(IVMExecution& execution, const HardValue& property) {
      SoftKey key(*this->vm, property.get());
      auto pfound = this->properties.find(key);
      if (pfound == this->properties.end()) {
        return HardValue::Void;
      }
      return execution.getSoftValue(pfound->second);
    }
    void propertyAdd(const HardValue& property, const HardValue& value) {
      SoftKey key(*this->vm, property.get());
      auto where = this->properties.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(*this->vm));
      assert(where.second);
      auto success = this->vm->setSoftValue(where.first->second, value);
      assert(success);
      if (success) {
        this->keys.emplace_back(key);
      } else {
        this->properties.erase(where.first);
      }
    }
    template<typename K>
    void propertyAdd(K property, const HardValue& value) {
      return this->propertyAdd(this->vm->createHardValue(property), value);
    }
    template<typename K, typename V>
    void propertyAdd(K property, V value) {
      return this->propertyAdd(this->vm->createHardValue(property), this->vm->createHardValue(value));
    }
  };

  class VMObjectVanillaObjectIterator : public VMObjectVanillaIterator<VMObjectVanillaObject, VMObjectVanillaObject::IteratorState> {
    VMObjectVanillaObjectIterator(const VMObjectVanillaObjectIterator&) = delete;
    VMObjectVanillaObjectIterator& operator=(const VMObjectVanillaObjectIterator&) = delete;
  private:
  public:
    VMObjectVanillaObjectIterator(IVM& vm, Container& object, VMObjectVanillaMutex::ReadLock& lock)
      : VMObjectVanillaIterator(vm, object, lock) {
    }
    virtual void print(Printer& printer) const override {
      printer.stream << "[vanilla object iterator]";
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
  };

  class VMObjectVanillaKeyValue : public VMObjectVanillaObject {
    VMObjectVanillaKeyValue(const VMObjectVanillaKeyValue&) = delete;
    VMObjectVanillaKeyValue& operator=(const VMObjectVanillaKeyValue&) = delete;
  public:
    VMObjectVanillaKeyValue(IVM& vm, const HardValue& key, const HardValue& value)
      : VMObjectVanillaObject(vm) {
      this->propertyAdd("key", key);
      this->propertyAdd("value", value);
    }
    virtual const char* getName() const override {
      return "Vanilla key-value pair";
    }
  };

  class VMObjectVanillaFunction : public VMObjectBase {
    VMObjectVanillaFunction(const VMObjectVanillaFunction&) = delete;
    VMObjectVanillaFunction& operator=(const VMObjectVanillaFunction&) = delete;
  private:
    Type ftype;
    HardPtr<IVMCallHandler> handler;
  public:
    VMObjectVanillaFunction(IVM& vm, const Type& ftype, IVMCallHandler& handler)
      : VMObjectBase(vm),
        ftype(ftype),
        handler(&handler) {
      assert(this->ftype != nullptr);
      assert(this->handler != nullptr);
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // Nothing to do
    }
    virtual void print(Printer& printer) const override {
      printer.describe(*this->ftype);
    }
    virtual Type vmRuntimeType() override {
      return this->ftype;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      return this->handler->call(execution, arguments);
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseRuntimeError(execution, "Vanilla function does not support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Vanilla function does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Vanilla function does not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Vanilla function does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Vanilla function does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Vanilla function does not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Vanilla function does not support properties");
    }
  };

  class VMStringProxyBase : public VMObjectBase {
    VMStringProxyBase(const VMStringProxyBase&) = delete;
    VMStringProxyBase& operator=(const VMStringProxyBase&) = delete;
  protected:
    String instance;
    const char* proxy;
  public:
    VMStringProxyBase(IVM& vm, const String& instance, const char* proxy)
      : VMObjectBase(vm),
        instance(instance),
        proxy(proxy) {
      assert(this->proxy != nullptr);
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual void print(Printer& printer) const override {
      printer << "[string proxy " << this->proxy << "]"; // TODO
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseProxyError(execution, "does not support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support properties");
    }
  protected:
    template<typename... ARGS>
    HardValue raiseProxyError(IVMExecution& execution, ARGS&&... args) {
      return this->raiseRuntimeError(execution, "String property '", this->proxy, "()' ", std::forward<ARGS>(args)...);
    }
  };

  class VMStringProxyCompareTo : public VMStringProxyBase {
    VMStringProxyCompareTo(const VMStringProxyCompareTo&) = delete;
    VMStringProxyCompareTo& operator=(const VMStringProxyCompareTo&) = delete;
  public:
    VMStringProxyCompareTo(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "compareTo") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      HardValue argument;
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentByIndex(0, argument)) {
        return this->raiseProxyError(execution, "expects one argument");
      }
      String other;
      if (!argument->getString(other)) {
        return this->raiseProxyError(execution, "expects its argument to be a 'string', but instead got ", describe(argument.get()));
      }
      return execution.createHardValueInt(this->instance.compareTo(other));
    }
  };

  class VMStringProxyContains : public VMStringProxyBase {
    VMStringProxyContains(const VMStringProxyContains&) = delete;
    VMStringProxyContains& operator=(const VMStringProxyContains&) = delete;
  public:
    VMStringProxyContains(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "contains") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      HardValue argument;
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentByIndex(0, argument)) {
        return this->raiseProxyError(execution, "expects one argument");
      }
      String needle;
      if (!argument->getString(needle)) {
        return this->raiseProxyError(execution, "expects its argument to be a 'string', but instead got ", describe(argument.get()));
      }
      return execution.createHardValueBool(this->instance.contains(needle));
    }
  };

  class VMStringProxyEndsWith : public VMStringProxyBase {
    VMStringProxyEndsWith(const VMStringProxyEndsWith&) = delete;
    VMStringProxyEndsWith& operator=(const VMStringProxyEndsWith&) = delete;
  public:
    VMStringProxyEndsWith(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "endsWith") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      HardValue argument;
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentByIndex(0, argument)) {
        return this->raiseProxyError(execution, "expects one argument");
      }
      String needle;
      if (!argument->getString(needle)) {
        return this->raiseProxyError(execution, "expects its argument to be a 'string', but instead got ", describe(argument.get()));
      }
      return execution.createHardValueBool(this->instance.endsWith(needle));
    }
  };

  class VMStringProxyHash : public VMStringProxyBase {
    VMStringProxyHash(const VMStringProxyHash&) = delete;
    VMStringProxyHash& operator=(const VMStringProxyHash&) = delete;
  public:
    VMStringProxyHash(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "hash") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raiseProxyError(execution, "expects no arguments");
      }
      return execution.createHardValueInt(Int(this->instance.hash()));
    }
  };

  class VMStringProxyIndexOf : public VMStringProxyBase {
    VMStringProxyIndexOf(const VMStringProxyIndexOf&) = delete;
    VMStringProxyIndexOf& operator=(const VMStringProxyIndexOf&) = delete;
  public:
    VMStringProxyIndexOf(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "indexOf") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      auto count = arguments.getArgumentCount();
      if ((count < 1) || (count > 2)) {
        return this->raiseProxyError(execution, "expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      String needle;
      if (!arguments.getArgumentByIndex(0, argument) || !argument->getString(needle)) {
        return this->raiseProxyError(execution, "expects its first argument to be a 'string', but instead got ", describe(argument.get()));
      }
      Int retval;
      if (arguments.getArgumentByIndex(1, argument)) {
        Int fromIndex;
        if (!argument->getInt(fromIndex)) {
          return this->raiseProxyError(execution, "expects its optional second argument to be an 'int', but instead got ", describe(argument.get()));
        }
        if (fromIndex < 0) {
          return this->raiseProxyError(execution, "expects its optional second argument to be a non-negative integer, but instead got ", fromIndex);
        }
        retval = this->instance.indexOfString(needle, size_t(fromIndex));
      } else {
        retval = this->instance.indexOfString(needle);
      }
      if (retval < 0) {
        return HardValue::Null;
      }
      return execution.createHardValueInt(retval);
    }
  };

  class VMStringProxyJoin : public VMStringProxyBase {
    VMStringProxyJoin(const VMStringProxyJoin&) = delete;
    VMStringProxyJoin& operator=(const VMStringProxyJoin&) = delete;
  public:
    VMStringProxyJoin(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "join") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      // OPTIMIZE
      StringBuilder sb;
      HardValue argument;
      for (size_t index = 0; arguments.getArgumentByIndex(index, argument); ++index) {
        if (index > 0) {
          sb.add(this->instance);
        }
        sb.add(argument);
      }
      return execution.createHardValueString(sb.build(this->vm->getAllocator()));
    }
  };

  class VMStringProxyLastIndexOf : public VMStringProxyBase {
    VMStringProxyLastIndexOf(const VMStringProxyLastIndexOf&) = delete;
    VMStringProxyLastIndexOf& operator=(const VMStringProxyLastIndexOf&) = delete;
  public:
    VMStringProxyLastIndexOf(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "lastIndexOf") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      auto count = arguments.getArgumentCount();
      if ((count < 1) || (count > 2)) {
        return this->raiseProxyError(execution, "expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      String needle;
      if (!arguments.getArgumentByIndex(0, argument) || !argument->getString(needle)) {
        return this->raiseProxyError(execution, "expects its first argument to be a 'string', but instead got ", describe(argument.get()));
      }
      Int retval;
      if (arguments.getArgumentByIndex(1, argument)) {
        Int fromIndex;
        if (!argument->getInt(fromIndex)) {
          return this->raiseProxyError(execution, "expects its optional second argument to be an 'int', but instead got ", describe(argument.get()));
        }
        if (fromIndex < 0) {
          return this->raiseProxyError(execution, "expects its optional second argument to be a non-negative integer, but instead got ", fromIndex);
        }
        retval = this->instance.lastIndexOfString(needle, size_t(fromIndex));
      } else {
        retval = this->instance.lastIndexOfString(needle);
      }
      if (retval < 0) {
        return HardValue::Null;
      }
      return execution.createHardValueInt(retval);
    }
  };

  class VMStringProxyPadLeft : public VMStringProxyBase {
    VMStringProxyPadLeft(const VMStringProxyPadLeft&) = delete;
    VMStringProxyPadLeft& operator=(const VMStringProxyPadLeft&) = delete;
  public:
    VMStringProxyPadLeft(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "padLeft") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      auto count = arguments.getArgumentCount();
      if ((count < 1) || (count > 2)) {
        return this->raiseProxyError(execution, "expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      Int target;
      if (!arguments.getArgumentByIndex(0, argument) || !argument->getInt(target)) {
        return this->raiseProxyError(execution, "expects its first argument to be an 'int', but instead got ", describe(argument.get()));
      }
      if (target < 0) {
        return this->raiseProxyError(execution, "expects its first argument to be a non-negative integer, but instead got ", target);
      }
      if (arguments.getArgumentByIndex(1, argument)) {
        String padding;
        if (!argument->getString(padding)) {
          return this->raiseProxyError(execution, "expects its optional second argument to be a 'string', but instead got ", describe(argument.get()));
        }
        return execution.createHardValueString(this->instance.padLeft(this->vm->getAllocator(), size_t(target), padding));
      }
      return execution.createHardValueString(this->instance.padLeft(this->vm->getAllocator(), size_t(target)));
    }
  };

  class VMStringProxyPadRight : public VMStringProxyBase {
    VMStringProxyPadRight(const VMStringProxyPadRight&) = delete;
    VMStringProxyPadRight& operator=(const VMStringProxyPadRight&) = delete;
  public:
    VMStringProxyPadRight(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "padRight") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      auto count = arguments.getArgumentCount();
      if ((count < 1) || (count > 2)) {
        return this->raiseProxyError(execution, "expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      Int target;
      if (!arguments.getArgumentByIndex(0, argument) || !argument->getInt(target)) {
        return this->raiseProxyError(execution, "expects its first argument to be an 'int', but instead got ", describe(argument.get()));
      }
      if (target < 0) {
        return this->raiseProxyError(execution, "expects its first argument to be a non-negative integer, but instead got ", target);
      }
      if (arguments.getArgumentByIndex(1, argument)) {
        String padding;
        if (!argument->getString(padding)) {
          return this->raiseProxyError(execution, "expects its optional second argument to be a 'string', but instead got ", describe(argument.get()));
        }
        return execution.createHardValueString(this->instance.padRight(this->vm->getAllocator(), size_t(target), padding));
      }
      return execution.createHardValueString(this->instance.padRight(this->vm->getAllocator(), size_t(target)));
    }
  };

  class VMStringProxyRepeat : public VMStringProxyBase {
    VMStringProxyRepeat(const VMStringProxyRepeat&) = delete;
    VMStringProxyRepeat& operator=(const VMStringProxyRepeat&) = delete;
  public:
    VMStringProxyRepeat(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "repeat") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      HardValue argument;
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentByIndex(0, argument)) {
        return this->raiseProxyError(execution, "expects one argument");
      }
      Int count;
      if (!argument->getInt(count)) {
        return this->raiseProxyError(execution, "expects its argument to be an 'int', but instead got ", describe(argument.get()));
      }
      if (count < 0) {
        return this->raiseProxyError(execution, "expects its argument to be a non-negative integer, but instead got ", count);
      }
      return execution.createHardValueString(this->instance.repeat(this->vm->getAllocator(), size_t(count)));
    }
  };

  class VMStringProxyReplace : public VMStringProxyBase {
    VMStringProxyReplace(const VMStringProxyReplace&) = delete;
    VMStringProxyReplace& operator=(const VMStringProxyReplace&) = delete;
  public:
    VMStringProxyReplace(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "replace") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      auto count = arguments.getArgumentCount();
      if ((count < 2) || (count > 3)) {
        return this->raiseProxyError(execution, "expects two or three arguments, but instead got ", count);
      }
      HardValue argument;
      String needle;
      if (!arguments.getArgumentByIndex(0, argument) || !argument->getString(needle)) {
        return this->raiseProxyError(execution, "expects its first argument to be a 'string', but instead got ", describe(argument.get()));
      }
      String replacement;
      if (!arguments.getArgumentByIndex(1, argument) || !argument->getString(replacement)) {
        return this->raiseProxyError(execution, "expects its second argument to be a 'string', but instead got ", describe(argument.get()));
      }
      Int occurrences = std::numeric_limits<Int>::max();
      if (arguments.getArgumentByIndex(2, argument)) {
        if (!argument->getInt(occurrences)) {
          return this->raiseProxyError(execution, "expects its optional third argument to be an 'int', but instead got ", describe(argument.get()));
        }
      }
      return execution.createHardValueString(this->instance.replace(this->vm->getAllocator(), needle, replacement, occurrences));
    }
  };

  class VMStringProxySlice : public VMStringProxyBase {
    VMStringProxySlice(const VMStringProxySlice&) = delete;
    VMStringProxySlice& operator=(const VMStringProxySlice&) = delete;
  public:
    VMStringProxySlice(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "slice") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      auto count = arguments.getArgumentCount();
      if ((count < 1) || (count > 2)) {
        return this->raiseProxyError(execution, "expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      Int begin;
      if (!arguments.getArgumentByIndex(0, argument) || !argument->getInt(begin)) {
        return this->raiseProxyError(execution, "expects its first argument to be an 'int', but instead got ", describe(argument.get()));
      }
      Int end = std::numeric_limits<Int>::max();
      if (arguments.getArgumentByIndex(1, argument)) {
        if (!argument->getInt(end)) {
          return this->raiseProxyError(execution, "expects its optional second argument to be an 'int', but instead got ", describe(argument.get()));
        }
      }
      return execution.createHardValueString(this->instance.slice(this->vm->getAllocator(), begin, end));
    }
  };

  class VMStringProxyStartsWith : public VMStringProxyBase {
    VMStringProxyStartsWith(const VMStringProxyStartsWith&) = delete;
    VMStringProxyStartsWith& operator=(const VMStringProxyStartsWith&) = delete;
  public:
    VMStringProxyStartsWith(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "startsWith") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      HardValue argument;
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentByIndex(0, argument)) {
        return this->raiseProxyError(execution, "expects one argument");
      }
      String needle;
      if (!argument->getString(needle)) {
        return this->raiseProxyError(execution, "expects its argument to be a 'string', but instead got ", describe(argument.get()));
      }
      return execution.createHardValueBool(this->instance.startsWith(needle));
    }
  };

  class VMStringProxyToString : public VMStringProxyBase {
    VMStringProxyToString(const VMStringProxyToString&) = delete;
    VMStringProxyToString& operator=(const VMStringProxyToString&) = delete;
  public:
    VMStringProxyToString(IVM& vm, const String& instance)
      : VMStringProxyBase(vm, instance, "toString") {
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raiseProxyError(execution, "expects no arguments");
      }
      return execution.createHardValueString(this->instance);
    }
  };


  class VMManifestionBase : public SoftReferenceCountedAllocator<IObject> {
    VMManifestionBase(const VMManifestionBase&) = delete;
    VMManifestionBase& operator=(const VMManifestionBase&) = delete;
  protected:
    Type type;
  public:
    explicit VMManifestionBase(IVM& vm)
      : SoftReferenceCountedAllocator<IObject>(vm.getAllocator()) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual void print(Printer& printer) const override {
      printer << "[manifestation " << this->getName() <<"]";
    }
    virtual Type vmRuntimeType() override {
      return this->type;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments&) override {
      return this->raiseManifestationError(execution, "does not support call semantics");
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raiseManifestationError(execution, "does not support iteration");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseManifestationError(execution, "does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseManifestationError(execution, "does not support indexing");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseManifestationError(execution, "does not support properties");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseManifestationError(execution, "does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseManifestationError(execution, "does not support properties");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raiseManifestationError(execution, "does not support properties");
    }
  protected:
    template<typename... ARGS>
    HardValue raiseRuntimeError(IVMExecution& execution, ARGS&&... args) {
      // TODO: Non-string exception?
      auto message = StringBuilder::concat(this->allocator, std::forward<ARGS>(args)...);
      return execution.raiseRuntimeError(message);
    }
    template<typename... ARGS>
    HardValue raiseManifestationError(IVMExecution& execution, ARGS&&... args) {
      auto message = StringBuilder::concat(this->allocator, "'", this->getName(), "' ", std::forward<ARGS>(args)...);
      return execution.raiseRuntimeError(message);
    }
    virtual const char* getName() const = 0;
  };

  class VMManifestionType : public VMManifestionBase {
    VMManifestionType(const VMManifestionType&) = delete;
    VMManifestionType& operator=(const VMManifestionType&) = delete;
  public:
    explicit VMManifestionType(IVM& vm)
      : VMManifestionBase(vm) {
      this->type = Type::Object; // WIBBLE
    }
  protected:
    virtual const char* getName() const override {
      return "type";
    }
  };

  class VMManifestionVoid : public VMManifestionBase {
    VMManifestionVoid(const VMManifestionVoid&) = delete;
    VMManifestionVoid& operator=(const VMManifestionVoid&) = delete;
  public:
    explicit VMManifestionVoid(IVM& vm)
      : VMManifestionBase(vm) {
      this->type = Type::Object; // WIBBLE
    }
  protected:
    virtual const char* getName() const override {
      return "void";
    }
  };

  class VMManifestionBool : public VMManifestionBase {
    VMManifestionBool(const VMManifestionBool&) = delete;
    VMManifestionBool& operator=(const VMManifestionBool&) = delete;
  public:
    explicit VMManifestionBool(IVM& vm)
      : VMManifestionBase(vm) {
      this->type = Type::Object; // WIBBLE
    }
  protected:
    virtual const char* getName() const override {
      return "bool";
    }
  };

  class VMManifestionInt : public VMManifestionBase {
    VMManifestionInt(const VMManifestionInt&) = delete;
    VMManifestionInt& operator=(const VMManifestionInt&) = delete;
  public:
    explicit VMManifestionInt(IVM& vm)
      : VMManifestionBase(vm) {
      this->type = Type::Object; // WIBBLE
    }
  protected:
    virtual const char* getName() const override {
      return "int";
    }
  };

  class VMManifestionFloat : public VMManifestionBase {
    VMManifestionFloat(const VMManifestionFloat&) = delete;
    VMManifestionFloat& operator=(const VMManifestionFloat&) = delete;
  public:
    explicit VMManifestionFloat(IVM& vm)
      : VMManifestionBase(vm) {
      this->type = Type::Object; // WIBBLE
    }
  protected:
    virtual const char* getName() const override {
      return "float";
    }
  };

  class VMManifestionString : public VMManifestionBase {
    VMManifestionString(const VMManifestionString&) = delete;
    VMManifestionString& operator=(const VMManifestionString&) = delete;
  public:
    explicit VMManifestionString(IVM& vm)
      : VMManifestionBase(vm) {
      this->type = Type::Object; // WIBBLE
    }
  protected:
    virtual const char* getName() const override {
      return "string";
    }
  };

  class VMManifestionObject : public VMManifestionBase {
    VMManifestionObject(const VMManifestionObject&) = delete;
    VMManifestionObject& operator=(const VMManifestionObject&) = delete;
  public:
    explicit VMManifestionObject(IVM& vm)
      : VMManifestionBase(vm) {
      this->type = Type::Object; // WIBBLE
    }
  protected:
    virtual const char* getName() const override {
      return "object";
    }
  };

  class VMManifestionAny : public VMManifestionBase {
    VMManifestionAny(const VMManifestionAny&) = delete;
    VMManifestionAny& operator=(const VMManifestionAny&) = delete;
  public:
    explicit VMManifestionAny(IVM& vm)
      : VMManifestionBase(vm) {
      this->type = Type::Object; // WIBBLE
    }
  protected:
    virtual const char* getName() const override {
      return "any";
    }
  };

  class ObjectBuilderInstance : public VMObjectVanillaObject {
    ObjectBuilderInstance(const ObjectBuilderInstance&) = delete;
    ObjectBuilderInstance& operator=(const ObjectBuilderInstance&) = delete;
  public:
    explicit ObjectBuilderInstance(IVM& vm)
      : VMObjectVanillaObject(vm) {
    }
    virtual void withProperty(const HardValue& property, const HardValue& value, bool readonly) = 0;
  };

  class ObjectBuilderRuntimeError : public ObjectBuilderInstance {
    ObjectBuilderRuntimeError(const ObjectBuilderRuntimeError&) = delete;
    ObjectBuilderRuntimeError& operator=(const ObjectBuilderRuntimeError&) = delete;
  private:
    String message;
    HardPtr<IVMCallStack> callstack;
  public:
    ObjectBuilderRuntimeError(IVM& vm, const String& message, const HardPtr<IVMCallStack>& callstack)
      : ObjectBuilderInstance(vm),
        message(message),
        callstack(callstack) {
    }
    virtual const char* getName() const override {
      return "Runtime error";
    }
    virtual void withProperty(const HardValue& property, const HardValue& value, bool readonly) override {
      // TODO
      (void)readonly;
      this->propertyAdd(property, value);
    }
    virtual void print(Printer& printer) const override {
      if (this->callstack != nullptr) {
        this->callstack->print(printer);
      }
      printer << this->message;
    }
    template<typename T>
    void withProperty(const char* property, T value) {
      // TODO still needed?
      this->withProperty(this->vm->createHardValue(property), this->vm->createHardValue(value), false);
    }
    void withPropertyOptional(const char* property, const String& value) {
      // TODO still needed?
      if (!value.empty()) {
        this->withProperty(property, value);
      }
    }
  };

  class ObjectBuilder : public HardReferenceCounted<IObjectBuilder> {
    ObjectBuilder(const ObjectBuilder&) = delete;
    ObjectBuilder& operator=(const ObjectBuilder&) = delete;
  protected:
    IVM& vm;
    HardPtr<ObjectBuilderInstance> instance;
  public:
    ObjectBuilder(IVM& vm, HardPtr<ObjectBuilderInstance>&& instance)
      : vm(vm),
        instance(std::move(instance)) {
    }
    virtual void addReadWriteProperty(const HardValue& property, const HardValue& value) override {
      assert(this->instance != nullptr);
      this->instance->withProperty(property, value, false);
    }
    virtual void addReadOnlyProperty(const HardValue& property, const HardValue& value) override {
      assert(this->instance != nullptr);
      this->instance->withProperty(property, value, true);
    }
    virtual HardObject build() override {
      assert(this->instance != nullptr);
      HardObject result{ this->instance.get() };
      this->instance = nullptr;
      return result;
    }
  protected:
    virtual void hardDestroy() const override {
      this->vm.getAllocator().destroy(this);
    }
  };
}

VMObjectVanillaMutex::ReadLock::ReadLock(VMObjectVanillaMutex& mutex)
  : lock(mutex.mutex),
    modifications(mutex.modifications) {
}

VMObjectVanillaMutex::WriteLock::WriteLock(VMObjectVanillaMutex& mutex)
  : mutex(mutex),
    lock(mutex.mutex),
    modified(false) {
}

VMObjectVanillaMutex::WriteLock::~WriteLock() {
  if (this->modified) {
    this->mutex.modifications++;
  }
}

egg::ovum::HardValue VMObjectVanillaArray::vmIterate(IVMExecution& execution) {
  VMObjectVanillaMutex::ReadLock lock{ this->mutex };
  auto iterator = makeHardObject<VMObjectVanillaArrayIterator>(*this->vm, *this, lock);
  return execution.createHardValueObject(iterator);
}

egg::ovum::HardValue VMObjectVanillaObject::vmIterate(IVMExecution& execution) {
  VMObjectVanillaMutex::ReadLock lock{ this->mutex };
  auto iterator = makeHardObject<VMObjectVanillaObjectIterator>(*this->vm, *this, lock);
  return execution.createHardValueObject(iterator);
}

egg::ovum::SoftObject::SoftObject(IVM& vm, const HardObject& instance)
  : SoftPtr(vm.acquireSoftObject(instance.get())) {
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

egg::ovum::HardObject egg::ovum::ObjectFactory::createVanillaArray(IVM& vm) {
  return makeHardObject<VMObjectVanillaArray>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createVanillaObject(IVM& vm) {
  return makeHardObject<VMObjectVanillaObject>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createVanillaKeyValue(IVM& vm, const HardValue& key, const HardValue& value) {
  return makeHardObject<VMObjectVanillaKeyValue>(vm, key, value);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createVanillaFunction(IVM& vm, const Type& ftype, IVMCallHandler& handler) {
  return makeHardObject<VMObjectVanillaFunction>(vm, ftype, handler);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyCompareTo(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyCompareTo>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyContains(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyContains>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyEndsWith(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyEndsWith>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyHash(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyHash>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyIndexOf(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyIndexOf>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyJoin(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyJoin>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyLastIndexOf(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyLastIndexOf>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyPadLeft(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyPadLeft>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyPadRight(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyPadRight>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyRepeat(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyRepeat>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyReplace(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyReplace>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxySlice(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxySlice>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyStartsWith(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyStartsWith>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createStringProxyToString(IVM& vm, const String& instance) {
  return makeHardObject<VMStringProxyToString>(vm, instance);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createManifestationType(IVM& vm) {
  return makeHardObject<VMManifestionType>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createManifestationVoid(IVM& vm) {
  return makeHardObject<VMManifestionVoid>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createManifestationBool(IVM& vm) {
  return makeHardObject<VMManifestionBool>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createManifestationInt(IVM& vm) {
  return makeHardObject<VMManifestionInt>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createManifestationFloat(IVM& vm) {
  return makeHardObject<VMManifestionFloat>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createManifestationString(IVM& vm) {
  return makeHardObject<VMManifestionString>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createManifestationObject(IVM& vm) {
  return makeHardObject<VMManifestionObject>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createManifestationAny(IVM& vm) {
  return makeHardObject<VMManifestionAny>(vm);
}

egg::ovum::HardPtr<egg::ovum::IObjectBuilder> egg::ovum::ObjectFactory::createRuntimeErrorBuilder(IVM& vm, const String& message, const HardPtr<IVMCallStack>& callstack) {
  auto& allocator = vm.getAllocator();
  HardPtr instance{ allocator.makeRaw<ObjectBuilderRuntimeError>(vm, message, callstack) };
  instance->withProperty("message", message);
  if (callstack != nullptr) {
    instance->withPropertyOptional("resource", callstack->getResource());
    auto* range = callstack->getSourceRange();
    if (range != nullptr) {
      if ((range->begin.line != 0) || (range->begin.column != 0)) {
        instance->withProperty("line", int(range->begin.line));
        if (range->begin.column != 0) {
          instance->withProperty("column", int(range->begin.column));
        }
      }
    }
    instance->withPropertyOptional("function", callstack->getFunction());
  }
  return HardPtr{ allocator.makeRaw<ObjectBuilder>(vm, std::move(instance)) };
}
