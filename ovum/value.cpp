#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/vanilla.h"

// WYBBLE
#include <iostream>

namespace {
  using namespace egg::ovum;

  template<typename T, typename... ARGS>
  Value makeValue(IAllocator& allocator, ARGS&&... args) {
    return Value(*static_cast<IValue*>(allocator.makeRaw<T>(allocator, std::forward<ARGS>(args)...)));
  }

  template<ValueFlags FLAGS>
  class ValueImmutable : public NotHardReferenceCounted<IValue> {
    ValueImmutable(const ValueImmutable&) = delete;
    ValueImmutable& operator=(const ValueImmutable&) = delete;
  public:
    ValueImmutable() {}
    virtual IValue* softAcquire(IBasket&) const override {
      printf("WYBBLE: %p %s\n", this, __FUNCTION__);
      return const_cast<ValueImmutable*>(this);
    }
    virtual void softRelease() const override {
      printf("WYBBLE: %p %s\n", this, __FUNCTION__);
      // Do nothing (no dynamic memory to reclaim)
    }
    virtual void softVisit(const ICollectable::Visitor&) const override {
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
    virtual bool getObject(Object&) const override {
      return false;
    }
    virtual bool getInner(Value&) const override {
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
    virtual void print(Printer& printer) const override {
      printer.write(FLAGS);
    }
    virtual bool validate() const override {
      return true;
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
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      Bool value;
      return rhs.getBool(value) && (value == VALUE);
    }
    virtual void print(Printer& printer) const override {
      printer.write(VALUE);
    }
  };
  const ValueBool<false> theFalse;
  const ValueBool<true> theTrue;

  class ValueMutable : public HardReferenceCounted<IValue> {
    ValueMutable(const ValueMutable&) = delete;
    ValueMutable& operator=(const ValueMutable&) = delete;
  public:
    explicit ValueMutable(IAllocator& allocator, decltype(atomic)::Underlying atomic = 0) : HardReferenceCounted(allocator, atomic) {
    }
    // Inherited via IValue
    virtual IValue* softAcquire(IBasket&) const override {
      printf("WYBBLE: %p %s\n", this, __FUNCTION__);
      return this->hardAcquire();
    }
    virtual void softRelease() const override {
      printf("WYBBLE: %p %s\n", this, __FUNCTION__);
      this->hardRelease();
    }
    virtual void softVisit(const ICollectable::Visitor&) const override {
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
    virtual bool getObject(Object&) const override {
      return false;
    }
    virtual bool getInner(Value&) const override {
      return false;
    }
    virtual bool validate() const override {
      // Assume all values are valid
      return this->atomic.get() >= 0;
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
    virtual void print(Printer& printer) const override {
      printer.write(this->value);
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
    virtual void print(Printer& printer) const override {
      printer.write(this->value);
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
    virtual bool validate() const override {
      return ValueMutable::validate() && this->value.validate();
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->value);
    }
  };

  class ValueObjectSoft final : public ValueMutable {
    ValueObjectSoft(const ValueObjectSoft&) = delete;
    ValueObjectSoft& operator=(const ValueObjectSoft&) = delete;
  private:
    IObject* object;
    using Delta = decltype(atomic)::Underlying;
    const Delta DELTA_SOFT = 1;
    const Delta DELTA_HARD = DELTA_SOFT << (sizeof(Delta) * 4);
  public:
    ValueObjectSoft(IAllocator& allocator, IBasket& basket, const Object& object)
      : ValueMutable(allocator),
      object(object.get()) {
      this->delta(+DELTA_SOFT);
      assert(this->validate());
      assert(this->object->softGetBasket() == &basket);
      (void)basket; // WYBBLE remove basket from signature
      printf("WYBBLE: CONSTRUCT ValueObjectSoft %p(%p) = %s = ", this, this->object, typeid(*this).name());
      Printer printer{ std::cout, Print::Options::DEFAULT };
      this->object->print(printer);
      putchar('\n');
    }
    virtual ~ValueObjectSoft() {
      printf("WYBBLE: DESTRUCT ValueObjectSoft %p(%p) = %s\n", this, this->object, typeid(*this).name());
    }
    virtual IValue* hardAcquire() const override {
      auto count = this->delta(+DELTA_HARD);
      assert(count >= DELTA_HARD);
      (void)count;
      printf("WYBBLE: %p %s -> %u\n", this, __FUNCTION__, unsigned(count));
      return const_cast<ValueObjectSoft*>(this);
    }
    virtual void hardRelease() const override {
      auto count = this->delta(-DELTA_HARD);
      assert(count >= 0);
      (void)count;
      printf("WYBBLE: %p %s -> %u\n", this, __FUNCTION__, unsigned(count));
      if (count == 0) {
        this->allocator.destroy(this);
      }
    }
    virtual IValue* softAcquire(IBasket&) const override {
      auto count = this->delta(+DELTA_SOFT);
      assert(count >= DELTA_SOFT);
      (void)count;
      printf("WYBBLE: %p %s -> %u\n", this, __FUNCTION__, unsigned(count));
      return const_cast<ValueObjectSoft*>(this);
    }
    virtual void softRelease() const override {
      auto count = this->delta(-DELTA_SOFT);
      assert(count >= 0);
      (void)count;
      printf("WYBBLE: %p %s -> %u\n", this, __FUNCTION__, unsigned(count));
      // The object we're pointing to has a lifetime managed by the basket, so just reclaim our memory
      if (count == 0) {
        this->allocator.destroy(this);
      }
    }
    virtual void softVisit(const ICollectable::Visitor& visitor) const override {
      assert(this->validate());
      this->object->softVisit(visitor);
    }
    virtual ValueFlags getFlags() const override {
      assert(this->validate());
      return ValueFlags::Object;
    }
    virtual Type getRuntimeType() const override {
      assert(this->validate());
      return this->object->getRuntimeType();
    }
    virtual bool getObject(Object& result) const override {
      assert(this->validate());
      result.set(this->object);
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      assert(this->validate());
      // TODO: Identity is not appropriate for pointers
      Object other{ *this->object };
      return rhs.getObject(other) && (this->object == other.get());
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && (this->object != nullptr) && this->object->validate();
    }
    virtual void print(Printer& printer) const override {
      assert(this->validate());
      this->object->print(printer);
    }
  private:
    Delta delta(Delta offset) const {
      return this->atomic.add(offset) + offset;
    }
  };

  class ValueObjectHard final : public ValueMutable {
    ValueObjectHard(const ValueObjectHard&) = delete;
    ValueObjectHard& operator=(const ValueObjectHard&) = delete;
  private:
    Object object; // never null
  public:
    ValueObjectHard(IAllocator& allocator, const Object& object)
      : ValueMutable(allocator),
        object(*object) {
      assert(this->validate());
      printf("WYBBLE: CONSTRUCT ValueObjectHard %p(%p) = %s = ", this, this->object.get(), typeid(*this).name());
      Printer printer{ std::cout, Print::Options::DEFAULT };
      this->object->print(printer);
      putchar('\n');
    }
    virtual ~ValueObjectHard() {
      printf("WYBBLE: DESTRUCT ValueObjectHard %p(%p) = %s\n", this, this->object.get(), typeid(*this).name());
    }
    virtual IValue* softAcquire(IBasket& basket) const override {
      printf("WYBBLE: %p %s\n", this, __FUNCTION__);
      return this->allocator.makeRaw<ValueObjectSoft>(this->allocator, basket, this->object);
    }
    virtual void softRelease() const override {
      printf("WYBBLE: %p %s\n", this, __FUNCTION__);
      assert(false); // WYBBLE
    }
    virtual void softVisit(const ICollectable::Visitor&) const override {
      assert(false); // WYBBLE
    }
    virtual ValueFlags getFlags() const override {
      assert(this->validate());
      return ValueFlags::Object;
    }
    virtual Type getRuntimeType() const override {
      assert(this->validate());
      return object->getRuntimeType();
    }
    virtual bool getObject(Object& result) const override {
      assert(this->validate());
      result.set(this->object.get());
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      assert(this->validate());
      // TODO: Identity is not appropriate for pointers
      Object other{ *this->object };
      return rhs.getObject(other) && (this->object.get() == other.get());
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && (this->object != nullptr) && this->object->validate();
    }
    virtual void print(Printer& printer) const override {
      assert(this->validate());
      this->object->print(printer);
    }
  };

  class ValueFlowControl final : public ValueMutable {
    ValueFlowControl(const ValueFlowControl&) = delete;
    ValueFlowControl& operator=(const ValueFlowControl&) = delete;
  private:
    ValueFlags flags;
    Value inner;
  public:
    ValueFlowControl(IAllocator& allocator, ValueFlags flags, const Value& child) : ValueMutable(allocator), flags(flags), inner(child) {
      assert(this->validate());
    }
    virtual void softVisit(const ICollectable::Visitor& visitor) const override {
      this->inner->softVisit(visitor);
    }
    virtual ValueFlags getFlags() const override {
      return this->flags | this->inner->getFlags();
    }
    virtual Type getRuntimeType() const override {
      // We should NOT really be asked for type information here
      assert(false);
      return Type::Void;
    }
    virtual bool getInner(Value& value) const override {
      value = this->inner;
      return true;
    }
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      Value other;
      return (rhs.getFlags() == this->flags) && rhs.getInner(other) && this->inner->equals(other.get(), compare);
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && Bits::hasAnySet(this->flags, ValueFlags::FlowControl) && this->inner.validate();
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->flags);
      printer << ' ';
      printer.write(this->inner);
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

const egg::ovum::Value egg::ovum::Value::Void{ theVoid.instance() };
const egg::ovum::Value egg::ovum::Value::Null{ theNull.instance() };
const egg::ovum::Value egg::ovum::Value::False{ theFalse.instance() };
const egg::ovum::Value egg::ovum::Value::True{ theTrue.instance() };
const egg::ovum::Value egg::ovum::Value::Break{ theBreak.instance() };
const egg::ovum::Value egg::ovum::Value::Continue{ theContinue.instance() };
const egg::ovum::Value egg::ovum::Value::Rethrow{ theRethrow.instance() };

egg::ovum::Value::Value() : ptr(&theVoid) {
  assert(this->validate());
}

egg::ovum::Value egg::ovum::ValueFactory::createInt(IAllocator& allocator, Int value) {
  return makeValue<ValueInt>(allocator, value);
}

egg::ovum::Value egg::ovum::ValueFactory::createFloat(IAllocator& allocator, Float value) {
  return makeValue<ValueFloat>(allocator, value);
}

egg::ovum::Value egg::ovum::ValueFactory::createString(IAllocator& allocator, const String& value) {
  return makeValue<ValueString>(allocator, value);
}

egg::ovum::Value egg::ovum::ValueFactory::createObject(IAllocator& allocator, const Object& value) {
  return makeValue<ValueObjectHard>(allocator, value);
}

egg::ovum::Value egg::ovum::ValueFactory::createFlowControl(IAllocator& allocator, ValueFlags flags, const Value& value) {
  return makeValue<ValueFlowControl>(allocator, flags, value);
}

std::string egg::ovum::Value::readable() const {
  auto p = this->ptr.get();
  if (p != nullptr) {
    std::stringstream oss;
    Print::write(oss, *this, Print::Options::DEFAULT);
    return oss.str();
  }
  return {};
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
