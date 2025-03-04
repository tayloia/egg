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

  ValueMutationOp toMutationOp(const String& value) {
    // OPTIMIZE
    if (value.equals("=")) {
      return ValueMutationOp::Assign;
    }
    if (value.equals("--")) {
      return ValueMutationOp::Decrement;
    }
    if (value.equals("++")) {
      return ValueMutationOp::Increment;
    }
    if (value.equals("+=")) {
      return ValueMutationOp::Add;
    }
    if (value.equals("-=")) {
      return ValueMutationOp::Subtract;
    }
    if (value.equals("*=")) {
      return ValueMutationOp::Multiply;
    }
    if (value.equals("/=")) {
      return ValueMutationOp::Divide;
    }
    if (value.equals("%=")) {
      return ValueMutationOp::Remainder;
    }
    if (value.equals("&=")) {
      return ValueMutationOp::BitwiseAnd;
    }
    if (value.equals("|=")) {
      return ValueMutationOp::BitwiseOr;
    }
    if (value.equals("^=")) {
      return ValueMutationOp::BitwiseXor;
    }
    if (value.equals("<<=")) {
      return ValueMutationOp::ShiftLeft;
    }
    if (value.equals(">>=")) {
      return ValueMutationOp::ShiftRight;
    }
    if (value.equals(">>>=")) {
      return ValueMutationOp::ShiftRightUnsigned;
    }
    if (value.equals("<|=")) {
      return ValueMutationOp::Minimum;
    }
    if (value.equals(">|=")) {
      return ValueMutationOp::Maximum;
    }
    if (value.equals("!!=")) {
      return ValueMutationOp::IfVoid;
    }
    if (value.equals("??=")) {
      return ValueMutationOp::IfNull;
    }
    if (value.equals("||=")) {
      return ValueMutationOp::IfFalse;
    }
    if (value.equals("&&=")) {
      return ValueMutationOp::IfTrue;
    }
    return ValueMutationOp::Noop;
  }

  bool parseMutationOp(const HardValue& value, ValueMutationOp& mutation, size_t& operands) {
    String text;
    if (value->getString(text)) {
      mutation = toMutationOp(text);
      operands = ((mutation == ValueMutationOp::Decrement) || (mutation == ValueMutationOp::Increment)) ? 1u : 2u;
      return mutation != ValueMutationOp::Noop;
    }
    return false;
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
    IVM& vm;
    template<typename... ARGS>
    HardValue raiseRuntimeError(IVMExecution& execution, ARGS&&... args) {
      // TODO: Non-string exception?
      auto message = StringBuilder::concat(this->vm.getAllocator(), std::forward<ARGS>(args)...);
      return execution.raiseRuntimeError(message, nullptr);
    }
    template<typename... ARGS>
    HardValue raisePrefixError(IVMExecution& execution, ARGS&&... args) {
      StringBuilder sb;
      this->printPrefix(sb);
      sb.add(std::forward<ARGS>(args)...);
      return execution.raiseRuntimeError(sb.build(this->vm.getAllocator()), nullptr);
    }
    virtual void printPrefix(Printer& printer) const = 0;
  public:
    explicit VMObjectBase(IVM& vm)
      : SoftReferenceCounted<IObject>(),
        vm(vm) {
      // Initially not adopted by any basket
      assert(this->basket == nullptr);
    }
    virtual void hardDestroy() const override {
      this->vm.getAllocator().destroy(this);
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments&) override {
      return this->raisePrefixError(execution, " does not support function call semantics");
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return this->raisePrefixError(execution, " does not support iteration");
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support properties (get)");
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support properties (set)");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support properties (mut)");
    }
    virtual HardValue vmPropertyRef(IVMExecution& execution, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support properties (ref)");
    }
    virtual HardValue vmPropertyDel(IVMExecution& execution, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support properties (del)");
    }
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support indexing (get)");
    }
    virtual HardValue vmIndexSet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support indexing (set)");
    }
    virtual HardValue vmIndexMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support indexing (mut)");
    }
    virtual HardValue vmIndexRef(IVMExecution& execution, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support indexing (ref)");
    }
    virtual HardValue vmIndexDel(IVMExecution& execution, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support indexing (del)");
    }
    virtual HardValue vmPointeeGet(IVMExecution& execution) override {
      return this->raisePrefixError(execution, " does not support pointer semantics (get)");
    }
    virtual HardValue vmPointeeSet(IVMExecution& execution, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support pointer semantics (set)");
    }
    virtual HardValue vmPointeeMut(IVMExecution& execution, ValueMutationOp, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support pointer semantics (mut)");
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
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Member handler";
    }
  public:
    VMObjectMemberHandler(IVM& vm, T& instance, Handler handler)
      : VMObjectBase(vm),
        instance(&instance),
        handler(handler) {
      this->vm.getBasket().take(*this->instance);
      assert(this->instance != nullptr);
      assert(this->handler != nullptr);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      assert(this->instance != nullptr);
      visitor.visit(*this->instance);
    }
    virtual int print(Printer& printer) const override {
      printer << "[member handler]";
      return 0;
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
    Type containerType;
    Accessability accessability;
    VMObjectVanillaContainer(IVM& vm, const Type& containerType, Accessability accessability)
      : VMObjectBase(vm),
        containerType(containerType),
        accessability(accessability) {
      assert(this->containerType.validate());
    }
    virtual void printPrefix(Printer& printer) const override {
      printer << "Instance of '";
      printer.describe(*this->containerType);
      printer << "'";
    }
  public:
    virtual Type vmRuntimeType() override {
      return this->containerType;
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
        return this->raisePrefixError(execution, " does not support function call arguments");
      }
      return this->container.iteratorNext(execution, this->state);
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
    Type elementType;
  public:
    VMObjectVanillaArray(IVM& vm, const Type& containerType, const Type& elementType, Accessability accessability)
      : VMObjectVanillaContainer(vm, containerType, accessability),
        elements(),
        elementType(elementType) {
    }
    HardValue iteratorNext(IVMExecution& execution, IteratorState& state) {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      if (lock.modifications != state.modifications) {
        return this->raisePrefixError(execution, " has been modified during iteration");
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
    virtual int print(Printer& printer) const override {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      Print::Options options{ printer.options };
      options.quote = '"';
      Printer inner{ printer.stream, options };
      char separator = '[';
      for (const auto& element : this->elements) {
        inner << separator << element.get();
        separator = ',';
      }
      if (separator == '[') {
        inner << '[';
      }
      inner << ']';
      return 0;
    }
    virtual HardValue vmIterate(IVMExecution& execution) override;
    virtual HardValue vmIndexGet(IVMExecution& execution, const HardValue& index) override {
      if (!this->hasAccessability(Accessability::Get)) {
        return this->raisePrefixError(execution, " does not permit reading indexed values");
      }
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
      if (!this->hasAccessability(Accessability::Set)) {
        return this->raisePrefixError(execution, " does not permit writing indexed values");
      }
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
      if (!this->hasAccessability(Accessability::Mut)) {
        return this->raisePrefixError(execution, " does not permit modifying indexed values");
      }
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      Int ivalue;
      if (!index->getInt(ivalue)) {
        return this->raiseRuntimeError(execution, "Expected array index value to be an 'int', but instead got ", describe(index.get()));
      }
      auto uvalue = size_t(ivalue);
      if (uvalue > this->elements.size()) {
        return this->raiseRuntimeError(execution, "Array index ", ivalue, " is out of range for an array of length ", this->elements.size());
      }
      return execution.mutSoftValue(this->elements[uvalue], mutation, value);
    }
    virtual HardValue vmIndexRef(IVMExecution& execution, const HardValue& index) override {
      if (!this->hasAccessability(Accessability::Ref)) {
        return this->raisePrefixError(execution, " does not permit referencing indexed values");
      }
      // TODO do we need to read or write lock here?
      return execution.refIndex(HardObject{ this }, index, Modifiability::All, this->elementType);
    }
    virtual HardValue vmIndexDel(IVMExecution& execution, const HardValue&) override {
      return this->raisePrefixError(execution, " does not permit deleting indexed values");
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
      auto instance = makeHardObject<VMObjectMemberHandler<VMObjectVanillaArray>>(this->vm, *this, handler);
      return this->vm.createHardValueObject(instance);
    }
    HardValue lengthMut(IVMExecution& execution, VMObjectVanillaMutex::WriteLock& lock, ValueMutationOp mutation, const HardValue& rhs) {
      auto value = ValueFactory::createInt(this->vm.getAllocator(), Int(this->elements.size()));
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
        this->elements.emplace_back(this->vm, HardValue::Null);
        diff--;
      }
      while (diff < 0) {
        this->elements.pop_back();
        diff++;
      }
      return success;
    }
    bool hasAccessability(Accessability bits) const {
      return Bits::hasAllSet(this->accessability, bits);
    }
    HardValue vmCallPush(IVMExecution&, const ICallArguments& arguments) {
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      HardValue argument;
      for (size_t index = 0; arguments.getArgumentValueByIndex(index, argument); ++index) {
        this->elements.emplace_back(this->vm, argument);
        lock.modified = true;
      }
      return HardValue::Void;
    }
  };

  class VMObjectVanillaArrayIterator : public VMObjectVanillaIterator<VMObjectVanillaArray, VMObjectVanillaArray::IteratorState> {
    VMObjectVanillaArrayIterator(const VMObjectVanillaArrayIterator&) = delete;
    VMObjectVanillaArrayIterator& operator=(const VMObjectVanillaArrayIterator&) = delete;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Array iterator";
    }
  public:
    VMObjectVanillaArrayIterator(IVM& vm, Container& array, VMObjectVanillaMutex::ReadLock& lock)
      : VMObjectVanillaIterator(vm, array, lock) {
    }
    virtual int print(Printer& printer) const override {
      printer << "[vanilla array iterator]";
      return 0;
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
    Type unknownType;
    std::map<SoftKey, SoftValue, SoftComparator> properties;
    std::map<SoftKey, Accessability, SoftComparator> accessabilities;
    std::map<SoftKey, Type, SoftComparator> types;
    std::vector<SoftKey> keys;
  public:
    VMObjectVanillaObject(IVM& vm, const Type& containerType, Accessability accessability)
      : VMObjectVanillaContainer(vm, containerType, accessability),
        unknownType(VMObjectVanillaObject::determineUnknownType(containerType)) {
      assert(this->unknownType.validate());
    }
    HardValue iteratorNext(IVMExecution& execution, IteratorState& state) {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      if (lock.modifications != state.modifications) {
        return this->raisePrefixError(execution, " has been modified during iteration");
      }
      if (state.index >= this->keys.size()) {
        return HardValue::Void;
      }
      const auto& softkey = this->keys[state.index++];
      auto key = this->vm.getSoftKey(softkey);
      auto found = this->properties.find(softkey);
      if (found == this->properties.end()) {
        return this->raisePrefixError(execution, " is missing property '", key.get(), "' during iteration");
      }
      const auto& softvalue = found->second;
      auto value = this->vm.getSoftValue(softvalue);
      auto object = ObjectFactory::createVanillaKeyValue(this->vm, key, value, Accessability::Get);
      return execution.createHardValueObject(object);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      // We only need to iterate around the property member elements as the key vectors are simply duplicates
      for (const auto& property : this->properties) {
        property.first.visit(visitor);
        property.second.visit(visitor);
      }
    }
    virtual int print(Printer& printer) const override {
      Print::Options options{ printer.options };
      options.quote = '"';
      Printer quoted{ printer.stream, options };
      Print::Options unoptions{ printer.options };
      unoptions.quote = '\0';
      Printer unquoted{ printer.stream, unoptions };
      char separator = '{';
      for (const auto& key : this->keys) {
        unquoted << separator << key.get() << ':';
        quoted << this->properties.find(key)->second.get();
        separator = ',';
      }
      if (separator == '{') {
        unquoted << '{';
      }
      unquoted << '}';
      return 0;
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
    virtual HardValue vmIndexRef(IVMExecution& execution, const HardValue& index) override {
      return this->propertyRef(execution, index, Modifiability::All);
    }
    virtual HardValue vmIndexDel(IVMExecution& execution, const HardValue& index) override {
      return this->propertyDel(execution, index);
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
    virtual HardValue vmPropertyRef(IVMExecution& execution, const HardValue& property) override {
      return this->propertyRef(execution, property, Modifiability::All);
    }
    virtual HardValue vmPropertyDel(IVMExecution& execution, const HardValue& property) override {
      return this->propertyDel(execution, property);
    }
  private:
    HardValue propertyGet(IVMExecution& execution, const HardValue& property) {
      VMObjectVanillaMutex::ReadLock lock{ this->mutex };
      auto pfound = this->properties.find(property);
      if (pfound == this->properties.end()) {
        return this->raisePrefixError(execution, " does not contain property '", property, "'");
      }
      if (!this->hasAccessability(property, Accessability::Get)) {
        return this->raisePrefixError(execution, " does not permit getting property '", property, "'");
      }
      return execution.getSoftValue(pfound->second);
    }
    HardValue propertySet(IVMExecution& execution, const HardValue& property, const HardValue& value) {
      if (!this->hasAccessability(property, Accessability::Set)) {
        return this->raisePrefixError(execution, " does not permit setting property '", property, "'");
      }
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      auto pfound = this->properties.find(property);
      if (pfound == this->properties.end()) {
        if (this->unknownType == Type::Void) {
          return this->raisePrefixError(execution, " does not permit creating property '", property, "'");
        }
        pfound = this->propertyCreate(property);
      }
      auto ptype = this->propertyType(property);
      if (ptype == nullptr) {
        // No need to type-check
        if (!execution.setSoftValue(pfound->second, value)) {
          return this->raiseRuntimeError(execution, "Type mismatch setting '", describe(*this->containerType), "' property '", property, "' to ", describe(value.get()));
        }
      } else {
        // Type-check the assignment
        if (!execution.assignValue(pfound->second.get(), ptype, value.get())) {
          return this->raiseRuntimeError(execution, "Type mismatch setting '", describe(*this->containerType), "' property '", property, "' (declared as '", describe(*ptype), "') to ", describe(value.get()));
        }
      }
      return HardValue::Void;
    }
    HardValue propertyMut(IVMExecution& execution, const HardValue& property, ValueMutationOp mutation, const HardValue& value) {
      if (!this->hasAccessability(property, Accessability::Mut)) {
        return this->raisePrefixError(execution, " does not permit modifying property '", property, "'");
      }
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      auto pfound = this->properties.find(property);
      if (pfound == this->properties.end()) {
        // Unknown property
        if ((mutation != ValueMutationOp::Assign) && (mutation != ValueMutationOp::IfVoid)) {
          return this->raisePrefixError(execution, " does not contain property '", property, "'");
        }
        if (this->unknownType == Type::Void) {
          return this->raisePrefixError(execution, " does not permit creating property '", property, "'");
        }
        pfound = this->propertyCreate(property);
      }
      // TODO type check
      return execution.mutSoftValue(pfound->second, mutation, value);
    }
    HardValue propertyRef(IVMExecution& execution, const HardValue& property, Modifiability modifiability) {
      if (!this->hasAccessability(property, Accessability::Ref)) {
        return this->raisePrefixError(execution, " does not permit referencing property '", property, "'");
      }
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      auto ptype = this->propertyType(property);
      if (ptype == nullptr) {
        return this->raisePrefixError(execution, " does not contain property '", property, "'");
      }
      return execution.refProperty(HardObject{ this }, property, modifiability, ptype);
    }
    HardValue propertyDel(IVMExecution& execution, const HardValue& property) {
      if (!this->hasAccessability(property, Accessability::Del)) {
        return this->raisePrefixError(execution, " does not permit deleting property '", property, "'");
      }
      VMObjectVanillaMutex::WriteLock lock{ this->mutex };
      auto pfound = this->properties.find(property);
      if (pfound == this->properties.end()) {
        // Didn't exist
        return HardValue::Void;
      }
      auto& needle = pfound->first.get();
      std::erase_if(this->keys, [&needle](const SoftKey& key) { return SoftKey::compare(key.get(), needle) == 0; });
      auto result = execution.getSoftValue(pfound->second);
      this->properties.erase(pfound);
      return result;
    }
  protected:
    std::map<SoftKey, SoftValue, SoftComparator>::iterator propertyCreate(const HardValue& pkey) {
      auto pair = this->properties.emplace(std::piecewise_construct, std::forward_as_tuple(this->vm, pkey), std::forward_as_tuple(this->vm));
      assert(pair.first != this->properties.end());
      assert(pair.second);
      this->keys.emplace_back(pair.first->first);
      return pair.first;
    }
    Accessability propertyAccessibility(const HardValue& pkey) const {
      auto afound = this->accessabilities.find(pkey);
      if (afound == this->accessabilities.end()) {
        return this->accessability;
      }
      return afound->second;
    }
    Type propertyType(const HardValue& pkey) const {
      auto tfound = this->types.find(pkey);
      if (tfound == this->types.end()) {
        return this->unknownType;
      }
      return tfound->second;
    }
    bool hasAccessability(const HardValue& pkey, Accessability bits) const {
      return Bits::hasAllSet(this->propertyAccessibility(pkey), bits);
    }
    void propertyEmplace(const HardValue& pkey, const Type& ptype, const HardValue* pvalue, Accessability paccessability) {
      // Only used during construction, so implicitly thread-safe
      auto emplaced = this->properties.emplace(std::piecewise_construct, std::forward_as_tuple(this->vm, pkey), std::forward_as_tuple(this->vm));
      assert(emplaced.first != this->properties.end());
      assert(emplaced.second);
      auto success = (pvalue == nullptr) || this->vm.setSoftValue(emplaced.first->second, *pvalue);
      if (success) {
        this->keys.emplace_back(emplaced.first->first);
        if (ptype != nullptr) {
          this->types.emplace(std::piecewise_construct, std::forward_as_tuple(this->vm, pkey), std::forward_as_tuple(ptype));
        }
        if (paccessability != this->accessability) {
          this->accessabilities.emplace(std::piecewise_construct, std::forward_as_tuple(this->vm, pkey), std::forward_as_tuple(paccessability));
        }
      } else {
        this->properties.erase(emplaced.first);
      }
      assert(success);
    }
  private:
    static Type determineUnknownType(const Type& runtimeType) {
      assert(runtimeType.validate());
      if (Bits::hasAnySet(runtimeType->getPrimitiveFlags(), ValueFlags::Object)) {
        // Allow anything
        return Type::AnyQ;
      }
      auto count = runtimeType->getShapeCount();
      for (size_t index = 0; index < count; ++index) {
        auto shape = runtimeType->getShape(index);
        if ((shape != nullptr) && (shape->dotable != nullptr) && !shape->dotable->isClosed()) {
          // Type is open (can have arbitrary property keys)
          auto ptype = shape->dotable->getType({});
          if (ptype != nullptr) {
            return ptype;
          }
        }
      }
      return Type::Void;
    }
  };

  class VMObjectVanillaObjectIterator : public VMObjectVanillaIterator<VMObjectVanillaObject, VMObjectVanillaObject::IteratorState> {
    VMObjectVanillaObjectIterator(const VMObjectVanillaObjectIterator&) = delete;
    VMObjectVanillaObjectIterator& operator=(const VMObjectVanillaObjectIterator&) = delete;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Object iterator";
    }
  public:
    VMObjectVanillaObjectIterator(IVM& vm, Container& object, VMObjectVanillaMutex::ReadLock& lock)
      : VMObjectVanillaIterator(vm, object, lock) {
    }
    virtual int print(Printer& printer) const override {
      printer << "[vanilla object iterator]";
      return 0;
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
  };

  class VMObjectVanillaKeyValue : public VMObjectVanillaObject {
    VMObjectVanillaKeyValue(const VMObjectVanillaKeyValue&) = delete;
    VMObjectVanillaKeyValue& operator=(const VMObjectVanillaKeyValue&) = delete;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Key-value pair";
    }
  public:
    VMObjectVanillaKeyValue(IVM& vm, const HardValue& key, const HardValue& value, Accessability accessability)
      : VMObjectVanillaObject(vm, Type::Object, accessability) {
      this->propertyEmplace(this->vm.createHardValue("key"), key->getRuntimeType(), &key, Accessability::Get);
      this->propertyEmplace(this->vm.createHardValue("value"), value->getRuntimeType(), &value, Accessability::Get);
    }
  };

  class VMObjectVanillaManifestation : public VMObjectVanillaContainer {
    VMObjectVanillaManifestation(const VMObjectVanillaManifestation&) = delete;
    VMObjectVanillaManifestation& operator=(const VMObjectVanillaManifestation&) = delete;
  private:
    Type infratype;
    std::map<SoftKey, SoftValue, SoftComparator> properties;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Manifestation";
    }
  public:
    VMObjectVanillaManifestation(IVM& vm, const Type& infratype, const Type& metatype)
      : VMObjectVanillaContainer(vm, metatype, Accessability::Get | Accessability::Set),
        infratype(infratype) {
      assert(this->infratype.validate());
    }
    virtual ~VMObjectVanillaManifestation() override {
      // If we go out of scope before freezing, the manifestation failed
      if (this->accessability != Accessability::Get) {
        this->vm.finalizeManifestation(this->infratype, HardObject());
      }
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      for (const auto& property : this->properties) {
        property.first.visit(visitor);
        property.second.visit(visitor);
      }
    }
    virtual int print(Printer& printer) const override {
      printer << "[manifestation]";
      return 0;
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) override {
      auto pfound = this->properties.find(property);
      if (pfound == this->properties.end()) {
        return this->raisePrefixError(execution, " does not contain property '", property, "'");
      }
      return execution.getSoftValue(pfound->second);
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue& property, const HardValue& value) override {
      if (!Bits::hasAllSet(this->accessability, Accessability::Set)) {
        // We're frozen
        return this->raisePrefixError(execution, " does not support property modification (set)");
      }
      if (property->getNull()) {
        // Freeze the instance
        this->accessability = Accessability::Get;
        this->vm.finalizeManifestation(this->infratype, HardObject(this));
        return HardValue::Void;
      }
      auto pfound = this->properties.find(property);
      if (pfound == this->properties.end()) {
        auto pair = this->properties.emplace(std::piecewise_construct, std::forward_as_tuple(this->vm, property), std::forward_as_tuple(this->vm));
        assert(pair.first != this->properties.end());
        assert(pair.second);
        pfound = pair.first;
      }
      if (!execution.setSoftValue(pfound->second, value)) {
        return this->raisePrefixError(execution, " cannot modify property '", property, "'");
      }
      return HardValue::Void;
    }
  };

  class VMObjectVanillaCaptures : public VMObjectBase, public IVMCallCaptures {
    VMObjectVanillaCaptures(const VMObjectVanillaCaptures&) = delete;
    VMObjectVanillaCaptures& operator=(const VMObjectVanillaCaptures&) = delete;
  protected:
    Type ftype;
    std::vector<VMCallCapture> captures;
  public:
    VMObjectVanillaCaptures(IVM& vm, const Type& ftype, std::vector<VMCallCapture>&& captures)
      : VMObjectBase(vm),
        ftype(ftype),
        captures(std::move(captures)) {
      assert(this->ftype != nullptr);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      for (const auto& capture : this->captures) {
        assert(capture.type != nullptr);
        visitor.visit(*capture.type);
        assert(capture.soft != nullptr);
        visitor.visit(*capture.soft);
      }
    }
    virtual int print(Printer& printer) const override {
      printer.describe(*this->ftype);
      return 0;
    }
    virtual Type vmRuntimeType() override {
      return this->ftype;
    }
    virtual size_t getCaptureCount() const override {
      return this->captures.size();
    }
    virtual const VMCallCapture* getCaptureByIndex(size_t index) const override {
      if (index < this->captures.size()) {
        return &this->captures[index];
      }
      return nullptr;
    }
  };

  class VMObjectVanillaFunction : public VMObjectVanillaCaptures {
    VMObjectVanillaFunction(const VMObjectVanillaFunction&) = delete;
    VMObjectVanillaFunction& operator=(const VMObjectVanillaFunction&) = delete;
  private:
    const IFunctionSignature& signature;
    const IVMModule::Node& definition;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Function";
    }
  public:
    VMObjectVanillaFunction(IVM& vm, const Type& ftype, const IFunctionSignature& signature, const IVMModule::Node& definition, std::vector<VMCallCapture>&& captures)
      : VMObjectVanillaCaptures(vm, ftype, std::move(captures)),
        signature(signature),
        definition(definition)  {
    }
    virtual int print(Printer& printer) const override {
      printer.describe(*this->ftype);
      return 0;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      return execution.initiateFunctionCall(this->signature, this->definition, arguments, this);
    }
  };

  class VMObjectVanillaGeneratorIterator : public VMObjectBase {
    VMObjectVanillaGeneratorIterator(const VMObjectVanillaGeneratorIterator&) = delete;
    VMObjectVanillaGeneratorIterator& operator=(const VMObjectVanillaGeneratorIterator&) = delete;
  private:
    Type ftype;
    IVMRunner* runner; // set to null when iteration finished
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Generator iterator function";
    }
  public:
    VMObjectVanillaGeneratorIterator(IVM& vm, const Type& ftype, IVMRunner& runner)
      : VMObjectBase(vm),
        ftype(ftype),
        runner(&runner) {
      assert(this->ftype != nullptr);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      if (this->runner != nullptr) {
        visitor.visit(*this->runner);
      }
    }
    virtual int print(Printer& printer) const override {
      printer.describe(*this->ftype);
      return 0;
    }
    virtual Type vmRuntimeType() override {
      return this->ftype;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raisePrefixError(execution, " expects no arguments");
      }
      return this->next();
    }
    virtual HardValue vmIterate(IVMExecution& execution) override {
      return execution.createHardValueObject(HardObject{ this });
    }
  private:
    enum class FetchOutcome { Yield, Break, Continue, Error };
    FetchOutcome fetch(HardValue& retval) {
      // TODO better locking for thread safety
      HardPtr lock{ this->runner };
      if (lock == nullptr) {
        // Already finished
        return FetchOutcome::Break;
      }
      retval = lock->yield();
      auto flags = retval->getPrimitiveFlag();
      assert(Bits::hasNoneSet(flags, ValueFlags::Return | ValueFlags::Yield));
      if (Bits::hasNoneSet(flags, ValueFlags::FlowControl)) {
        return FetchOutcome::Yield;
      }
      if (flags == ValueFlags::Break) {
        return FetchOutcome::Break;
      }
      if (flags == ValueFlags::Continue) {
        return FetchOutcome::Continue;
      }
      return FetchOutcome::Error;
    }
    HardValue next() {
      HardValue retval;
      auto outcome = this->fetch(retval);
      while (outcome == FetchOutcome::Continue) {
        // Loop past 'yield continue'
        outcome = this->fetch(retval);
      }
      if (outcome == FetchOutcome::Break) {
        return HardValue::Void;
      }
      return retval;
    }
  };

  class VMObjectBuiltinAssert : public VMObjectBase {
    VMObjectBuiltinAssert(const VMObjectBuiltinAssert&) = delete;
    VMObjectBuiltinAssert& operator=(const VMObjectBuiltinAssert&) = delete;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Builtin 'assert()'";
    }
  public:
    explicit VMObjectBuiltinAssert(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual int print(Printer& printer) const override {
      printer << "[builtin assert]";
      return 0;
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 1) {
        return this->raisePrefixError(execution, " expects exactly one argument");
      }
      HardValue value;
      Bool success = false;
      if (!arguments.getArgumentValueByIndex(0, value) || !value->getBool(success)) {
        return this->raisePrefixError(execution, " expects a 'bool' argument, but instead got ", describe(value.get()));
      }
      if (!success) {
        auto message = StringBuilder::concat(this->vm.getAllocator(), "Assertion failure");
        SourceRange source;
        if (arguments.getArgumentSourceByIndex(0, source)) {
          return execution.raiseRuntimeError(message, &source);
        }
        return execution.raiseRuntimeError(message, nullptr);
      }
      return HardValue::Void;
    }
  };

  class VMObjectBuiltinPrint : public VMObjectBase {
    VMObjectBuiltinPrint(const VMObjectBuiltinPrint&) = delete;
    VMObjectBuiltinPrint& operator=(const VMObjectBuiltinPrint&) = delete;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Builtin 'print()'";
    }
  public:
    explicit VMObjectBuiltinPrint(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual int print(Printer& printer) const override {
      printer << "[builtin print]";
      return 0;
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
        if (!arguments.getArgumentValueByIndex(i, value) || arguments.getArgumentNameByIndex(i, name)) {
          return this->raisePrefixError(execution, " expects unnamed arguments");
        }
        sb.add(value);
      }
      execution.log(ILogger::Source::User, ILogger::Severity::None, sb.build(this->vm.getAllocator()));
      return HardValue::Void;
    }
  };

  class VMObjectBuiltinExpando : public VMObjectBase {
    VMObjectBuiltinExpando(const VMObjectBuiltinExpando&) = delete;
    VMObjectBuiltinExpando& operator=(const VMObjectBuiltinExpando&) = delete;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Builtin 'expando()'";
    }
  public:
    explicit VMObjectBuiltinExpando(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual int print(Printer& printer) const override {
      printer << "[builtin expando]";
      return 0;
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raisePrefixError(execution, " expects no arguments");
      }
      auto instance = makeHardObject<VMObjectVanillaObject>(this->vm, Type::Object, Accessability::All);
      return execution.createHardValueObject(instance);
    }
  };

  class VMObjectBuiltinCollector : public VMObjectBase {
    VMObjectBuiltinCollector(const VMObjectBuiltinCollector&) = delete;
    VMObjectBuiltinCollector& operator=(const VMObjectBuiltinCollector&) = delete;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Builtin 'collector()'";
    }
  public:
    explicit VMObjectBuiltinCollector(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual int print(Printer& printer) const override {
      printer << "[builtin collector]";
      return 0;
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raisePrefixError(execution, " expects no arguments");
      }
      auto collected = this->vm.getBasket().collect();
      return execution.createHardValueInt(Int(collected));
    }
  };

  class VMObjectBuiltinSymtable : public VMObjectBase {
    VMObjectBuiltinSymtable(const VMObjectBuiltinSymtable&) = delete;
    VMObjectBuiltinSymtable& operator=(const VMObjectBuiltinSymtable&) = delete;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Builtin 'symtable()'";
    }
  public:
    explicit VMObjectBuiltinSymtable(IVM& vm)
      : VMObjectBase(vm) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual int print(Printer& printer) const override {
      printer << "[builtin symtable]";
      return 0;
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      if (arguments.getArgumentCount() != 0) {
        return this->raisePrefixError(execution, " expects no arguments");
      }
      return execution.debugSymtable();
    }
  };

  class VMObjectPointerBase : public VMObjectBase {
    VMObjectPointerBase(const VMObjectPointerBase&) = delete;
    VMObjectPointerBase& operator=(const VMObjectPointerBase&) = delete;
  protected:
    Modifiability modifiability;
    virtual void printPrefix(Printer& printer) const override {
      printer << "Pointer";
    }
  public:
    VMObjectPointerBase(IVM& vm, Modifiability modifiability)
      : VMObjectBase(vm),
        modifiability(modifiability) {
    }
    virtual int print(Printer& printer) const override {
      // TODO
      printer << "[pointer]";
      return 0;
    }
    virtual HardValue vmPointeeGet(IVMExecution& execution) override {
      if (Bits::hasNoneSet(this->modifiability, Modifiability::Read)) {
        return this->raisePrefixError(execution, "Pointer does not permit reading values");
      }
      return this->pointeeGet(execution);
    }
    virtual HardValue vmPointeeSet(IVMExecution& execution, const HardValue& value) {
      if (Bits::hasNoneSet(this->modifiability, Modifiability::Write)) {
        return this->raiseRuntimeError(execution, "Pointer does not permit assigning values");
      }
      return this->pointeeSet(execution, value);
    }
    virtual HardValue vmPointeeMut(IVMExecution& execution, ValueMutationOp mutation, const HardValue& value) {
      if (Bits::hasNoneSet(this->modifiability, Modifiability::Write)) {
        return this->raisePrefixError(execution, "Pointer does not permit modifying values");
      }
      return this->pointeeMut(execution, mutation, value);
    }
  protected:
    virtual HardValue pointeeGet(IVMExecution& execution) = 0;
    virtual HardValue pointeeSet(IVMExecution& execution, const HardValue& value) = 0;
    virtual HardValue pointeeMut(IVMExecution& execution, ValueMutationOp mutation, const HardValue& value) = 0;
  };

  class VMObjectPointerToAlias : public VMObjectPointerBase {
    VMObjectPointerToAlias(const VMObjectPointerToAlias&) = delete;
    VMObjectPointerToAlias& operator=(const VMObjectPointerToAlias&) = delete;
  private:
    IValue& alias;
  public:
    VMObjectPointerToAlias(IVM& vm, IValue& alias, Modifiability modifiability)
      : VMObjectPointerBase(vm, modifiability),
        alias(alias) {
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      visitor.visit(this->alias);
    }
    virtual Type vmRuntimeType() override {
      return this->vm.getTypeForge().forgePointerType(this->alias.getRuntimeType(), this->modifiability);
    }
  protected:
    virtual HardValue pointeeGet(IVMExecution&) override {
      return HardValue(this->alias);
    }
    virtual HardValue pointeeSet(IVMExecution& execution, const HardValue& value) {
      if (this->alias.set(value.get())) {
        return this->raiseRuntimeError(execution, "Cannot assign value via pointer");
      }
      return HardValue::Void;
    }
    virtual HardValue pointeeMut(IVMExecution&, ValueMutationOp mutation, const HardValue& value) {
      return this->alias.mutate(mutation, value.get());
    }
  };

  class VMObjectPointerToIndex : public VMObjectPointerBase {
    VMObjectPointerToIndex(const VMObjectPointerToIndex&) = delete;
    VMObjectPointerToIndex& operator=(const VMObjectPointerToIndex&) = delete;
  private:
    SoftObject instance;
    SoftValue index;
    Type pointerType;
  public:
    VMObjectPointerToIndex(IVM& vm, const HardObject& instance, const HardValue& index, Modifiability modifiability, const Type& pointerType)
      : VMObjectPointerBase(vm, modifiability),
        instance(vm, instance),
        index(vm),
      pointerType(pointerType) {
      this->vm.setSoftValue(this->index, index); // cloned
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      this->instance.visit(visitor);
      this->index.visit(visitor);
    }
    virtual Type vmRuntimeType() override {
      return this->pointerType;
    }
  protected:
    virtual HardValue pointeeGet(IVMExecution& execution) override {
      return this->instance->vmIndexGet(execution, execution.getSoftValue(this->index));
    }
    virtual HardValue pointeeSet(IVMExecution& execution, const HardValue& value) {
      return this->instance->vmIndexSet(execution, execution.getSoftValue(this->index), value);
    }
    virtual HardValue pointeeMut(IVMExecution& execution, ValueMutationOp mutation, const HardValue& value) {
      return this->instance->vmIndexMut(execution, execution.getSoftValue(this->index), mutation, value);
    }
  };

  class VMObjectPointerToProperty : public VMObjectPointerBase {
    VMObjectPointerToProperty(const VMObjectPointerToProperty&) = delete;
    VMObjectPointerToProperty& operator=(const VMObjectPointerToProperty&) = delete;
  private:
    SoftObject instance;
    SoftValue property;
    Type pointerType;
  public:
    VMObjectPointerToProperty(IVM& vm, const HardObject& instance, const HardValue& property, Modifiability modifiability, const Type& pointerType)
      : VMObjectPointerBase(vm, modifiability),
        instance(vm, instance),
        property(vm),
        pointerType(pointerType) {
      this->vm.setSoftValue(this->property, property); // cloned
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      this->instance.visit(visitor);
      this->property.visit(visitor);
    }
    virtual Type vmRuntimeType() override {
      return this->pointerType;
    }
  protected:
    virtual HardValue pointeeGet(IVMExecution& execution) override {
      return this->instance->vmPropertyGet(execution, execution.getSoftValue(this->property));
    }
    virtual HardValue pointeeSet(IVMExecution& execution, const HardValue& value) {
      return this->instance->vmPropertySet(execution, execution.getSoftValue(this->property), value);
    }
    virtual HardValue pointeeMut(IVMExecution& execution, ValueMutationOp mutation, const HardValue& value) {
      return this->instance->vmPropertyMut(execution, execution.getSoftValue(this->property), mutation, value);
    }
  };

  class VMStringProxyBase : public VMObjectBase {
    VMStringProxyBase(const VMStringProxyBase&) = delete;
    VMStringProxyBase& operator=(const VMStringProxyBase&) = delete;
  protected:
    String instance;
    const char* proxy;
    virtual void printPrefix(Printer& printer) const override {
      printer << "String property '" << this->proxy << "()'";
    }
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
    virtual int print(Printer& printer) const override {
      printer << "[string proxy " << this->proxy << "]"; // TODO
      return 0;
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
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentValueByIndex(0, argument)) {
        return this->raisePrefixError(execution, " expects one argument");
      }
      String other;
      if (!argument->getString(other)) {
        return this->raisePrefixError(execution, " expects its argument to be a 'string', but instead got ", describe(argument.get()));
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
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentValueByIndex(0, argument)) {
        return this->raisePrefixError(execution, " expects one argument");
      }
      String needle;
      if (!argument->getString(needle)) {
        return this->raisePrefixError(execution, " expects its argument to be a 'string', but instead got ", describe(argument.get()));
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
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentValueByIndex(0, argument)) {
        return this->raisePrefixError(execution, " expects one argument");
      }
      String needle;
      if (!argument->getString(needle)) {
        return this->raisePrefixError(execution, " expects its argument to be a 'string', but instead got ", describe(argument.get()));
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
        return this->raisePrefixError(execution, " expects no arguments");
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
        return this->raisePrefixError(execution, " expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      String needle;
      if (!arguments.getArgumentValueByIndex(0, argument) || !argument->getString(needle)) {
        return this->raisePrefixError(execution, " expects its first argument to be a 'string', but instead got ", describe(argument.get()));
      }
      Int retval;
      if (arguments.getArgumentValueByIndex(1, argument)) {
        Int fromIndex;
        if (!argument->getInt(fromIndex)) {
          return this->raisePrefixError(execution, " expects its optional second argument to be an 'int', but instead got ", describe(argument.get()));
        }
        if (fromIndex < 0) {
          return this->raisePrefixError(execution, " expects its optional second argument to be a non-negative integer, but instead got ", fromIndex);
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
      for (size_t index = 0; arguments.getArgumentValueByIndex(index, argument); ++index) {
        if (index > 0) {
          sb.add(this->instance);
        }
        sb.add(argument);
      }
      return execution.createHardValueString(sb.build(this->vm.getAllocator()));
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
        return this->raisePrefixError(execution, " expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      String needle;
      if (!arguments.getArgumentValueByIndex(0, argument) || !argument->getString(needle)) {
        return this->raisePrefixError(execution, " expects its first argument to be a 'string', but instead got ", describe(argument.get()));
      }
      Int retval;
      if (arguments.getArgumentValueByIndex(1, argument)) {
        Int fromIndex;
        if (!argument->getInt(fromIndex)) {
          return this->raisePrefixError(execution, " expects its optional second argument to be an 'int', but instead got ", describe(argument.get()));
        }
        if (fromIndex < 0) {
          return this->raisePrefixError(execution, " expects its optional second argument to be a non-negative integer, but instead got ", fromIndex);
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
        return this->raisePrefixError(execution, " expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      Int target;
      if (!arguments.getArgumentValueByIndex(0, argument) || !argument->getInt(target)) {
        return this->raisePrefixError(execution, " expects its first argument to be an 'int', but instead got ", describe(argument.get()));
      }
      if (target < 0) {
        return this->raisePrefixError(execution, " expects its first argument to be a non-negative integer, but instead got ", target);
      }
      if (arguments.getArgumentValueByIndex(1, argument)) {
        String padding;
        if (!argument->getString(padding)) {
          return this->raisePrefixError(execution, " expects its optional second argument to be a 'string', but instead got ", describe(argument.get()));
        }
        return execution.createHardValueString(this->instance.padLeft(this->vm.getAllocator(), size_t(target), padding));
      }
      return execution.createHardValueString(this->instance.padLeft(this->vm.getAllocator(), size_t(target)));
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
        return this->raisePrefixError(execution, " expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      Int target;
      if (!arguments.getArgumentValueByIndex(0, argument) || !argument->getInt(target)) {
        return this->raisePrefixError(execution, " expects its first argument to be an 'int', but instead got ", describe(argument.get()));
      }
      if (target < 0) {
        return this->raisePrefixError(execution, " expects its first argument to be a non-negative integer, but instead got ", target);
      }
      if (arguments.getArgumentValueByIndex(1, argument)) {
        String padding;
        if (!argument->getString(padding)) {
          return this->raisePrefixError(execution, " expects its optional second argument to be a 'string', but instead got ", describe(argument.get()));
        }
        return execution.createHardValueString(this->instance.padRight(this->vm.getAllocator(), size_t(target), padding));
      }
      return execution.createHardValueString(this->instance.padRight(this->vm.getAllocator(), size_t(target)));
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
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentValueByIndex(0, argument)) {
        return this->raisePrefixError(execution, " expects one argument");
      }
      Int count;
      if (!argument->getInt(count)) {
        return this->raisePrefixError(execution, " expects its argument to be an 'int', but instead got ", describe(argument.get()));
      }
      if (count < 0) {
        return this->raisePrefixError(execution, " expects its argument to be a non-negative integer, but instead got ", count);
      }
      return execution.createHardValueString(this->instance.repeat(this->vm.getAllocator(), size_t(count)));
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
        return this->raisePrefixError(execution, " expects two or three arguments, but instead got ", count);
      }
      HardValue argument;
      String needle;
      if (!arguments.getArgumentValueByIndex(0, argument) || !argument->getString(needle)) {
        return this->raisePrefixError(execution, " expects its first argument to be a 'string', but instead got ", describe(argument.get()));
      }
      String replacement;
      if (!arguments.getArgumentValueByIndex(1, argument) || !argument->getString(replacement)) {
        return this->raisePrefixError(execution, " expects its second argument to be a 'string', but instead got ", describe(argument.get()));
      }
      Int occurrences = std::numeric_limits<Int>::max();
      if (arguments.getArgumentValueByIndex(2, argument)) {
        if (!argument->getInt(occurrences)) {
          return this->raisePrefixError(execution, " expects its optional third argument to be an 'int', but instead got ", describe(argument.get()));
        }
      }
      return execution.createHardValueString(this->instance.replace(this->vm.getAllocator(), needle, replacement, occurrences));
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
        return this->raisePrefixError(execution, " expects one or two arguments, but instead got ", count);
      }
      HardValue argument;
      Int begin;
      if (!arguments.getArgumentValueByIndex(0, argument) || !argument->getInt(begin)) {
        return this->raisePrefixError(execution, " expects its first argument to be an 'int', but instead got ", describe(argument.get()));
      }
      Int end = std::numeric_limits<Int>::max();
      if (arguments.getArgumentValueByIndex(1, argument)) {
        if (!argument->getInt(end)) {
          return this->raisePrefixError(execution, " expects its optional second argument to be an 'int', but instead got ", describe(argument.get()));
        }
      }
      return execution.createHardValueString(this->instance.slice(this->vm.getAllocator(), begin, end));
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
      if ((arguments.getArgumentCount() != 1) || !arguments.getArgumentValueByIndex(0, argument)) {
        return this->raisePrefixError(execution, " expects one argument");
      }
      String needle;
      if (!argument->getString(needle)) {
        return this->raisePrefixError(execution, " expects its argument to be a 'string', but instead got ", describe(argument.get()));
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
        return this->raisePrefixError(execution, " expects no arguments");
      }
      return execution.createHardValueString(this->instance);
    }
  };

  template<typename T>
  class VMManifestationMemberHandler : public VMObjectBase {
    VMManifestationMemberHandler(const VMManifestationMemberHandler&) = delete;
    VMManifestationMemberHandler& operator=(const VMManifestationMemberHandler&) = delete;
  public:
    typedef HardValue (T::*Handler)(IVMExecution& execution, const ICallArguments& arguments);
  private:
    T& manifestation; // manifestation lifetime is directly controlled by the VM instance
    const char* name;
    Handler handler;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "'" << this->manifestation.getManifestationName() << '.' << this->name << "' member handler";
    }
  public:
    VMManifestationMemberHandler(IVM& vm, T& manifestation, const char* name, Handler handler)
      : VMObjectBase(vm),
        manifestation(manifestation),
        name(name),
        handler(handler) {
      assert(this->name != nullptr);
      assert(this->handler != nullptr);
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // Nothing to visit
    }
    virtual int print(Printer& printer) const override {
      printer << '[' << this->manifestation.getManifestationName() << '.' << this->name << ']';
      return 0;
    }
    virtual Type vmRuntimeType() override {
      // TODO
      return Type::Object;
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      assert(this->handler != nullptr);
      return (this->manifestation.*(this->handler))(execution, arguments);
    }
  };

  class VMManifestionBase : public VMObjectBase {
    VMManifestionBase(const VMManifestionBase&) = delete;
    VMManifestionBase& operator=(const VMManifestionBase&) = delete;
  protected:
    Type type;
    virtual void printPrefix(Printer& printer) const override {
      if (this->type->isPrimitive()) {
        printer << "Type ";
      }
      printer << "'" << this->getManifestationName() << "'";
    }
  public:
    VMManifestionBase(IVM& vm, const Type& typeParent, const char* typeName)
      : VMObjectBase(vm),
      type(getNamedType(vm, typeParent, typeName)) {
      assert(this->type.validate());
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // No soft links
    }
    virtual int print(Printer& printer) const override {
      printer << '[' << this->getManifestationName() << ']';
      return 0;
    }
    virtual Type vmRuntimeType() override {
      return this->type;
    }
    virtual const char* getManifestationName() const = 0;
  private:
    static Type getNamedType(IVM& vm, const Type& typeParent, const char* typeName) {
      return vm.getTypeForge().getNamedType(typeParent, vm.createString(typeName));
    }
  };

  template<typename T>
  class VMManifestionWithProperties : public VMManifestionBase {
    VMManifestionWithProperties(const VMManifestionWithProperties&) = delete;
    VMManifestionWithProperties& operator=(const VMManifestionWithProperties&) = delete;
  protected:
    std::map<String, SoftValue> properties;
  public:
    VMManifestionWithProperties(IVM& vm, const Type& typeParent, const char* typeName)
      : VMManifestionBase(vm, typeParent, typeName) {
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      for (const auto& property : this->properties) {
        property.second.visit(visitor);
      }
    }
    virtual HardValue vmPropertyGet(IVMExecution& execution, const HardValue& property) override {
      String name;
      if (!property->getString(name)) {
        return this->raiseRuntimeError(execution, "Expected '", this->getManifestationName(), "' static property name to be a 'string', but instead got ", describe(property.get()));
      }
      auto found = this->properties.find(name);
      if (found == this->properties.end()) {
        return this->raisePrefixError(execution, " does not have a static property named '", name, "'");
      }
      return this->vm.getSoftValue(found->second);
    }
    virtual HardValue vmPropertySet(IVMExecution& execution, const HardValue&, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support static property modification (set)");
    }
    virtual HardValue vmPropertyMut(IVMExecution& execution, const HardValue&, ValueMutationOp, const HardValue&) override {
      return this->raisePrefixError(execution, " does not support static property modification (mut)");
    }
  protected:
    typedef HardValue (T::*MemberHandler)(IVMExecution& execution, const ICallArguments& arguments);
    void addMemberHandler(const char* pname, MemberHandler phandler) {
      auto instance = makeHardObject<VMManifestationMemberHandler<T>>(this->vm, *static_cast<T*>(this), pname, phandler);
      this->addProperty(pname, this->vm.createHardValueObject(instance));
    }
    void addProperty(const char* pname, const HardValue& pvalue) {
      this->properties.emplace(std::piecewise_construct, std::forward_as_tuple(this->vm.createString(pname)), std::forward_as_tuple(this->vm, pvalue));
    }
  };

  class VMManifestionType : public VMManifestionWithProperties<VMManifestionType> {
    VMManifestionType(const VMManifestionType&) = delete;
    VMManifestionType& operator=(const VMManifestionType&) = delete;
  public:
    explicit VMManifestionType(IVM& vm)
      : VMManifestionWithProperties(vm, Type::Type_, "Type") {
      this->addMemberHandler("of", &VMManifestionType::vmCallTypeOf);
    }
    HardValue vmCallTypeOf(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 1) {
        return this->raiseRuntimeError(execution, "'type.of()' expects exactly one argument");
      }
      HardValue value;
      if (arguments.getArgumentValueByIndex(0, value)) {
        Print::Options options{};
        options.names = false;
        StringBuilder sb{ options };
        sb << value->getRuntimeType();
        return this->vm.createHardValueString(sb.build(this->vm.getAllocator()));
      }
      return this->vm.createHardValue("unknown");
    }
    virtual const char* getManifestationName() const override {
      return "type";
    }
  };

  class VMManifestionVoid : public VMManifestionBase {
    VMManifestionVoid(const VMManifestionVoid&) = delete;
    VMManifestionVoid& operator=(const VMManifestionVoid&) = delete;
  public:
    explicit VMManifestionVoid(IVM& vm)
      : VMManifestionBase(vm, Type::Void, "Void") {
    }
    virtual HardValue vmCall(IVMExecution&, const ICallArguments&) override {
      // Just discard the values
      return HardValue::Void;
    }
    virtual const char* getManifestationName() const override {
      return "void";
    }
  };

  class VMManifestionBool : public VMManifestionBase {
    VMManifestionBool(const VMManifestionBool&) = delete;
    VMManifestionBool& operator=(const VMManifestionBool&) = delete;
  public:
    explicit VMManifestionBool(IVM& vm)
      : VMManifestionBase(vm, Type::Bool, "Bool") {
    }
    virtual const char* getManifestationName() const override {
      return "bool";
    }
  };

  class VMManifestionInt : public VMManifestionBase {
    VMManifestionInt(const VMManifestionInt&) = delete;
    VMManifestionInt& operator=(const VMManifestionInt&) = delete;
  public:
    explicit VMManifestionInt(IVM& vm)
      : VMManifestionBase(vm, Type::Int, "Int") {
    }
    virtual const char* getManifestationName() const override {
      return "int";
    }
  };

  class VMManifestionFloat : public VMManifestionBase {
    VMManifestionFloat(const VMManifestionFloat&) = delete;
    VMManifestionFloat& operator=(const VMManifestionFloat&) = delete;
  public:
    explicit VMManifestionFloat(IVM& vm)
      : VMManifestionBase(vm, Type::Float, "Float") {
    }
    virtual const char* getManifestationName() const override {
      return "float";
    }
  };

  class VMManifestionString : public VMManifestionBase {
    VMManifestionString(const VMManifestionString&) = delete;
    VMManifestionString& operator=(const VMManifestionString&) = delete;
  public:
    explicit VMManifestionString(IVM& vm)
      : VMManifestionBase(vm, Type::String, "String") {
      auto& forge = vm.getTypeForge();
      this->type = forge.forgeShapeType(forge.forgeStringShape());
    }
    virtual HardValue vmCall(IVMExecution& execution, const ICallArguments& arguments) override {
      // Constructor calls
      StringBuilder sb;
      size_t count = arguments.getArgumentCount();
      for (size_t index = 0; index < count; ++index) {
        HardValue value;
        if (arguments.getArgumentValueByIndex(index, value)) {
          sb.add(value);
        }
      }
      return execution.createHardValueString(sb.build(this->vm.getAllocator()));
    }
    virtual const char* getManifestationName() const override {
      return "string";
    }
  };

  class VMManifestionObjectIndex : public VMManifestionWithProperties<VMManifestionObjectIndex> {
    VMManifestionObjectIndex(const VMManifestionObjectIndex&) = delete;
    VMManifestionObjectIndex& operator=(const VMManifestionObjectIndex&) = delete;
  public:
    explicit VMManifestionObjectIndex(IVM& vm)
      : VMManifestionWithProperties(vm, Type::Object, "Index") {
      this->addMemberHandler("get", &VMManifestionObjectIndex::vmCallObjectGet);
      this->addMemberHandler("set", &VMManifestionObjectIndex::vmCallObjectSet);
      this->addMemberHandler("mut", &VMManifestionObjectIndex::vmCallObjectMut);
      this->addMemberHandler("ref", &VMManifestionObjectIndex::vmCallObjectRef);
      this->addMemberHandler("del", &VMManifestionObjectIndex::vmCallObjectDel);
    }
    HardValue vmCallObjectGet(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 2) {
        return this->raiseRuntimeError(execution, "'object.index.get()' expects exactly two arguments");
      }
      HardObject instance;
      auto index = this->checkArguments(execution, arguments, "get", instance);
      if (index.hasFlowControl()) {
        return index;
      }
      return instance->vmIndexGet(execution, index);
    }
    HardValue vmCallObjectSet(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 3) {
        return this->raiseRuntimeError(execution, "'object.index.set()' expects exactly three arguments");
      }
      HardObject instance;
      auto index = this->checkArguments(execution, arguments, "set", instance);
      if (index.hasFlowControl()) {
        return index;
      }
      HardValue value;
      if (!arguments.getArgumentValueByIndex(2, value)) {
        return this->raiseRuntimeError(execution, "'object.index.set()' expects its third argument to be a value");
      }
      return instance->vmIndexSet(execution, index, value);
    }
    HardValue vmCallObjectMut(IVMExecution& execution, const ICallArguments& arguments) {
      if ((arguments.getArgumentCount() < 3) || (arguments.getArgumentCount() > 4)) {
        return this->raiseRuntimeError(execution, "'object.index.mut()' expects three or four arguments");
      }
      HardObject instance;
      auto index = this->checkArguments(execution, arguments, "mut", instance);
      if (index.hasFlowControl()) {
        return index;
      }
      HardValue value;
      ValueMutationOp mutation;
      size_t operands;
      if (!arguments.getArgumentValueByIndex(2, value) || !parseMutationOp(value, mutation, operands)) {
        return this->raiseRuntimeError(execution, "'object.index.mut()' expects its third argument to be a mutation string");
      }
      if (operands == 1) {
        // Decrement/increment don't take a value
        if (arguments.getArgumentCount() != 3) {
          return this->raiseRuntimeError(execution, "'object.index.mut()' expects three arguments for mutation '", mutation, "'");
        }
        return instance->vmIndexMut(execution, index, mutation, HardValue::Void);
      }
      if (arguments.getArgumentCount() != 4) {
        return this->raiseRuntimeError(execution, "'object.index.mut()' expects four arguments for mutation '", mutation, "'");
      }
      if (!arguments.getArgumentValueByIndex(3, value)) {
        return this->raiseRuntimeError(execution, "'object.index.mut()' expects its fourth argument to be a value");
      }
      return instance->vmIndexMut(execution, index, mutation, value);
    }
    HardValue vmCallObjectRef(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 2) {
        return this->raiseRuntimeError(execution, "'object.index.ref()' expects exactly two arguments");
      }
      HardObject instance;
      auto index = this->checkArguments(execution, arguments, "ref", instance);
      if (index.hasFlowControl()) {
        return index;
      }
      return instance->vmIndexRef(execution, index);
    }
    HardValue vmCallObjectDel(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 2) {
        return this->raiseRuntimeError(execution, "'object.index.del()' expects exactly two arguments");
      }
      HardObject instance;
      auto index = this->checkArguments(execution, arguments, "del", instance);
      if (index.hasFlowControl()) {
        return index;
      }
      return instance->vmIndexDel(execution, index);
    }
    virtual const char* getManifestationName() const override {
      return "object.index";
    }
  private:
    HardValue checkArguments(IVMExecution& execution, const ICallArguments& arguments, const char* member, HardObject& instance) {
      HardValue argument;
      if (!arguments.getArgumentValueByIndex(0, argument) || !argument->getHardObject(instance)) {
        return this->raiseRuntimeError(execution, "'object.index.", member, "()' expects its first argument to be an object instance");
      }
      if (!arguments.getArgumentValueByIndex(1, argument)) {
        return this->raiseRuntimeError(execution, "'object.index.", member, "()' expects its second argument to be an index");
      }
      return argument;
    }
  };

  class VMManifestionObjectProperty : public VMManifestionWithProperties<VMManifestionObjectProperty> {
    VMManifestionObjectProperty(const VMManifestionObjectProperty&) = delete;
    VMManifestionObjectProperty& operator=(const VMManifestionObjectProperty&) = delete;
  public:
    explicit VMManifestionObjectProperty(IVM& vm)
      : VMManifestionWithProperties(vm, Type::Object, "Property") {
      this->addMemberHandler("get", &VMManifestionObjectProperty::vmCallObjectGet);
      this->addMemberHandler("set", &VMManifestionObjectProperty::vmCallObjectSet);
      this->addMemberHandler("mut", &VMManifestionObjectProperty::vmCallObjectMut);
      this->addMemberHandler("ref", &VMManifestionObjectProperty::vmCallObjectRef);
      this->addMemberHandler("del", &VMManifestionObjectProperty::vmCallObjectDel);
    }
    HardValue vmCallObjectGet(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 2) {
        return this->raiseRuntimeError(execution, "'object.property.get()' expects exactly two arguments");
      }
      HardObject instance;
      auto property = this->checkArguments(execution, arguments, "get", instance);
      if (property.hasFlowControl()) {
        return property;
      }
      return instance->vmPropertyGet(execution, property);
    }
    HardValue vmCallObjectSet(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 3) {
        return this->raiseRuntimeError(execution, "'object.property.set()' expects exactly three arguments");
      }
      HardObject instance;
      auto property = this->checkArguments(execution, arguments, "set", instance);
      if (property.hasFlowControl()) {
        return property;
      }
      HardValue value;
      if (!arguments.getArgumentValueByIndex(2, value)) {
        return this->raiseRuntimeError(execution, "'object.property.set()' expects its third argument to be a value");
      }
      return instance->vmPropertySet(execution, property, value);
    }
    HardValue vmCallObjectMut(IVMExecution& execution, const ICallArguments& arguments) {
      if ((arguments.getArgumentCount() < 3) || (arguments.getArgumentCount() > 4)) {
        return this->raiseRuntimeError(execution, "'object.property.mut()' expects three or four arguments");
      }
      HardObject instance;
      auto property = this->checkArguments(execution, arguments, "mut", instance);
      if (property.hasFlowControl()) {
        return property;
      }
      HardValue value;
      ValueMutationOp mutation;
      size_t operands;
      if (!arguments.getArgumentValueByIndex(2, value) || !parseMutationOp(value, mutation, operands)) {
        return this->raiseRuntimeError(execution, "'object.property.mut()' expects its third argument to be a mutation string");
      }
      if (operands == 1) {
        // Decrement/increment don't take a value
        if (arguments.getArgumentCount() != 3) {
          return this->raiseRuntimeError(execution, "'object.property.mut()' expects three arguments for mutation '", mutation, "'");
        }
        return instance->vmPropertyMut(execution, property, mutation, HardValue::Void);
      }
      if (arguments.getArgumentCount() != 4) {
        return this->raiseRuntimeError(execution, "'object.property.mut()' expects four arguments for mutation '", mutation, "'");
      }
      if (!arguments.getArgumentValueByIndex(3, value)) {
        return this->raiseRuntimeError(execution, "'object.property.mut()' expects its fourth argument to be a value");
      }
      return instance->vmPropertyMut(execution, property, mutation, value);
    }
    HardValue vmCallObjectRef(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 2) {
        return this->raiseRuntimeError(execution, "'object.property.ref()' expects exactly two arguments");
      }
      HardObject instance;
      auto property = this->checkArguments(execution, arguments, "ref", instance);
      if (property.hasFlowControl()) {
        return property;
      }
      return instance->vmPropertyRef(execution, property);
    }
    HardValue vmCallObjectDel(IVMExecution& execution, const ICallArguments& arguments) {
      if (arguments.getArgumentCount() != 2) {
        return this->raiseRuntimeError(execution, "'object.property.del()' expects exactly two arguments");
      }
      HardObject instance;
      auto property = this->checkArguments(execution, arguments, "del", instance);
      if (property.hasFlowControl()) {
        return property;
      }
      return instance->vmPropertyDel(execution, property);
    }
    virtual const char* getManifestationName() const override {
      return "object.property";
    }
  private:
    HardValue checkArguments(IVMExecution& execution, const ICallArguments& arguments, const char* member, HardObject& instance) {
      HardValue argument;
      if (!arguments.getArgumentValueByIndex(0, argument) || !argument->getHardObject(instance)) {
        return this->raiseRuntimeError(execution, "'object.property.", member, "()' expects its first argument to be an object instance");
      }
      String name;
      if (!arguments.getArgumentValueByIndex(1, argument) || !argument->getString(name)) {
        return this->raiseRuntimeError(execution, "'object.property.", member, "()' expects its second argument to be a property name");
      }
      return argument;
    }
  };

  class VMManifestionObject : public VMManifestionWithProperties<VMManifestionObject> {
    VMManifestionObject(const VMManifestionObject&) = delete;
    VMManifestionObject& operator=(const VMManifestionObject&) = delete;
  public:
    explicit VMManifestionObject(IVM& vm)
      : VMManifestionWithProperties(vm, Type::Object, "Object") {
      this->addProperty("index", this->createValueIndex());
      this->addProperty("property", this->createValueProperty());
    }
    virtual const char* getManifestationName() const override {
      return "object";
    }
  private:
    HardValue createValueIndex() {
      auto instance = makeHardObject<VMManifestionObjectIndex>(this->vm);
      return this->vm.createHardValueObject(instance);
    }
    HardValue createValueProperty() {
      auto instance = makeHardObject<VMManifestionObjectProperty>(this->vm);
      return this->vm.createHardValueObject(instance);
    }
  };

  class VMManifestionAny : public VMManifestionBase {
    VMManifestionAny(const VMManifestionAny&) = delete;
    VMManifestionAny& operator=(const VMManifestionAny&) = delete;
  public:
    explicit VMManifestionAny(IVM& vm)
      : VMManifestionBase(vm, Type::Any, "Any") {
    }
    virtual const char* getManifestationName() const override {
      return "any";
    }
  };

  class ObjectBuilderInstance : public VMObjectVanillaObject {
    ObjectBuilderInstance(const ObjectBuilderInstance&) = delete;
    ObjectBuilderInstance& operator=(const ObjectBuilderInstance&) = delete;
  public:
    ObjectBuilderInstance(IVM& vm, const Type& runtimeType, Accessability accessability)
      : VMObjectVanillaObject(vm, runtimeType, accessability) {
    }
    void withProperty(const HardValue& pkey, const Type& ptype, const HardValue& pvalue, Accessability paccessability) {
      this->propertyEmplace(pkey, ptype, &pvalue, paccessability);
    }
    template<typename T>
    void withProperty(const char* pname, T pvalue, Accessability paccessability = Accessability::Get) {
      this->withProperty(this->vm.createHardValue(pname), nullptr, this->vm.createHardValue(pvalue), paccessability);
    }
  };

  class ObjectBuilderRuntimeError : public ObjectBuilderInstance {
    ObjectBuilderRuntimeError(const ObjectBuilderRuntimeError&) = delete;
    ObjectBuilderRuntimeError& operator=(const ObjectBuilderRuntimeError&) = delete;
  private:
    String message;
    HardPtr<IVMCallStack> callstack;
  protected:
    virtual void printPrefix(Printer& printer) const override {
      printer << "Runtime error";
    }
  public:
    ObjectBuilderRuntimeError(IVM& vm, const String& message, const HardPtr<IVMCallStack>& callstack)
      : ObjectBuilderInstance(vm, Type::Object, Accessability::Get),
        message(message),
        callstack(callstack) {
    }
    virtual int print(Printer& printer) const override {
      if (this->callstack != nullptr) {
        this->callstack->print(printer);
      }
      printer << this->message;
      return 0;
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
    virtual void addProperty(const HardValue& property, const Type& type, const HardValue& value, Accessability accessability) override {
      assert(this->instance != nullptr);
      this->instance->withProperty(property, type, value, accessability);
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
  auto iterator = makeHardObject<VMObjectVanillaArrayIterator>(this->vm, *this, lock);
  return execution.createHardValueObject(iterator);
}

egg::ovum::HardValue VMObjectVanillaObject::vmIterate(IVMExecution& execution) {
  VMObjectVanillaMutex::ReadLock lock{ this->mutex };
  auto iterator = makeHardObject<VMObjectVanillaObjectIterator>(this->vm, *this, lock);
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

egg::ovum::HardObject egg::ovum::ObjectFactory::createBuiltinSymtable(IVM& vm) {
  return makeHardObject<VMObjectBuiltinSymtable>(vm);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createVanillaArray(IVM& vm, const Type& elementType, Accessability accessability) {
  auto containerType = vm.getTypeForge().forgeArrayType(elementType, accessability);
  return makeHardObject<VMObjectVanillaArray>(vm, containerType, elementType, accessability);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createVanillaObject(IVM& vm, const Type& runtimeType, Accessability accessability) {
  return makeHardObject<VMObjectVanillaObject>(vm, runtimeType, accessability);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createVanillaKeyValue(IVM& vm, const HardValue& key, const HardValue& value, Accessability accessability) {
  return makeHardObject<VMObjectVanillaKeyValue>(vm, key, value, accessability);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createVanillaManifestation(IVM& vm, const Type& infratype, const Type& metatype) {
  return makeHardObject<VMObjectVanillaManifestation>(vm, infratype, metatype);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createPointerToValue(IVM& vm, const HardValue& value) {
  // We can only read from a pointer-to-value
  return makeHardObject<VMObjectPointerToAlias>(vm, vm.createSoftAlias(value), Modifiability::Read);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createPointerToAlias(IVM& vm, IValue& alias, Modifiability modifiability) {
  return makeHardObject<VMObjectPointerToAlias>(vm, alias, modifiability);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createPointerToIndex(IVM& vm, const HardObject& instance, const HardValue& index, Modifiability modifiability, const Type& pointerType) {
  return makeHardObject<VMObjectPointerToIndex>(vm, instance, index, modifiability, pointerType);
}

egg::ovum::HardObject egg::ovum::ObjectFactory::createPointerToProperty(IVM& vm, const HardObject& instance, const HardValue& property, Modifiability modifiability, const Type& pointerType) {
  return makeHardObject<VMObjectPointerToProperty>(vm, instance, property, modifiability, pointerType);
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

egg::ovum::HardPtr<egg::ovum::IObjectBuilder> egg::ovum::ObjectFactory::createObjectBuilder(IVM& vm, const Type& containerType, Accessability accessability) {
  auto& allocator = vm.getAllocator();
  HardPtr instance{ allocator.makeRaw<ObjectBuilderInstance>(vm, containerType, accessability) };
  return HardPtr{ allocator.makeRaw<ObjectBuilder>(vm, std::move(instance)) };
}

egg::ovum::HardPtr<egg::ovum::IObjectBuilder> egg::ovum::ObjectFactory::createRuntimeErrorBuilder(IVM& vm, const String& message, const HardPtr<IVMCallStack>& callstack) {
  auto& allocator = vm.getAllocator();
  HardPtr instance{ allocator.makeRaw<ObjectBuilderRuntimeError>(vm, message, callstack) };
  instance->withProperty("message", message);
  if (callstack != nullptr) {
    auto resource = callstack->getResource();
    if (!resource.empty()) {
      instance->withProperty("resource", resource);
    }
    auto* range = callstack->getSourceRange();
    if (range != nullptr) {
      if ((range->begin.line != 0) || (range->begin.column != 0)) {
        instance->withProperty("line", int(range->begin.line));
        if (range->begin.column != 0) {
          instance->withProperty("column", int(range->begin.column));
        }
      }
    }
    auto function = callstack->getFunction();
    if (!function.empty()) {
      instance->withProperty("function", function);
    }
  }
  return HardPtr{ allocator.makeRaw<ObjectBuilder>(vm, std::move(instance)) };
}

egg::ovum::HardObject egg::ovum::VMFactory::createFunction(IVM& vm, const Type& ftype, const IFunctionSignature& signature, const IVMModule::Node& definition, std::vector<VMCallCapture>&& captures) {
  return makeHardObject<VMObjectVanillaFunction>(vm, ftype, signature, definition, std::move(captures));
}

egg::ovum::HardObject egg::ovum::VMFactory::createGeneratorIterator(IVM& vm, const Type& ftype, IVMRunner& runner) {
  return makeHardObject<VMObjectVanillaGeneratorIterator>(vm, ftype, runner);
}
