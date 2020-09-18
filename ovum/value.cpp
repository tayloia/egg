#include "ovum/ovum.h"
#include "ovum/print.h"

namespace {
  using namespace egg::ovum;

  template<ValueFlags FLAGS>
  class ValueImmutable : public NotReferenceCounted<IValue> {
    ValueImmutable(const ValueImmutable&) = delete;
    ValueImmutable& operator=(const ValueImmutable&) = delete;
  public:
    ValueImmutable() {}
    virtual IValue* softAcquire() const override {
      return const_cast<ValueImmutable*>(this);
    }
    virtual void softRelease() const override {
      // Do nothing
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
    virtual bool getObject(Object&) const override {
      return false;
    }
    virtual bool getMemory(Memory&) const override {
      return false;
    }
    virtual bool getChild(Value&) const override {
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
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      // Default equality for constants is when the types are the same
      return rhs.getFlags() == FLAGS;
    }
    virtual String toString() const override {
      return Print::toString(FLAGS);
    }
    virtual bool validate() const override {
      return true;
    }
    constexpr IValue* operator&() const {
      return const_cast<ValueImmutable*>(this);
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
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      Bool value;
      return rhs.getBool(value) && (value == VALUE);
    }
    virtual String toString() const override {
      return Print::toString(VALUE);
    }
  };
  const ValueBool<false> theFalse;
  const ValueBool<true> theTrue;

  class ValueMutable : public HardReferenceCounted<IValue> {
    ValueMutable(const ValueMutable&) = delete;
    ValueMutable& operator=(const ValueMutable&) = delete;
  public:
    ValueMutable(IAllocator& allocator) : HardReferenceCounted(allocator, 0) {
    }
    // Inherited via IValue
    virtual IValue* softAcquire() const override {
      return this->hardAcquire();
    }
    virtual void softRelease() const override {
      this->hardRelease();
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
    virtual bool getObject(Object&) const override {
      return false;
    }
    virtual bool getMemory(Memory&) const override {
      return false;
    }
    virtual bool getChild(Value&) const override {
      return false;
    }
  };

  class ValueInt final : public ValueMutable {
    ValueInt(const ValueInt&) = delete;
    ValueInt& operator=(const ValueInt&) = delete;
  private:
    Int value;
  public:
    ValueInt(IAllocator& allocator, Int value) : ValueMutable(allocator), value(value) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Int;
    }
    virtual Type getRuntimeType() const override {
      return Type::Int;
    }
    virtual bool getInt(Int& result) const override {
      result = this->value;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      Int i;
      if (rhs.getInt(i)) {
        return this->value == i;
      }
      if (Bits::hasAnySet(compare, ValueCompare::PromoteInts)) {
        Float f;
        if (rhs.getFloat(f)) {
          return Arithmetic::equal(f, this->value);
        }
      }
      return false;
    }
    virtual String toString() const override {
      return Print::toString(this->value);
    }
    virtual bool validate() const override {
      return true;
    }
  };

  class ValueFloat final : public ValueMutable {
    ValueFloat(const ValueFloat&) = delete;
    ValueFloat& operator=(const ValueFloat&) = delete;
  private:
    Float value;
  public:
    ValueFloat(IAllocator& allocator, Float value) : ValueMutable(allocator), value(value) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Float;
    }
    virtual Type getRuntimeType() const override {
      return Type::Float;
    }
    virtual bool getFloat(Float& result) const override {
      result = this->value;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      Float f;
      if (rhs.getFloat(f)) {
        return Arithmetic::equal(this->value, f, false);
      }
      if (Bits::hasAnySet(compare, ValueCompare::PromoteInts)) {
        Int i;
        if (rhs.getInt(i)) {
          return Arithmetic::equal(this->value, i);
        }
      }
      return false;
    }
    virtual String toString() const override {
      return Print::toString(this->value);
    }
    virtual bool validate() const override {
      return true;
    }
  };

  // TODO: Make 'String' implement 'IValue'
  class ValueString final : public ValueMutable {
    ValueString(const ValueString&) = delete;
    ValueString& operator=(const ValueString&) = delete;
  private:
    String value;
  public:
    ValueString(IAllocator& allocator, const String& value) : ValueMutable(allocator), value(value) {
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
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      String other;
      return rhs.getString(other) && this->value.equals(other);
    }
    virtual String toString() const override {
      return this->value;
    }
    virtual bool validate() const override {
      return this->value.validate();
    }
  };

