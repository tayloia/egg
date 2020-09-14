#include "ovum/ovum.h"

#include <iomanip>

namespace {
  using namespace egg::ovum;

  template<ValueFlags FLAGS>
  class ValueImmutable : public NotReferenceCounted<IValue> {
    ValueImmutable(const ValueImmutable&) = delete;
    ValueImmutable& operator=(const ValueImmutable&) = delete;
  public:
    ValueImmutable() {}
    virtual IValue* clone() const override {
      // Constants are immutable, so just return self
      return const_cast<ValueImmutable*>(this);
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
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      // Default equality for constants is when the types are the same
      return rhs.getFlags() == FLAGS;
    }
    virtual bool validate() const override {
      return true;
    }
    constexpr IValue* operator&() const {
      return const_cast<ValueImmutable*>(this);
    }
  };

  class ValueVoid : public ValueImmutable<ValueFlags::Void> {
    ValueVoid(const ValueVoid&) = delete;
    ValueVoid& operator=(const ValueVoid&) = delete;
  public:
    ValueVoid() {}
    virtual bool getVoid() const override {
      return true;
    }
  };

  class ValueNull : public ValueImmutable<ValueFlags::Null> {
    ValueNull(const ValueNull&) = delete;
    ValueNull& operator=(const ValueNull&) = delete;
  public:
    ValueNull() {}
    virtual bool getNull() const override {
      return true;
    }
  };

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
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      Bool value;
      return rhs.getBool(value) && (value == VALUE);
    }
  };

  class ValueMutable : public HardReferenceCounted<IValue> {
    ValueMutable(const ValueMutable&) = delete;
    ValueMutable& operator=(const ValueMutable&) = delete;
  public:
    ValueMutable(IAllocator& allocator) : HardReferenceCounted(allocator, 0) {
    }
    // Inherited via IValue
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
    virtual IValue* clone() const override {
      return this->allocator.make<ValueInt, ValueInt*>(this->value);
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Int;
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
          return arithmeticEquals(f, this->value);
        }
      }
      return false;
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
    virtual IValue* clone() const override {
      return this->allocator.make<ValueFloat, ValueFloat*>(this->value);
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Float;
    }
    virtual bool getFloat(Float& result) const override {
      result = this->value;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      Float f;
      if (rhs.getFloat(f)) {
        return arithmeticEquals(this->value, f, false);
      }
      if (Bits::hasAnySet(compare, ValueCompare::PromoteInts)) {
        Int i;
        if (rhs.getInt(i)) {
          return arithmeticEquals(this->value, i);
        }
      }
      return false;
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
    virtual IValue* clone() const override {
      return this->allocator.make<ValueString, ValueString*>(this->value);
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::String;
    }
    virtual bool getString(String& result) const override {
      result = this->value;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      String other;
      return rhs.getString(other) && this->value.equals(other);
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
    virtual IValue* clone() const override {
      // TODO Should we clone the reference to the object instance, not the object instance itself?
      return this->allocator.make<ValueObject, ValueObject*>(this->value);
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual bool getObject(Object& result) const override {
      result = this->value;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      Object other{ *this->value };
      return rhs.getObject(other) && (this->value.get() == other.get());
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
    virtual IValue* clone() const override {
      return this->allocator.make<ValueMemory, ValueMemory*>(this->value);
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Memory;
    }
    virtual bool getMemory(Memory& result) const override {
      result = this->value;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      Memory other;
      return rhs.getMemory(other) && Memory::equals(this->value.get(), other.get());
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
    virtual IValue* clone() const override {
      return this->allocator.make<ValuePointer, ValuePointer*>(this->child);
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Pointer;
    }
    virtual bool getChild(Value& value) const override {
      value = child;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      Value other;
      return (rhs.getFlags() == ValueFlags::Pointer) && rhs.getChild(other) && this->equals(other.get(), compare);
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
    virtual IValue* clone() const override {
      return this->allocator.make<ValueFlowControl, ValueFlowControl*>(this->flags, this->child);
    }
    virtual ValueFlags getFlags() const override {
      return this->flags | this->child->getFlags();
    }
    virtual bool getChild(Value& value) const override {
      value = this->child;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      Value other;
      return (rhs.getFlags() == this->flags) && rhs.getChild(other) && this->child->equals(other.get(), compare);
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

egg::ovum::Value::Value() {
  static const ValueVoid instance;
  this->ptr.set(&instance);
  assert(this->validate());
}

egg::ovum::Value egg::ovum::ValueFactory::createVoid() {
  return Value();
}

egg::ovum::Value egg::ovum::ValueFactory::createNull() {
  static const ValueNull instance;
  return Value(&instance);
}

egg::ovum::Value egg::ovum::ValueFactory::createBool(Bool value) {
  static const ValueBool<false> no;
  static const ValueBool<true> yes;
  return Value(value ? &yes : &no);
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

egg::ovum::Value egg::ovum::ValueFactory::createFlowControl(ValueFlags flags) {
  static const ValueImmutable<ValueFlags::Break> cbreak;
  static const ValueImmutable<ValueFlags::Continue> ccontinue;
  static const ValueImmutable<ValueFlags::Throw> crethrow;
  if (flags == ValueFlags::Break) {
    return Value(&cbreak);
  }
  if (flags == ValueFlags::Continue) {
    return Value(&ccontinue);
  }
  assert(flags == ValueFlags::Throw);
  return Value(&crethrow);
}

egg::ovum::Value egg::ovum::ValueFactory::createFlowControl(IAllocator& allocator, ValueFlags flags, const Value& value) {
  return Value(allocator.make<ValueFlowControl, IValue*>(flags, value));
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

void egg::ovum::Value::print(std::ostream& stream, ValueFlags flags) {
  // This is used by GoogleTest's ::testing::internal::PrintTo
  size_t found = 0;
#define EGG_OVUM_VARIANT_PRINT(name, text) if (Bits::hasAnySet(flags, ValueFlags::name)) { if (++found > 1) { stream << '|'; } stream << #name; }
  EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VARIANT_PRINT)
#undef EGG_OVUM_VARIANT_PRINT
    // cppcheck-suppress knownConditionTrueFalse
    if (found == 0) {
      stream << '(' << int(flags) << ')';
    }
}

void egg::ovum::Value::print(std::ostream& stream, const Value& value) {
  // This is used by GoogleTest's ::testing::internal::PrintTo
  auto flags = value->getFlags();
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
  switch (flags) {
  case egg::ovum::ValueFlags::Void:
    stream << "void";
    return;
  case egg::ovum::ValueFlags::Null:
    stream << "null";
    return;
  case egg::ovum::ValueFlags::Bool: {
    egg::ovum::Bool b;
    if (value->getBool(b)) {
      stream << (b ? "true" : "false");
      return;
    }
    break;
  }
  case egg::ovum::ValueFlags::Int: {
    egg::ovum::Int i;
    if (value->getInt(i)) {
      stream << i;
      return;
    }
    break;
  }
  case egg::ovum::ValueFlags::Float: {
    egg::ovum::Float f;
    if (value->getFloat(f)) {
      stream << f;
      return;
    }
    break;
  }
  case egg::ovum::ValueFlags::String: {
    egg::ovum::String s;
    if (value->getString(s)) {
      stream << '"' << s.toUTF8() << '"';
      return;
    }
    break;
  }
  case egg::ovum::ValueFlags::Object: {
    egg::ovum::Object o;
    if (value->getObject(o)) {
      auto* p = o.get();
      stream << "[object 0x" << std::hex << std::setw(sizeof(p) * 2) << std::setfill('0') << p << ']';
      return;
    }
    break;
  }
  case egg::ovum::ValueFlags::Pointer: {
    egg::ovum::Value v;
    if (value->getChild(v)) {
      auto* p = &v.get();
      stream << "[pointer 0x" << std::hex << std::setw(sizeof(p) * 2) << std::setfill('0') << p << ']';
      return;
    }
    break;
  }
  }
  EGG_WARNING_SUPPRESS_SWITCH_END();
  stream << "[BAD ";
  print(stream, value->getFlags());
  stream << ']';
}
