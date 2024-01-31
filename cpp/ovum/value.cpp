#include "ovum/ovum.h"

namespace egg::internal {
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
    virtual bool getHardType(Type&) const override {
      return false;
    }
    virtual bool getInner(HardValue&) const override {
      return false;
    }
    virtual ValueFlags getPrimitiveFlag() const override {
      static_assert(Bits::hasOneSet(FLAGS), "Exactly one primitive flag bit should be set");
      return FLAGS;
    }
    virtual int print(Printer& printer) const override {
      printer << FLAGS;
      return 0;
    }
    virtual bool validate() const override {
      return true;
    }
    virtual bool set(const IValue&) override {
      // Cannot set an immutable instance
      return false;
    }
    virtual HardValue mutate(ValueMutationOp op, const IValue&) override {
      // There are very few valid mutation operations on immutables!
      if (op == ValueMutationOp::Noop) {
        return HardValue(*this);
      }
      return HardValue::Rethrow; // No allocator available
    }
    constexpr IValue& instance() const {
      return *const_cast<ValueImmutable*>(this);
    }
  };

  class ValueBreak : public ValueImmutable<ValueFlags::Break> {
    ValueBreak(const ValueBreak&) = delete;
    ValueBreak& operator=(const ValueBreak&) = delete;
  public:
    ValueBreak() {}
    virtual Type getRuntimeType() const override {
      return Type::None;
    }
  };
  const ValueBreak theBreak;

  class ValueContinue : public ValueImmutable<ValueFlags::Continue> {
    ValueContinue(const ValueContinue&) = delete;
    ValueContinue& operator=(const ValueContinue&) = delete;
  public:
    ValueContinue() {}
    virtual Type getRuntimeType() const override {
      return Type::None;
    }
  };
  const ValueContinue theContinue;

  class ValueRethrow : public ValueImmutable<ValueFlags::Throw> {
    ValueRethrow(const ValueRethrow&) = delete;
    ValueRethrow& operator=(const ValueRethrow&) = delete;
  public:
    ValueRethrow() {}
    virtual Type getRuntimeType() const override {
      return Type::None;
    }
  };
  const ValueRethrow theRethrow;

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
    virtual int print(Printer& printer) const override {
      printer << VALUE;
      return 0;
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
    virtual bool getHardType(Type&) const override {
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
    HardValue createRuntimeError(const char* ascii) const {
      auto message = ValueFactory::createStringASCII(this->allocator, ascii);
      return ValueFactory::createHardThrow(this->allocator, message);
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
    virtual Type getRuntimeType() const override {
      return Type::Int;
    }
    virtual ValueFlags getPrimitiveFlag() const override {
      return ValueFlags::Int;
    }
    virtual bool getInt(Int& result) const override {
      result = this->value.get();
      return true;
    }
    virtual int print(Printer& printer) const override {
      printer << this->value.get();
      return 0;
    }
    virtual bool set(const IValue& rhs) override {
      Int rvalue;
      if (rhs.getInt(rvalue)) {
        this->value.set(rvalue);
        return true;
      }
      return false;
    }
    virtual HardValue mutate(ValueMutationOp op, const IValue& rhs) override {
      Int rvalue;
      switch (op) {
      case ValueMutationOp::Assign:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.exchange(rvalue));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation assignment to integer");
      case ValueMutationOp::Decrement:
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        return this->createBefore(this->value.add(-1));
      case ValueMutationOp::Increment:
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        return this->createBefore(this->value.add(+1));
      case ValueMutationOp::Add:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.add(rvalue));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation addition to integer");
      case ValueMutationOp::Subtract:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.sub(rvalue));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation subtract from integer");
      case ValueMutationOp::Multiply:
        if (rhs.getInt(rvalue)) {
          return this->createAtomic([rvalue](Int lvalue) { return lvalue * rvalue; });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation multiply by integer");
      case ValueMutationOp::Divide:
        if (rhs.getInt(rvalue)) {
          if (rvalue == 0) {
            return this->createRuntimeError("TODO: Division by zero in mutation divide");
          }
          return this->createAtomic([rvalue](Int lvalue) { return lvalue / rvalue; });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation divide by integer");
      case ValueMutationOp::Remainder:
        if (rhs.getInt(rvalue)) {
          if (rvalue == 0) {
            return this->createRuntimeError("TODO: Division by zero in mutation remainder");
          }
          return this->createAtomic([rvalue](Int lvalue) { return lvalue % rvalue; });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation remainder for integer");
      case ValueMutationOp::BitwiseAnd:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.bitwiseAnd(rvalue));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation bitwise-and of integer");
      case ValueMutationOp::BitwiseOr:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.bitwiseOr(rvalue));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation bitwise-or of integer");
      case ValueMutationOp::BitwiseXor:
        if (rhs.getInt(rvalue)) {
          return this->createBefore(this->value.bitwiseXor(rvalue));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation bitwise-xor of integer");
      case ValueMutationOp::ShiftLeft:
        if (rhs.getInt(rvalue)) {
          return this->createAtomic([rvalue](Int lvalue) { return Arithmetic::shift(Arithmetic::Shift::ShiftLeft, lvalue, rvalue); });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation shift left of integer");
      case ValueMutationOp::ShiftRight:
        if (rhs.getInt(rvalue)) {
          return this->createAtomic([rvalue](Int lvalue) { return Arithmetic::shift(Arithmetic::Shift::ShiftRight, lvalue, rvalue); });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation shift right of integer");
      case ValueMutationOp::ShiftRightUnsigned:
        if (rhs.getInt(rvalue)) {
          return this->createAtomic([rvalue](Int lvalue) { return Arithmetic::shift(Arithmetic::Shift::ShiftRightUnsigned, lvalue, rvalue); });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation unsigned shift right of integer");
      case ValueMutationOp::IfNull:
        return this->createBefore(this->value.get());
      case ValueMutationOp::IfFalse:
        return this->createRuntimeError("TODO: Mutation operator '||=' is unsupported for integers");
      case ValueMutationOp::IfTrue:
        return this->createRuntimeError("TODO: Mutation operator '&&=' is unsupported for integers");
      case ValueMutationOp::Noop:
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        return this->createBefore(this->value.get());
      }
      return this->createRuntimeError("TODO: Unknown integer mutation operation");
    }
  private:
    HardValue createAtomic(std::function<Int(Int)> eval) {
      Int before, after;
      do {
        before = this->value.get();
        after = eval(before);
      } while (this->value.update(before, after) != before);
      return ValueFactory::createInt(this->allocator, before);
    }
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
    virtual Type getRuntimeType() const override {
      return Type::Float;
    }
    virtual ValueFlags getPrimitiveFlag() const override {
      return ValueFlags::Float;
    }
    virtual bool getFloat(Float& result) const override {
      result = this->value.get();
      return true;
    }
    virtual int print(Printer& printer) const override {
      printer << this->value.get();
      return 0;
    }
    virtual bool set(const IValue& rhs) override {
      Float rvalue;
      if (rhs.getFloat(rvalue)) {
        this->value.set(rvalue);
        return true;
      }
      return false;
    }
    virtual HardValue mutate(ValueMutationOp op, const IValue& rhs) override {
      Float fvalue;
      Int ivalue;
      switch (op) {
      case ValueMutationOp::Assign:
        if (rhs.getFloat(fvalue)) {
          return this->createBefore(this->value.exchange(fvalue));
        }
        if (rhs.getInt(ivalue)) {
          return this->createBefore(this->value.exchange(Float(ivalue)));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation assignment to float");
      case ValueMutationOp::Decrement:
        return this->createRuntimeError("TODO: Mutation decrement is unsupported for floats");
      case ValueMutationOp::Increment:
        return this->createRuntimeError("TODO: Mutation increment is unsupported for floats");
      case ValueMutationOp::Add:
        if (rhs.getFloat(fvalue)) {
          return this->createBefore(this->value.add(fvalue));
        }
        if (rhs.getInt(ivalue)) {
          return this->createBefore(this->value.add(Float(ivalue)));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation addition to float");
      case ValueMutationOp::Subtract:
        if (rhs.getFloat(fvalue)) {
          return this->createBefore(this->value.sub(fvalue));
        }
        if (rhs.getInt(ivalue)) {
          return this->createBefore(this->value.sub(Float(ivalue)));
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation subtract from float");
      case ValueMutationOp::Multiply:
        if (rhs.getFloat(fvalue)) {
          return this->createAtomic([fvalue](Float lvalue) { return lvalue * fvalue; });
        }
        if (rhs.getInt(ivalue)) {
          fvalue = Float(ivalue);
          return this->createAtomic([ivalue](Float lvalue) { return lvalue * Float(ivalue); });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation multiply by float");
      case ValueMutationOp::Divide:
        if (rhs.getFloat(fvalue)) {
          return this->createAtomic([fvalue](Float lvalue) { return lvalue / fvalue; });
        }
        if (rhs.getInt(ivalue)) {
          // Promote explicitly to guarantee division by zero success
          return this->createAtomic([ivalue](Float lvalue) { return lvalue / Float(ivalue); });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation divide by float");
      case ValueMutationOp::Remainder:
        if (rhs.getFloat(fvalue)) {
          return this->createAtomic([fvalue](Float lvalue) { return std::fmod(lvalue, fvalue); });
        }
        if (rhs.getInt(ivalue)) {
          // Promote explicitly to guarantee division by zero success
          return this->createAtomic([ivalue](Float lvalue) { return std::fmod(lvalue, Float(ivalue)); });
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation remainder for float");
      case ValueMutationOp::BitwiseAnd:
        return this->createRuntimeError("TODO: Mutation bitwise-and is unsupported for floats");
      case ValueMutationOp::BitwiseOr:
        return this->createRuntimeError("TODO: Mutation bitwise-or is unsupported for floats");
      case ValueMutationOp::BitwiseXor:
        return this->createRuntimeError("TODO: Mutation bitwise-xor is unsupported for floats");
      case ValueMutationOp::ShiftLeft:
        return this->createRuntimeError("TODO: Mutation shift left is unsupported for floats");
      case ValueMutationOp::ShiftRight:
        return this->createRuntimeError("TODO: Mutation shift right is unsupported for floats");
      case ValueMutationOp::ShiftRightUnsigned:
        return this->createRuntimeError("TODO: Mutation unsigned shift right is unsupported for floats");
      case ValueMutationOp::IfNull:
        return this->createBefore(this->value.get());
      case ValueMutationOp::IfFalse:
        return this->createRuntimeError("TODO: Mutation operator '||=' is unsupported for floats");
      case ValueMutationOp::IfTrue:
        return this->createRuntimeError("TODO: Mutation operator '&&=' is unsupported for floats");
      case ValueMutationOp::Noop:
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        return this->createBefore(this->value.get());
      }
      return this->createRuntimeError("TODO: Unknown float mutation operation");
    }
  private:
    HardValue createAtomic(std::function<Float(Float)> eval) {
      // Don't use IEEE equality to detect updates
      Float before, after;
      do {
        before = this->value.get();
        after = eval(before);
      } while (!Arithmetic::equal(this->value.update(before, after), before, false));
      return ValueFactory::createFloat(this->allocator, before);
    }
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
    virtual Type getRuntimeType() const override {
      return Type::String;
    }
    virtual ValueFlags getPrimitiveFlag() const override {
      return ValueFlags::String;
    }
    virtual bool getString(String& result) const override {
      result = this->value;
      return true;
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && this->value.validate();
    }
    virtual int print(Printer& printer) const override {
      printer << this->value;
      return 0;
    }
    virtual bool set(const IValue& rhs) override {
      return rhs.getString(this->value);
    }
    virtual HardValue mutate(ValueMutationOp op, const IValue& rhs) override {
      // There are few valid mutation operations on strings
      if (op == ValueMutationOp::Assign) {
        String rvalue;
        if (rhs.getString(rvalue)) {
          this->value.swap(rvalue);
          return ValueFactory::createString(this->allocator, rvalue);
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation assignment to string");
      }
      if (op == ValueMutationOp::Noop) {
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        return HardValue(*this);
      }
      return this->createRuntimeError("TODO: Mutation not supported for strings");
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
    virtual Type getRuntimeType() const override {
      return this->value->vmRuntimeType();
    }
    virtual ValueFlags getPrimitiveFlag() const override {
      return ValueFlags::Object;
    }
    virtual bool getHardObject(HardObject& result) const override {
      result = this->value;
      return true;
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && this->value.validate();
    }
    virtual int print(Printer& printer) const override {
      printer << this->value;
      return 0;
    }
    virtual bool set(const IValue& rhs) override {
      return rhs.getHardObject(this->value);
    }
    virtual HardValue mutate(ValueMutationOp op, const IValue& rhs) override {
      // There are few valid mutation operations on objects
      if (op == ValueMutationOp::Assign) {
        HardObject rvalue;
        if (rhs.getHardObject(rvalue)) {
          this->value.swap(rvalue);
          return ValueFactory::createHardObject(this->allocator, rvalue);
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation assignment to object");
      }
      if (op == ValueMutationOp::Noop) {
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        return HardValue(*this);
      }
      return this->createRuntimeError("TODO: Mutation not supported for objects");
    }
  };

  class ValueHardInner final : public ValueMutable {
    ValueHardInner(const ValueHardInner&) = delete;
    ValueHardInner& operator=(const ValueHardInner&) = delete;
  private:
    ValueFlags flags;
    HardValue inner;
  public:
    ValueHardInner(IAllocator& allocator, ValueFlags flags, const HardValue& inner)
      : ValueMutable(allocator),
        flags(flags),
        inner(inner) {
      assert(this->validate());
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // Our inner value is a hard reference as we only expect to exist for a short time
    }
    virtual Type getRuntimeType() const override {
      return Type::None;
    }
    virtual ValueFlags getPrimitiveFlag() const override {
      return this->flags | this->inner->getPrimitiveFlag();
    }
    virtual bool getInner(HardValue& value) const override {
      value = this->inner;
      return true;
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && this->inner.validate();
    }
    virtual int print(Printer& printer) const override {
      printer << this->inner;
      return 0;
    }
    virtual bool set(const IValue&) override {
      // Flow controls are effectively immutable
      return false;
    }
    virtual HardValue mutate(ValueMutationOp op, const IValue&) override {
      // There are few valid mutation operations on objects
      if (op == ValueMutationOp::Noop) {
        return HardValue(*this);
      }
      return this->createRuntimeError("TODO: Mutation not supported for flow control instances");
    }
  };

  class ValueType : public ValueMutable {
    ValueType(const ValueType&) = delete;
    ValueType& operator=(const ValueType&) = delete;
  private:
    Type type;
  public:
    ValueType(IAllocator& allocator, const Type& type)
      : ValueMutable(allocator),
        type(type) {
      assert(type != nullptr);
    }
    virtual void softVisit(ICollectable::IVisitor& visitor) const override {
      this->type->softVisit(visitor);
    }
    virtual bool getHardType(Type& value) const override {
      value = this->type;
      return true;
    }
    virtual Type getRuntimeType() const override {
      return Type::None;
    }
    virtual ValueFlags getPrimitiveFlag() const override {
      return ValueFlags::Type;
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && (this->type != nullptr) && this->type->validate();
    }
    virtual int print(Printer& printer) const override {
      printer << this->type;
      return 0;
    }
    virtual bool set(const IValue&) override {
      // Types are effectively immutable
      return false;
    }
    virtual HardValue mutate(ValueMutationOp op, const IValue&) override {
      // There are few valid mutation operations on types
      if (op == ValueMutationOp::Noop) {
        return HardValue(*this);
      }
      return this->createRuntimeError("TODO: Mutation not supported for type values");
    }
  };

  class ValuePoly final : public SoftReferenceCountedAllocator<IValue> {
    ValuePoly(const ValuePoly&) = delete;
    ValuePoly& operator=(const ValuePoly&) = delete;
  private:
    ValueFlags flags;
    union {
      Atomic<Int> ivalue; // 0/1 for Bool
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
        value = this->ivalue.get() != 0;
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
    virtual bool getHardType(Type&) const override {
      // We never store types
      return false;
    }
    virtual bool getInner(HardValue&) const override {
      // We never have inner values
      return false;
    }
    virtual Type getRuntimeType() const override {
      assert(this->validate());
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (this->flags) {
      case ValueFlags::Void:
        return Type::Void;
      case ValueFlags::Null:
        return Type::Null;
      case ValueFlags::Bool:
        return Type::Bool;
      case ValueFlags::Int:
        return Type::Int;
      case ValueFlags::Float:
        return Type::Float;
      case ValueFlags::String:
        return Type::String;
      case ValueFlags::Object:
        assert(this->ovalue != nullptr);
        return this->ovalue->vmRuntimeType();
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return Type::None;
    }
    virtual ValueFlags getPrimitiveFlag() const override {
      assert(this->validate());
      return this->flags;
    }
    virtual int print(Printer& printer) const override {
      assert(this->validate());
      EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (this->flags) {
      case ValueFlags::Bool:
        printer << (this->ivalue.get() != 0);
        break;
      case ValueFlags::Int:
        printer << this->ivalue.get();
        break;
      case ValueFlags::Float:
        printer << this->fvalue.get();
        break;
      case ValueFlags::String:
        printer << String(this->svalue);
        break;
      case ValueFlags::Object:
        assert(this->ovalue != nullptr);
        this->ovalue->print(printer);
        break;
      default:
        printer << this->flags;
        break;
      }
      EGG_WARNING_SUPPRESS_SWITCH_END
      return 0;
    }
    bool validate() const {
      // TODO
      if (this->flags == ValueFlags::Bool) {
        // Only allow 0 and 1 as explicit values so the bitwise operations work without modifications
        return (this->ivalue.get() | 1) == 1;
      }
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
        return ValueFactory::createBool(this->ivalue.get() != 0);
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
      switch (value.getPrimitiveFlag()) {
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
          this->ivalue.set(b ? 1 : 0);
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
          this->ivalue.set(i);
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
          this->fvalue.set(f);
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
    virtual HardValue mutate(ValueMutationOp op, const IValue& rhs) override {
      switch (op) {
      case ValueMutationOp::Assign:
      {
        auto before = this->hardClone();
        if (this->set(rhs)) {
          return before;
        }
        return this->createRuntimeError("TODO: Invalid right-hand value for mutation assignment");
      }
      case ValueMutationOp::Decrement:
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        if (this->flags == ValueFlags::Int) {
          return this->createBeforeInt(this->ivalue.add(-1));
        }
        return this->createRuntimeError("TODO: Mutation decrement is only supported for values of type 'int'");
      case ValueMutationOp::Increment:
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        if (this->flags == ValueFlags::Int) {
          return this->createBeforeInt(this->ivalue.add(+1));
        }
        return this->createRuntimeError("TODO: Mutation increment is only supported for values of type 'int'");
      case ValueMutationOp::Add:
        return this->createArithmetic(rhs,
                                      [this](Int rvalue) { return this->ivalue.add(rvalue); },
                                      nullptr,
                                      [](Float lvalue, Float rvalue) { return lvalue + rvalue; },
                                      "TODO: Mutation addition is only supported for values of type 'int' or 'float'");
      case ValueMutationOp::Subtract:
        return this->createArithmetic(rhs,
                                      [this](Int rvalue) { return this->ivalue.sub(rvalue); },
                                      nullptr,
                                      [](Float lvalue, Float rvalue) { return lvalue - rvalue; },
                                      "TODO: Mutation subtract is only supported for values of type 'int' or 'float'");
      case ValueMutationOp::Multiply:
        return this->createArithmetic(rhs,
                                      nullptr,
                                      [](Int lvalue, Int rvalue) { return lvalue * rvalue; },
                                      [](Float lvalue, Float rvalue) { return lvalue * rvalue; },
                                      "TODO: Mutation multiply is only supported for values of type 'int' or 'float'");
      case ValueMutationOp::Divide:
        return this->createArithmetic(rhs,
                                      nullptr,
                                      [](Int lvalue, Int rvalue) { return lvalue / rvalue; },
                                      [](Float lvalue, Float rvalue) { return lvalue / rvalue; },
                                      "TODO: Mutation divide is only supported for values of type 'int' or 'float'",
                                      "TODO: Division by zero in mutation divide");
      case ValueMutationOp::Remainder:
        return this->createArithmetic(rhs,
                                      nullptr,
                                      [](Int lvalue, Int rvalue) { return lvalue % rvalue; },
                                      [](Float lvalue, Float rvalue) { return std::fmod(lvalue, rvalue); },
                                      "TODO: Mutation remainder is only supported for values of type 'int' or 'float'",
                                      "TODO: Division by zero in mutation remainder");
      case ValueMutationOp::BitwiseAnd:
        return this->createBitwise(rhs,
                                   [this](Int rvalue) { return this->ivalue.bitwiseAnd(rvalue); },
                                   "TODO: Mutation bitwise-and is only supported for values of type 'bool' or 'int'");
      case ValueMutationOp::BitwiseOr:
        return this->createBitwise(rhs,
                                   [this](Int rvalue) { return this->ivalue.bitwiseOr(rvalue); },
                                   "TODO: Mutation bitwise-or is only supported for values of type 'bool' or 'int'");
      case ValueMutationOp::BitwiseXor:
        return this->createBitwise(rhs,
                                   [this](Int rvalue) { return this->ivalue.bitwiseXor(rvalue); },
                                   "TODO: Mutation bitwise-xor is only supported for values of type 'bool' or 'int'");
      case ValueMutationOp::ShiftLeft:
        return this->createShift(rhs,
                                 Arithmetic::Shift::ShiftLeft,
                                 "TODO: Mutation shift left is only supported for values of type 'int'");
      case ValueMutationOp::ShiftRight:
        return this->createShift(rhs,
                                 Arithmetic::Shift::ShiftRight,
                                 "TODO: Mutation shift right is only supported for values of type 'int'");
      case ValueMutationOp::ShiftRightUnsigned:
        return this->createShift(rhs,
                                 Arithmetic::Shift::ShiftRightUnsigned,
                                 "TODO: Mutation unsigned shift right is only supported for values of type 'int'");
      case ValueMutationOp::IfNull:
      case ValueMutationOp::IfFalse:
      case ValueMutationOp::IfTrue:
        return this->createRuntimeError("TODO: Short-circuit mutations");
      case ValueMutationOp::Noop:
        assert(rhs.getPrimitiveFlag() == ValueFlags::Void);
        return this->hardClone();
      }
      return this->createRuntimeError("TODO: Unknown mutation");
    }
  private:
    HardValue createBeforeInt(Int before) {
      return ValueFactory::createInt(this->allocator, before);
    }
    HardValue createBeforeFloat(Float before) {
      return ValueFactory::createFloat(this->allocator, before);
    }
    HardValue createArithmetic(const IValue& rhs,
                               std::function<Int(Int)> atomicInt,
                               std::function<Int(Int, Int)> evalInt,
                               std::function<Float(Float, Float)> evalFloat,
                               const char* mismatchMessage,
                               const char* zeroMessage = nullptr) {
      assert((atomicInt != nullptr) || (evalInt != nullptr));
      assert(evalFloat != nullptr);
      if (this->flags == ValueFlags::Int) {
        Int irhs;
        if (rhs.getInt(irhs)) {
          // No need to promote
          if ((zeroMessage != nullptr) && (irhs == 0)) {
            return this->createRuntimeError(zeroMessage);
          }
          if (atomicInt != nullptr) {
            return this->createBeforeInt(atomicInt(irhs));
          }
          return this->createEvalInt(evalInt, irhs);
        }
        Float frhs;
        if (rhs.getFloat(frhs)) {
          // Need to promote the lhs target
          // TODO thread safety
          auto flhs = Float(this->ivalue.get());
          this->flags = ValueFlags::Float;
          this->fvalue.set(flhs);
          return this->createEvalFloat(evalFloat, frhs);
        }
        return this->createRuntimeError(mismatchMessage);
      }
      if (this->flags == ValueFlags::Float) {
        Float frhs;
        if (!rhs.getFloat(frhs)) {
          Int irhs;
          if (!rhs.getInt(irhs)) {
            return this->createRuntimeError(mismatchMessage);
          }
          frhs = Float(irhs);
        }
        return this->createEvalFloat(evalFloat, frhs);
      }
      return this->createRuntimeError(mismatchMessage);
    }
    HardValue createBitwise(const IValue& rhs,
                            std::function<Int(Int)> atomicInt,
                            const char* mismatchMessage) {
      if (this->flags == ValueFlags::Bool) {
        Bool brhs;
        if (rhs.getBool(brhs)) {
          auto before = atomicInt(brhs ? 1 : 0);
          return ValueFactory::createBool(before != 0);
        }
      } else if (this->flags == ValueFlags::Int) {
        Int irhs;
        if (rhs.getInt(irhs)) {
          return this->createBeforeInt(atomicInt(irhs));
        }
      }
      return this->createRuntimeError(mismatchMessage);
    }
    HardValue createShift(const IValue& rhs,
                          Arithmetic::Shift op,
                          const char* mismatchMessage) {
      if (this->flags == ValueFlags::Int) {
        Int irhs;
        if (rhs.getInt(irhs)) {
          return this->createEvalInt([op](Int lvalue, Int rvalue) { return Arithmetic::shift(op, lvalue, rvalue); }, irhs);
        }
      }
      return this->createRuntimeError(mismatchMessage);
    }
    HardValue createEvalInt(std::function<Int(Int, Int)> eval, Int rhs) {
      Int before, after;
      do {
        before = this->ivalue.get();
        after = eval(before, rhs);
      } while (this->ivalue.update(before, after) != before);
      return ValueFactory::createInt(this->allocator, before);
    }
    HardValue createEvalFloat(std::function<Float(Float, Float)> eval, Float rhs) {
      Float before, after;
      do {
        before = this->fvalue.get();
        after = eval(before, rhs);
      } while (this->fvalue.update(before, after) != before);
      return ValueFactory::createFloat(this->allocator, before);
    }
    HardValue createRuntimeError(const char* ascii) const {
      auto message = ValueFactory::createStringASCII(this->allocator, ascii);
      return ValueFactory::createHardThrow(this->allocator, message);
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

using namespace egg::internal;

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

egg::ovum::HardValue egg::ovum::ValueFactory::createHardThrow(IAllocator& allocator, const HardValue& value) {
  return makeHardValue<ValueHardInner>(allocator, ValueFlags::Throw, value);
}

egg::ovum::HardValue egg::ovum::ValueFactory::createHardReturn(IAllocator& allocator, const HardValue& value) {
  return makeHardValue<ValueHardInner>(allocator, ValueFlags::Return, value);
}

egg::ovum::HardValue egg::ovum::ValueFactory::createType(IAllocator& allocator, const Type& value) {
  return makeHardValue<ValueType>(allocator, value);
}

egg::ovum::HardValue egg::ovum::ValueFactory::createStringASCII(IAllocator& allocator, const char* value, size_t codepoints) {
  // TODO check 7-bit only
  if (value == nullptr) {
    return HardValue::Void;
  }
  if (codepoints == SIZE_MAX) {
    codepoints = std::strlen(value);
  }
  return makeHardValue<ValueString>(allocator, String::fromUTF8(allocator, value, codepoints, codepoints));
}

egg::ovum::HardValue egg::ovum::ValueFactory::createStringUTF8(IAllocator& allocator, const void* value, size_t bytes, size_t codepoints) {
  if (value == nullptr) {
    return HardValue::Void;
  }
  return makeHardValue<ValueString>(allocator, String::fromUTF8(allocator, value, bytes, codepoints));
}

egg::ovum::HardValue egg::ovum::ValueFactory::createStringUTF8(IAllocator& allocator, const std::u8string& value, size_t codepoints) {
  return makeHardValue<ValueString>(allocator, String::fromUTF8(allocator, value.data(), value.size(), codepoints));
}

egg::ovum::HardValue egg::ovum::ValueFactory::createStringUTF32(IAllocator& allocator, const std::u32string& value) {
  return makeHardValue<ValueString>(allocator, String::fromUTF32(allocator, value.data(), value.size()));
}

bool egg::ovum::HardValue::validate() const {
  auto p = this->ptr.get();
  if (p == nullptr) {
    return false;
  }
  if (!validateFlags(p->getPrimitiveFlag())) {
    return false;
  }
  return p->validate();
}

egg::ovum::SoftKey::SoftKey(const SoftKey& value) : ptr(&value.get()) {
  assert(this->validate());
}

egg::ovum::SoftKey::SoftKey(IVM&, const HardValue& hard) : ptr(&hard.get()) { // WIBBLE
  assert(this->validate());
}

/* WIBBLE
egg::ovum::SoftKey::SoftKey(IVM& vm, const HardValue& value) : ptr(vm.createSoftKey(value.get())) {
  assert(this->validate());
}
*/

egg::ovum::SoftKey& egg::ovum::SoftKey::soften(IVM& vm) {
  assert(this->validate());
  this->ptr = vm.softenWIBBLE(*this->ptr);
  assert(this->validate());
  return *this;
}

bool egg::ovum::SoftKey::validate() const {
  auto p = this->ptr;
  if (p == nullptr) {
    return false;
  }
  if (!validateFlags(p->getPrimitiveFlag())) {
    return false;
  }
  return p->validate();
}

int egg::ovum::SoftKey::compare(const IValue& lhs, const IValue& rhs) {
  auto lflags = lhs.getPrimitiveFlag();
  auto rflags = rhs.getPrimitiveFlag();
  if (lflags != rflags) {
    return compareInt(Int(lflags), Int(rflags));
  }
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN
  switch (lflags) {
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
  if (!validateFlags(p->getPrimitiveFlag())) {
    return false;
  }
  return p->validate();
}

egg::ovum::IValue* egg::ovum::SoftValue::createPoly(IAllocator& allocator) {
  return allocator.makeRaw<ValuePoly>(allocator);
}

bool egg::ovum::SoftValue::isPoly(const IValue* value) { // WOBBLE
  return (value != nullptr) && (typeid(*value) == typeid(ValuePoly));
}
