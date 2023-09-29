#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  template<typename T, typename... ARGS>
  HardValue makeHardValue(IAllocator& allocator, ARGS&&... args) {
    return HardValue(*static_cast<IValue*>(allocator.makeRaw<T>(allocator, std::forward<ARGS>(args)...)));
  }

  template<ValueFlags FLAGS>
  class ValueImmutable : public SoftReferenceCountedNone<IValue> {
    ValueImmutable(const ValueImmutable&) = delete;
    ValueImmutable& operator=(const ValueImmutable&) = delete;
  public:
    ValueImmutable() {}
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // Nothing to visit
    }
    virtual bool getVoid() const override {
      return false;
    }
    virtual bool getNull() const override {
      return false;
    }
    virtual bool getBool(Bool&) const override {
      return false;
    }
    virtual bool getInt(Int&) const override {
      return false;
    }
    virtual bool getFloat(Float&) const override {
      return false;
    }
    virtual bool getString(String&) const override {
      return false;
    }
    virtual bool getHardObject(HardObject&) const override {
      return false;
    }
    virtual bool getInner(HardValue&) const override {
      return false;
    }
    virtual ValueFlags getFlags() const override {
      return FLAGS;
    }
    virtual Type getRuntimeType() const override {
      // We should NOT really be asked for type information here
      assert(false);
      return Type::Void;
    }
    virtual void print(Printer& printer) const override {
      printer.write(FLAGS);
    }
    virtual bool validate() const override {
      return true;
    }
    virtual bool set(const IValue&) override {
      // Cannot set an immutable instance
      return false;
    }
    virtual HardValue mutate(Mutation op, const IValue&) override {
      // There are very few valid mutation operations on immutables!
      if (op == Mutation::Noop) {
        return HardValue(*this);
      }
      return HardValue::Rethrow; // No allocator available
    }
    constexpr IValue& instance() const {
      return *const_cast<ValueImmutable*>(this);
    }
  };
  const ValueImmutable<ValueFlags::Break> theBreak;
  const ValueImmutable<ValueFlags::Continue> theContinue;
  const ValueImmutable<ValueFlags::Throw> theRethrow;

  class ValueVoid : public ValueImmutable<ValueFlags::Void> {
    ValueVoid(const ValueVoid&) = delete;
    ValueVoid& operator=(const ValueVoid&) = delete;
  public:
    ValueVoid() {}
    virtual bool getVoid() const override {
      return true;
    }
    virtual Type getRuntimeType() const override {
      return Type::Void;
    }
  };
  const ValueVoid theVoid;

  class ValueNull : public ValueImmutable<ValueFlags::Null> {
    ValueNull(const ValueNull&) = delete;
    ValueNull& operator=(const ValueNull&) = delete;
  public:
    ValueNull() {}
    virtual bool getNull() const override {
      return true;
    }
    virtual Type getRuntimeType() const override {
      return Type::Null;
    }
  };
  const ValueNull theNull;

  template<bool VALUE>
  class ValueBool : public ValueImmutable<ValueFlags::Bool> {
    ValueBool(const ValueBool&) = delete;
    ValueBool& operator=(const ValueBool&) = delete;
  public:
    ValueBool() {}
    virtual bool getBool(Bool& value) const override {
      value = VALUE;
      return true;
    }
    virtual Type getRuntimeType() const override {
      return Type::Bool;
    }
    virtual void print(Printer& printer) const override {
      printer.write(VALUE);
    }
  };
  const ValueBool<false> theFalse;
  const ValueBool<true> theTrue;

  class ValueMutable : public SoftReferenceCountedAllocator<IValue> {
    ValueMutable(const ValueMutable&) = delete;
    ValueMutable& operator=(const ValueMutable&) = delete;
  public:
    explicit ValueMutable(IAllocator& allocator)
      : SoftReferenceCountedAllocator<IValue>(allocator) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // By default, nothing to visit
    }
    virtual bool getVoid() const override {
      return false;
    }
    virtual bool getNull() const override {
      return false;
    }
    virtual bool getBool(Bool&) const override {
      return false;
    }
    virtual bool getInt(Int&) const override {
      return false;
    }
    virtual bool getFloat(Float&) const override {
      return false;
    }
    virtual bool getString(String&) const override {
      return false;
    }
    virtual bool getHardObject(HardObject&) const override {
      return false;
    }
    virtual bool getInner(HardValue&) const override {
      return false;
    }
    virtual bool validate() const override {
      // Assume all values are valid
      return this->atomic.get() >= 0;
    }
  protected:
    HardValue createException(const char* message) const {
      auto inner = ValueFactory::createStringASCII(this->allocator, message);
      return ValueFactory::createHardFlowControl(this->allocator, ValueFlags::Throw, inner);
    }
  };

  class ValueInt final : public ValueMutable {
    ValueInt(const ValueInt&) = delete;
    ValueInt& operator=(const ValueInt&) = delete;
  private:
    Atomic<Int> value;
  public:
    ValueInt(IAllocator& allocator, Int value)
      : ValueMutable(allocator), value(value) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Int;
    }
    virtual Type getRuntimeType() const override {
      return Type::Int;
    }
    virtual bool getInt(Int& result) const override {
      result = this->value.get();
      return true;
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->value.get());
    }
    virtual bool set(const IValue& rhs) override {
      Int rvalue;
      if (rhs.getInt(rvalue)) {
        this->value.exchange(rvalue);
        return true;
      }
      return false;
    }
    virtual HardValue mutate(Mutation op, const IValue& rhs) override {
      Int rvalue;
      switch (op) {
      case Mutation::Assign:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.exchange(rvalue));
        }
        return this->createException("TODO: Invalid right-hand value for mutation assignment to integer");
      case Mutation::Decrement:
        assert(rhs.getFlags() == ValueFlags::Void);
        return this->createBefore(this->value.decrement() + 1);
      case Mutation::Increment:
        assert(rhs.getFlags() == ValueFlags::Void);
        return this->createBefore(this->value.increment() - 1);
      case Mutation::Add:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.add(rvalue));
        }
        return this->createException("TODO: Invalid right-hand value for mutation addition to integer");
      case Mutation::Subtract:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.sub(rvalue));
        }
        return this->createException("TODO: Invalid right-hand value for mutation subtraction from integer");
      case Mutation::Multiply:
        return this->createException("TODO: Mutation '*' not supported for integers"); // WIBBLE
      case Mutation::Divide:
        return this->createException("TODO: Mutation '/' not supported for integers"); // WIBBLE
      case Mutation::Remainder:
        return this->createException("TODO: Mutation '%' not supported for integers"); // WIBBLE
      case Mutation::BitwiseAnd:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.bitwiseAnd(rvalue));
        }
        return this->createException("TODO: Invalid right-hand value for mutation bitwise-and from integer");
      case Mutation::BitwiseOr:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.bitwiseOr(rvalue));
        }
        return this->createException("TODO: Invalid right-hand value for mutation bitwise-or from integer");
      case Mutation::BitwiseXor:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.bitwiseXor(rvalue));
        }
        return this->createException("TODO: Invalid right-hand value for mutation bitwise-xor from integer");
      case Mutation::ShiftLeft:
        return this->createException("TODO: Mutation '<<' not supported for integers"); // WIBBLE
      case Mutation::ShiftRight:
        return this->createException("TODO: Mutation '>>' not supported for integers"); // WIBBLE
      case Mutation::ShiftRightUnsigned:
        return this->createException("TODO: Mutation '>>>' not supported for integers"); // WIBBLE
      case Mutation::Noop:
        assert(rhs.getFlags() == ValueFlags::Void);
        return this->createBefore(this->value.get());
      }
      // TODO
      return this->createException("TODO: Mutation operator unknown"); // WIBBLE
    }
  private:
    HardValue createBefore(Int before) {
      return ValueFactory::createInt(this->allocator, before);
    }
  };

  class ValueFloat final : public ValueMutable {
    ValueFloat(const ValueFloat&) = delete;
    ValueFloat& operator=(const ValueFloat&) = delete;
  private:
    Atomic<Float> value;
  public:
    ValueFloat(IAllocator& allocator, Float value)
      : ValueMutable(allocator), value(value) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Float;
    }
    virtual Type getRuntimeType() const override {
      return Type::Float;
    }
    virtual bool getFloat(Float& result) const override {
      result = this->value.get();
      return true;
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->value.get());
    }
    virtual bool set(const IValue& rhs) override {
      Float rvalue;
      if (rhs.getFloat(rvalue)) {
        this->value.exchange(rvalue);
        return true;
      }
      return false;
    }
    virtual HardValue mutate(Mutation op, const IValue& rhs) override {
      Float rvalue;
      switch (op) {
      case Mutation::Assign:
        if (rhs.getFloat(rvalue)) {
          return this->createBefore(this->value.exchange(rvalue));
        }
        return this->createException("TODO: Invalid right-hand value for mutation assignment to float"); // WIBBLE
      case Mutation::Decrement:
        assert(rhs.getFlags() == ValueFlags::Void);
        return this->createBefore(this->value.decrement() + 1);
      case Mutation::Increment:
        assert(rhs.getFlags() == ValueFlags::Void);
        return this->createBefore(this->value.increment() - 1);
      case Mutation::Add:
        return this->createException("TODO: Mutation '+' not supported for floats"); // WIBBLE
      case Mutation::Subtract:
        return this->createException("TODO: Mutation '-' not supported for floats"); // WIBBLE
      case Mutation::Multiply:
        return this->createException("TODO: Mutation '*' not supported for floats"); // WIBBLE
      case Mutation::Divide:
        return this->createException("TODO: Mutation '/' not supported for floats"); // WIBBLE
      case Mutation::Remainder:
        return this->createException("TODO: Mutation '%' not supported for floats"); // WIBBLE
      case Mutation::BitwiseAnd:
        return this->createException("TODO: Mutation '&' not supported for floats"); // WIBBLE
      case Mutation::BitwiseOr:
        return this->createException("TODO: Mutation '|' not supported for floats"); // WIBBLE
      case Mutation::BitwiseXor:
        return this->createException("TODO: Mutation '^' not supported for floats"); // WIBBLE
      case Mutation::ShiftLeft:
        return this->createException("TODO: Mutation '<<' not supported for floats"); // WIBBLE
      case Mutation::ShiftRight:
        return this->createException("TODO: Mutation '>>' not supported for floats"); // WIBBLE
      case Mutation::ShiftRightUnsigned:
        return this->createException("TODO: Mutation '>>>' not supported for floats"); // WIBBLE
      case Mutation::Noop:
        assert(rhs.getFlags() == ValueFlags::Void);
        return this->createBefore(this->value.get());
      }
      // TODO
      return this->createException("TODO: Mutation operator unknown"); // WIBBLE
    }
  private:
    HardValue createBefore(Float before) {
      return ValueFactory::createFloat(this->allocator, before);
    }
  };

  // TODO: Make 'String' implement 'IValue'
  class ValueString final : public ValueMutable {
    ValueString(const ValueString&) = delete;
    ValueString& operator=(const ValueString&) = delete;
  private:
    String value;
  public:
    ValueString(IAllocator& allocator, const String& value)
      : ValueMutable(allocator), value(value) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::String;
    }
    virtual Type getRuntimeType() const override {
      return Type::String;
    }
    virtual bool getString(String& result) const override {
      result = this->value;
      return true;
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && this->value.validate();
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->value);
    }
    virtual bool set(const IValue& rhs) override {
      return rhs.getString(this->value);
    }
    virtual HardValue mutate(Mutation op, const IValue& rhs) override {
      // There are few valid mutation operations on strings
      if (op == Mutation::Assign) {
        String rvalue;
        if (rhs.getString(rvalue)) {
          String before{ this->value.exchange(rvalue.get()) };
          return ValueFactory::createString(this->allocator, before);
        }
        return this->createException("TODO: Invalid right-hand value for mutation assignment to string"); // WIBBLE
      }
      if (op == Mutation::Noop) {
        assert(rhs.getFlags() == ValueFlags::Void);
        return HardValue(*this);
      }
      return this->createException("TODO: Mutation not supported for strings"); // WIBBLE
    }
  };

  class ValueHardObject final : public ValueMutable {
    ValueHardObject(const ValueHardObject&) = delete;
    ValueHardObject& operator=(const ValueHardObject&) = delete;
  private:
    HardObject value;
  public:
    ValueHardObject(IAllocator& allocator, const HardObject& value)
      : ValueMutable(allocator), value(value) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual Type getRuntimeType() const override {
      return Type::Object;
    }
    virtual bool getHardObject(HardObject& result) const override {
      result = this->value;
      return true;
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && this->value.validate();
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->value);
    }
    virtual bool set(const IValue& rhs) override {
      return rhs.getHardObject(this->value);
    }
    virtual HardValue mutate(Mutation op, const IValue& rhs) override {
      // There are few valid mutation operations on objects
      if (op == Mutation::Assign) {
        HardObject rvalue;
        if (rhs.getHardObject(rvalue)) {
          HardObject before{ this->value.exchange(rvalue.get()) };
          return ValueFactory::createHardObject(this->allocator, before);
        }
        return this->createException("TODO: Invalid right-hand value for mutation assignment to object"); // WIBBLE
      }
      if (op == Mutation::Noop) {
        assert(rhs.getFlags() == ValueFlags::Void);
        return HardValue(*this);
      }
      return this->createException("TODO: Mutation not supported for objects"); // WIBBLE
    }
  };

  class ValueHardFlowControl final : public ValueMutable {
    ValueHardFlowControl(const ValueHardFlowControl&) = delete;
    ValueHardFlowControl& operator=(const ValueHardFlowControl&) = delete;
  private:
    ValueFlags flags;
    HardValue inner;
  public:
    ValueHardFlowControl(IAllocator& allocator, ValueFlags flags, const HardValue& inner)
      : ValueMutable(allocator), flags(flags), inner(inner) {
      assert(this->validate());
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // Our inner value is a hard reference as we only expect to exist for a short time
    }
    virtual ValueFlags getFlags() const override {
      return this->flags | this->inner->getFlags();
    }
    virtual Type getRuntimeType() const override {
      // We should NOT really be asked for type information here
      assert(false);
      return Type::Void;
    }
    virtual bool getInner(HardValue& value) const override {
      value = this->inner;
      return true;
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && Bits::hasAnySet(this->flags, ValueFlags::FlowControl) && this->inner.validate();
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->flags);
      printer << ' ';
      printer.write(this->inner);
    }
    virtual bool set(const IValue&) override {
      // Flow controls are effectively immutable
      return false;
    }
    virtual HardValue mutate(Mutation op, const IValue&) override {
      // There are few valid mutation operations on objects
      if (op == Mutation::Noop) {
        return HardValue(*this);
      }
      return this->createException("TODO: Mutation not supported for objects"); // WIBBLE
    }
  };

  class ValuePoly final : public SoftReferenceCountedAllocator<IValue> {
    ValuePoly(const ValuePoly&) = delete;
    ValuePoly& operator=(const ValuePoly&) = delete;
  private:
    ValueFlags flags;
    union {
      Atomic<Bool> bvalue;
      Atomic<Int> ivalue;
      Atomic<Float> fvalue;
      const IMemory* svalue;
      IObject* ovalue;
    };
  public:
    ValuePoly(IAllocator& allocator)
      : SoftReferenceCountedAllocator<IValue>(allocator),
        flags(ValueFlags::Void) {
        assert(this->validate());
    }
    virtual ~ValuePoly() {
      this->destroy();
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      if (this->flags == ValueFlags::Object) {
        auto* object = this->ovalue;
        assert(object != nullptr);
        visitor.visit(*object);
      }
    }
    virtual bool getVoid() const override {
      assert(this->validate());
      return this->flags == ValueFlags::Void;
    }
    virtual bool getNull() const override {
      assert(this->validate());
      return this->flags == ValueFlags::Null;
    }
    virtual bool getBool(Bool& value) const override {
      assert(this->validate());
      if (this->flags == ValueFlags::Bool) {
        value = this->bvalue.get();
        return true;
      }
      return false;
    }
    virtual bool getInt(Int& value) const override {
      assert(this->validate());
      if (this->flags == ValueFlags::Int) {
        value = this->ivalue.get();
        return true;
      }
      return false;
    }
    virtual bool getFloat(Float& value) const override {
      assert(this->validate());
      if (this->flags == ValueFlags::Float) {
        value = this->fvalue.get();
        return true;
      }
      return false;
    }
    virtual bool getString(String& value) const override {
      assert(this->validate());
      if (this->flags == ValueFlags::String) {
        value.set(this->svalue);
        return true;
      }
      return false;
    }
    virtual bool getHardObject(HardObject& value) const override {
      assert(this->validate());
      if (this->flags == ValueFlags::Object) {
        value.set(this->ovalue);
        return true;
      }
      return false;
    }
    virtual bool getInner(HardValue&) const override {
      // We never have inner values
      return false;
    }
    virtual ValueFlags getFlags() const override {
      assert(this->validate());
      return this->flags;
    }
    virtual Type getRuntimeType() const override {
      // TODO
      assert(false);
      return Type::Void;
    }
    virtual void print(Printer& printer) const override {
      assert(this->validate());
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (this->flags) {
      case ValueFlags::Bool:
        printer.write(this->bvalue.get());
        break;
      case ValueFlags::Int:
        printer.write(this->ivalue.get());
        break;
      case ValueFlags::Float:
        printer.write(this->fvalue.get());
        break;
      case ValueFlags::String:
        printer.write(String(this->svalue));
        break;
      case ValueFlags::Object:
        assert(this->ovalue != nullptr);
        this->ovalue->print(printer);
        break;
      default:
        printer.write(this->flags);
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
    }
    bool validate() const {
      // TODO
      if (this->flags == ValueFlags::Object) {
        auto* object = this->ovalue;
        return (object != nullptr) && (this->basket != nullptr) && (object->softGetBasket() == this->basket);
      }
      return true;
    }
    HardValue hardClone() const {
      // TODO atomic
      assert(this->validate());
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (this->flags) {
      case ValueFlags::Void:
        return HardValue::Void;
      case ValueFlags::Null:
        return HardValue::Null;
      case ValueFlags::Bool:
        return ValueFactory::createBool(this->bvalue.get());
      case ValueFlags::Int:
        return ValueFactory::createInt(this->allocator, this->ivalue.get());
      case ValueFlags::Float:
        return ValueFactory::createFloat(this->allocator, this->fvalue.get());
      case ValueFlags::String:
        return ValueFactory::createString(this->allocator, String(this->svalue));
      case ValueFlags::Object:
        return ValueFactory::createHardObject(this->allocator, HardObject(this->ovalue));
      default:
        assert(false);
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return HardValue::Rethrow;
    }
    virtual bool set(const IValue& value) override {
      // TODO atomic
      assert(this->validate());
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (value.getFlags()) {
      case ValueFlags::Void:
        this->destroy();
        this->flags = ValueFlags::Void;
        assert(this->validate());
        return true;
      case ValueFlags::Null:
        this->destroy();
        this->flags = ValueFlags::Null;
        assert(this->validate());
        return true;
      case ValueFlags::Bool:
      {
        Bool b;
        if (value.getBool(b)) {
          this->destroy();
          this->flags = ValueFlags::Bool;
          this->bvalue.exchange(b);
          assert(this->validate());
          return true;
        }
        break;
      }
      case ValueFlags::Int:
      {
        Int i;
        if (value.getInt(i)) {
          this->destroy();
          this->flags = ValueFlags::Int;
          this->ivalue.exchange(i);
          assert(this->validate());
          return true;
        }
        break;
      }
      case ValueFlags::Float:
      {
        Float f;
        if (value.getFloat(f)) {
          this->destroy();
          this->flags = ValueFlags::Float;
          this->fvalue.exchange(f);
          assert(this->validate());
          return true;
        }
        break;
      }
      case ValueFlags::String:
      {
        String s;
        if (value.getString(s)) {
          this->destroy();
          this->flags = ValueFlags::String;
          this->svalue = static_cast<const IMemory*>(s->hardAcquire());
          assert(this->validate());
          return true;
        }
        break;
      }
      case ValueFlags::Object:
      {
        HardObject o;
        assert(this->basket != nullptr);
        if (value.getHardObject(o)) {
          auto* instance = o.get();
          assert(instance != nullptr);
          auto* taken = this->basket->take(*instance);
          assert(taken == instance);
          this->destroy();
          this->flags = ValueFlags::Object;
          this->ovalue = static_cast<IObject*>(taken);
          assert(this->validate());
          return true;
        }
        break;
      }
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return false;
    }
    virtual HardValue mutate(Mutation op, const IValue& rhs) override {
      switch (op) {
      case Mutation::Assign:
      {
        auto before = this->hardClone();
        if (this->set(rhs)) {
          return before;
        }
        return this->createException("TODO: Invalid right-hand value for mutation assignment");
      }
      case Mutation::Decrement:
        assert(rhs.getFlags() == ValueFlags::Void);
        if (this->flags == ValueFlags::Int) {
          return this->createBeforeInt(this->ivalue.decrement() - 1);
        }
        return this->createException("TODO: Mutation '--' only supported for integers");
      case Mutation::Increment:
        assert(rhs.getFlags() == ValueFlags::Void);
        if (this->flags == ValueFlags::Int) {
          return this->createBeforeInt(this->ivalue.increment() + 1);
        }
        return this->createException("TODO: Mutation '++' only supported for integers");
      case Mutation::Add:
        return this->createException("TODO: Mutation '+' not supported for poly"); // WIBBLE
      case Mutation::Subtract:
        return this->createException("TODO: Mutation '-' not supported for poly"); // WIBBLE
      case Mutation::Multiply:
        return this->createException("TODO: Mutation '*' not supported for poly"); // WIBBLE
      case Mutation::Divide:
        return this->createException("TODO: Mutation '/' not supported for poly"); // WIBBLE
      case Mutation::Remainder:
        return this->createException("TODO: Mutation '%' not supported for poly"); // WIBBLE
      case Mutation::BitwiseAnd:
        return this->createException("TODO: Mutation '&' not supported for poly"); // WIBBLE
      case Mutation::BitwiseOr:
        return this->createException("TODO: Mutation '|' not supported for poly"); // WIBBLE
      case Mutation::BitwiseXor:
        return this->createException("TODO: Mutation '^' not supported for poly"); // WIBBLE
      case Mutation::ShiftLeft:
        return this->createException("TODO: Mutation '<<' not supported for poly"); // WIBBLE
      case Mutation::ShiftRight:
        return this->createException("TODO: Mutation '>>' not supported for poly"); // WIBBLE
      case Mutation::ShiftRightUnsigned:
        return this->createException("TODO: Mutation '>>>' not supported for poly"); // WIBBLE
      case Mutation::Noop:
        assert(rhs.getFlags() == ValueFlags::Void);
        return this->hardClone();
      }
      // TODO
      return this->createException("TODO: Mutation operator unknown"); // WIBBLE
    }
  private:
    HardValue createBeforeInt(Int before) {
      return ValueFactory::createInt(this->allocator, before);
    }
    HardValue createException(const char* message) const {
      auto inner = ValueFactory::createStringASCII(this->allocator, message);
      return ValueFactory::createHardFlowControl(this->allocator, ValueFlags::Throw, inner);
    }
    void destroy() {
      if (this->flags == ValueFlags::String) {
        this->svalue->hardRelease();
      }
    }
  };

  bool validateFlags(ValueFlags flags) {
    auto upper = Bits::mask(flags, ValueFlags::FlowControl);
    auto lower = Bits::clear(flags, ValueFlags::FlowControl);
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (upper) {
    case ValueFlags::None:
    case ValueFlags::Return:
    case ValueFlags::Yield:
      // Expect a simple value
      return Bits::hasOneSet(lower);
    case ValueFlags::Break:
    case ValueFlags::Continue:
      // Don't expect anything else
      return !Bits::hasAnySet(lower);
    case ValueFlags::Throw:
      // Throw with nothing is a rethrow
      return Bits::hasZeroOrOneSet(lower);
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    return false;
  }

  int compareBool(Bool lhs, Bool rhs) {
    return (lhs == rhs) ? 0 : (lhs == false) ? -1 : +1;
  }

  int compareInt(Int lhs, Int rhs) {
    return (lhs == rhs) ? 0 : (lhs < rhs) ? -1 : +1;
  }

  int compareFloat(Float lhs, Float rhs) {
    // Place NaNs before all other values
    if (std::isnan(lhs)) {
      return std::isnan(rhs) ? 0 : -1;
    }
    if (std::isnan(rhs)) {
      return +1;
    }
    return (lhs == rhs) ? 0 : (lhs < rhs) ? -1 : +1;
  }

  int compareString(const String& lhs, const String& rhs) {
    // Codepoint ordering
    return int(lhs.compareTo(rhs));
  }

  int compareObject(const HardObject lhs, const HardObject& rhs) {
    // TODO: More complex/stable ordering?
    auto* lptr = lhs.get();
    auto* rptr = rhs.get();
    return (lptr == rptr) ? 0 : (lptr < rptr) ? -1 : +1;
  }
}

const egg::ovum::HardValue egg::ovum::HardValue::Void{ theVoid.instance() };
const egg::ovum::HardValue egg::ovum::HardValue::Null{ theNull.instance() };
const egg::ovum::HardValue egg::ovum::HardValue::False{ theFalse.instance() };
const egg::ovum::HardValue egg::ovum::HardValue::True{ theTrue.instance() };
const egg::ovum::HardValue egg::ovum::HardValue::Break{ theBreak.instance() };
const egg::ovum::HardValue egg::ovum::HardValue::Continue{ theContinue.instance() };
const egg::ovum::HardValue egg::ovum::HardValue::Rethrow{ theRethrow.instance() };

egg::ovum::HardValue::HardValue() : ptr(&theVoid) {
  assert(this->validate());
}

egg::ovum::HardValue egg::ovum::ValueFactory::createInt(IAllocator& allocator, Int value) {
  return makeHardValue<ValueInt>(allocator, value);
}

egg::ovum::HardValue egg::ovum::ValueFactory::createFloat(IAllocator& allocator, Float value) {
  return makeHardValue<ValueFloat>(allocator, value);
}

egg::ovum::HardValue egg::ovum::ValueFactory::createString(IAllocator& allocator, const String& value) {
  return makeHardValue<ValueString>(allocator, value);
}

egg::ovum::HardValue egg::ovum::ValueFactory::createHardObject(IAllocator& allocator, const HardObject& value) {
  return makeHardValue<ValueHardObject>(allocator, value);
}

egg::ovum::HardValue egg::ovum::ValueFactory::createHardFlowControl(IAllocator& allocator, ValueFlags flags, const HardValue& value) {
  return makeHardValue<ValueHardFlowControl>(allocator, flags, value);
}

egg::ovum::HardValue egg::ovum::ValueFactory::createStringASCII(IAllocator& allocator, const char* value, size_t codepoints) {
  // TODO check 7-bit only
  if (value == nullptr) {
    return HardValue::Void;
  }
  if (codepoints == SIZE_MAX) {
    codepoints = std::strlen(value);
  }
  return makeHardValue<ValueString>(allocator, String::fromUTF8(&allocator, value, codepoints, codepoints));
}

egg::ovum::HardValue egg::ovum::ValueFactory::createStringUTF8(IAllocator& allocator, const std::u8string& value, size_t codepoints) {
  return makeHardValue<ValueString>(allocator, String::fromUTF8(&allocator, value.data(), value.size(), codepoints));
}

egg::ovum::HardValue egg::ovum::ValueFactory::createStringUTF32(IAllocator& allocator, const std::u32string& value) {
  return makeHardValue<ValueString>(allocator, String::fromUTF32(&allocator, value.data(), value.size()));
}

bool egg::ovum::HardValue::validate() const {
  auto p = this->ptr.get();
  if (p == nullptr) {
    return false;
  }
  if (!validateFlags(p->getFlags())) {
    return false;
  }
  return p->validate();
}

egg::ovum::SoftKey::SoftKey(const SoftKey& value) : ptr(&value.get()) {
  assert(this->validate());
}

egg::ovum::SoftKey::SoftKey(IVM& vm, const IValue& value) : ptr(vm.createSoftAlias(value)) {
  assert(this->validate());
}

bool egg::ovum::SoftKey::validate() const {
  auto p = this->ptr;
  if (p == nullptr) {
    return false;
  }
  if (!validateFlags(p->getFlags())) {
    return false;
  }
  return p->validate();
}

int egg::ovum::SoftKey::compare(const IValue& lhs, const IValue& rhs) {
  auto lflags = lhs.getFlags();
  auto rflags = rhs.getFlags();
  if (lflags != rflags) {
    return compareInt(Int(lflags), Int(rflags));
  }
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN
  switch (lhs.getFlags()) {
  case ValueFlags::Void:
  case ValueFlags::Null:
    // Must be equal
    return 0;
  case ValueFlags::Bool:
  {
    Bool lvalue, rvalue;
    if (lhs.getBool(lvalue) && rhs.getBool(rvalue)) {
      return compareBool(lvalue, rvalue);
    }
    break;
  }
  case ValueFlags::Int:
  {
    Int lvalue, rvalue;
    if (lhs.getInt(lvalue) && rhs.getInt(rvalue)) {
      return compareInt(lvalue, rvalue);
    }
    break;
  }
  case ValueFlags::Float:
  {
    Float lvalue, rvalue;
    if (lhs.getFloat(lvalue) && rhs.getFloat(rvalue)) {
      return compareFloat(lvalue, rvalue);
    }
    break;
  }
  case ValueFlags::String:
  {
    String lvalue, rvalue;
    if (lhs.getString(lvalue) && rhs.getString(rvalue)) {
      return compareString(lvalue, rvalue);
    }
    break;
  }
  case ValueFlags::Object:
  {
    // TODO: More complex/stable ordering?
    HardObject lvalue, rvalue;
    if (lhs.getHardObject(lvalue) && rhs.getHardObject(rvalue)) {
      return compareObject(lvalue, rvalue);
    }
    break;
  }
  }
  EGG_WARNING_SUPPRESS_SWITCH_END
  // Not comparable
  assert(false);
  return 0;
}

egg::ovum::SoftValue::SoftValue(IVM& vm) : ptr(vm.createSoftValue()) {
  assert(this->validate());
}

bool egg::ovum::SoftValue::validate() const {
  auto p = this->ptr.get();
  if (p == nullptr) {
    return false;
  }
  if (!validateFlags(p->getFlags())) {
    return false;
  }
  return p->validate();
}

egg::ovum::IValue* egg::ovum::SoftValue::createPoly(IAllocator& allocator) {
  return allocator.makeRaw<ValuePoly>(allocator);
}
