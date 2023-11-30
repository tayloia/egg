#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  template<typename T>
  std::string describe(const T& value) {
    std::stringstream ss;
    Print::describe(ss, value, Print::Options::DEFAULT);
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
        return this->raiseRuntimeError(execution, "Builtin 'assert()' expects a 'bool' argument");
      }
      if (!success) {
        return this->raiseRuntimeError(execution, "Assertion failure");
      }
      return HardValue::Void;
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'assert()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
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
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'print()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
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
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue& index) override {
      SoftKey key(*this->vm, index.get());
      auto pfound = this->properties.find(key);
      if (pfound == this->properties.end()) {
        return this->raiseRuntimeError(execution, "TODO: Cannot find index '", key, "' in expando object");
      }
      return this->vm->getSoftValue(pfound->second);
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue& index, const HardValue& value) override {
      SoftKey key(*this->vm, index.get());
      auto pfound = this->properties.find(key);
      if (pfound == this->properties.end()) {
        pfound = this->properties.emplace_hint(pfound, std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(*this->vm));
      }
      if (!this->vm->setSoftValue(pfound->second, value)) {
        return this->raiseRuntimeError(execution, "TODO: Cannot modify index '", key, "'");
      }
      return HardValue::Void;
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) override {
      SoftKey pname(*this->vm, property.get());
      auto pfound = this->properties.find(pname);
      if (pfound == this->properties.end()) {
        return this->raiseRuntimeError(execution, "TODO: Cannot find property '", pname, "' in expando object");
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
        return this->raiseRuntimeError(execution, "TODO: Cannot modify property '", pname, "'");
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
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'expando()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
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
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Builtin 'collector()' does not support properties");
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
      printer << "[string." << this->proxy << "]"; // TODO
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseProxyError(execution, "does not support properties");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
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

  class VMObjectRuntimeError : public VMObjectBase {
    VMObjectRuntimeError(const VMObjectRuntimeError&) = delete;
    VMObjectRuntimeError& operator=(const VMObjectRuntimeError&) = delete;
  private:
    std::map<SoftKey, SoftValue> properties;
    String message;
    HardPtr<IVMCallStack> callstack;
  public:
    VMObjectRuntimeError(IVM& vm, const String& message, const HardPtr<IVMCallStack>& callstack)
      : VMObjectBase(vm),
        message(message),
        callstack(callstack) {
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      for (const auto& property : this->properties) {
        property.first.visit(visitor);
        property.second.visit(visitor);
      }
    }
    virtual void print(Printer& printer) const override {
      if (this->callstack != nullptr) {
        this->callstack->print(printer);
      }
      printer << this->message;
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments&) override {
      return this->raiseRuntimeError(execution, "Runtime error objects do not support function call semantics");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Runtime error objects do not support indexing");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raiseRuntimeError(execution, "Runtime error objects do not support indexing");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) override {
      // TODO access 'callstack' and 'message'
      SoftKey pname(*this->vm, property.get());
      auto pfound = this->properties.find(pname);
      if (pfound == this->properties.end()) {
        return this->raiseRuntimeError(execution, "Cannot find property '", pname, "' in runtime error object");
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
        return this->raiseRuntimeError(execution, "Cannot modify property '", pname, "'");
      }
      return HardValue::Void;
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

egg::ovum::HardObject egg::ovum::ObjectFactory::createRuntimeError(IVM& vm, const String& message, const HardPtr<IVMCallStack>& callstack) {
  return makeHardObject<VMObjectRuntimeError>(vm, message, callstack);
}