  class ValueMemory final : public ValueMutable {
    ValueMemory(const ValueMemory&) = delete;
    ValueMemory& operator=(const ValueMemory&) = delete;
  private:
    Memory value;
  public:
    ValueMemory(IAllocator& allocator, const Memory& value) : ValueMutable(allocator), value(value) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Memory;
    }
    virtual Type getRuntimeType() const override {
      return Type::Memory;
    }
    virtual bool getMemory(Memory& result) const override {
      result = this->value;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      Memory other;
      return rhs.getMemory(other) && Memory::equals(this->value.get(), other.get());
    }
    virtual String toString() const override {
      return "<memory>";
    }
    virtual bool validate() const override {
      return this->value.validate();
    }
  };

  class ValueObject final : public ValueMutable {
    ValueObject(const ValueObject&) = delete;
    ValueObject& operator=(const ValueObject&) = delete;
  private:
    Object value;
  public:
    ValueObject(IAllocator& allocator, const Object& value) : ValueMutable(allocator), value(value) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual Type getRuntimeType() const override {
      return value->getRuntimeType();
    }
    virtual bool getObject(Object& result) const override {
      result = this->value;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      Object other{ *this->value };
      return rhs.getObject(other) && (this->value.get() == other.get());
    }
    virtual String toString() const override {
      return "<object>"; // TODO Object::toString()
    }
    virtual bool validate() const override {
      return this->value.validate();
    }
  };

  class ValuePointer final : public ValueMutable {
    ValuePointer(const ValuePointer&) = delete;
    ValuePointer& operator=(const ValuePointer&) = delete;
  private:
    Value child;
  public:
    ValuePointer(IAllocator& allocator, const Value& child) : ValueMutable(allocator), child(child) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Pointer;
    }
    virtual Type getRuntimeType() const override {
      return Type::Null; // WIBBLE
    }
    virtual bool getChild(Value& value) const override {
      value = child;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      Value other;
      return (rhs.getFlags() == ValueFlags::Pointer) && rhs.getChild(other) && this->equals(other.get(), compare);
    }
    virtual String toString() const override {
      return "<pointer>";
    }
    virtual bool validate() const override {
      return this->child.validate();
    }
  };

  class ValueFlowControl final : public ValueMutable {
    ValueFlowControl(const ValueFlowControl&) = delete;
    ValueFlowControl& operator=(const ValueFlowControl&) = delete;
  private:
    ValueFlags flags;
    Value child;
  public:
    ValueFlowControl(IAllocator& allocator, ValueFlags flags, const Value& child) : ValueMutable(allocator), flags(flags), child(child) {
      assert(this->validate());
    }
    virtual ValueFlags getFlags() const override {
      return this->flags | this->child->getFlags();
    }
    virtual Type getRuntimeType() const override {
      // We should NOT really be asked for type information here
      assert(false);
      return Type::Void;
    }
    virtual bool getChild(Value& value) const override {
      value = this->child;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      Value other;
      return (rhs.getFlags() == this->flags) && rhs.getChild(other) && this->child->equals(other.get(), compare);
    }
    virtual String toString() const override {
      StringBuilder sb;
      Print::add(sb, this->flags);
      sb.add(' ');
      Print::add(sb, this->child);
      return sb.str();
    }
    virtual bool validate() const override {
      return this->child.validate();
    }
  };

  bool validateFlags(ValueFlags flags) {
    auto upper = Bits::mask(flags, ValueFlags::FlowControl);
    auto lower = Bits::clear(flags, ValueFlags::FlowControl);
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
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
    EGG_WARNING_SUPPRESS_SWITCH_END();
    return false;
  }
}

const egg::ovum::Value egg::ovum::Value::Void{ &theVoid };
const egg::ovum::Value egg::ovum::Value::Null{ &theNull };
const egg::ovum::Value egg::ovum::Value::False{ &theFalse };
const egg::ovum::Value egg::ovum::Value::True{ &theTrue };
const egg::ovum::Value egg::ovum::Value::Break{ &theBreak };
const egg::ovum::Value egg::ovum::Value::Continue{ &theContinue };
const egg::ovum::Value egg::ovum::Value::Rethrow{ &theRethrow };

egg::ovum::Value::Value() : ptr(&theVoid) {
  assert(this->validate());
}

egg::ovum::Value egg::ovum::ValueFactory::createInt(IAllocator& allocator, Int value) {
  return Value(allocator.make<ValueInt, IValue*>(value));
}

egg::ovum::Value egg::ovum::ValueFactory::createFloat(IAllocator& allocator, Float value) {
  return Value(allocator.make<ValueFloat, IValue*>(value));
}

egg::ovum::Value egg::ovum::ValueFactory::createString(IAllocator& allocator, const String& value) {
  return Value(allocator.make<ValueString, IValue*>(value));
}

egg::ovum::Value egg::ovum::ValueFactory::createObject(IAllocator& allocator, const Object& value) {
  return Value(allocator.make<ValueObject, IValue*>(value));
}

egg::ovum::Value egg::ovum::ValueFactory::createMemory(IAllocator& allocator, const Memory& value) {
  return Value(allocator.make<ValueMemory, IValue*>(value));
}

egg::ovum::Value egg::ovum::ValueFactory::createPointer(IAllocator& allocator, const Value& value) {
  return Value(allocator.make<ValuePointer, IValue*>(value));
}

egg::ovum::Value egg::ovum::ValueFactory::createFlowControl(IAllocator& allocator, ValueFlags flags, const Value& value) {
  return Value(allocator.make<ValueFlowControl, IValue*>(flags, value));
}

egg::ovum::Value egg::ovum::ValueFactory::createException(IAllocator& allocator, const LocationSource& location, const String& message) {
  auto object = ObjectFactory::createVanillaException(allocator, location, message);
  auto value = ValueFactory::createObject(allocator, object);
  return ValueFactory::createFlowControl(allocator, ValueFlags::Throw, value);
}

bool egg::ovum::Value::validate() const {
  auto p = this->ptr.get();
  if (p == nullptr) {
    return false;
  }
  if (!validateFlags(p->getFlags())) {
    return false;
  }
  return p->validate();
}
