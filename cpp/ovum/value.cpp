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
    virtual bool set(const IValue&) override {
      // Cannot set an immutable instance
      return false;
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
    virtual bool set(const IValue&) override {
      // TODO
      return false;
    }
  };

  class ValueInt final : public ValueMutable {
    ValueInt(const ValueInt&) = delete;
    ValueInt& operator=(const ValueInt&) = delete;
  private:
    Int value;
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
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      HardObject other;
      return rhs.getHardObject(other) && this->value.equals(other);
    }
    virtual bool validate() const override {
      return ValueMutable::validate() && this->value.validate();
    }
    virtual void print(Printer& printer) const override {
      printer.write(this->value);
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
    virtual bool equals(const IValue& rhs, ValueCompare compare) const override {
      HardValue other;
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

  class ValuePoly final : public SoftReferenceCountedAllocator<IValue> {
    ValuePoly(const ValuePoly&) = delete;
    ValuePoly& operator=(const ValuePoly&) = delete;
  private:
    ValueFlags flags;
    union {
      Bool bvalue;
      Int ivalue;
      Float fvalue;
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
        value = this->bvalue;
        return true;
      }
      return false;
    }
    virtual bool getInt(Int& value) const override {
      assert(this->validate());
      if (this->flags == ValueFlags::Int) {
        value = this->ivalue;
        return true;
      }
      return false;
    }
    virtual bool getFloat(Float& value) const override {
      assert(this->validate());
      if (this->flags == ValueFlags::Float) {
        value = this->fvalue;
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
    virtual bool equals(const IValue& rhs, ValueCompare) const override {
      // TODO
      return this == &rhs;
    }
    virtual void print(Printer& printer) const override {
      // TODO
      printer.write(this->flags);
    }
    bool validate() const {
      // TODO
      if (this->flags == ValueFlags::Object) {
        auto* object = this->ovalue;
        return (object != nullptr) && (this->basket != nullptr) && (object->softGetBasket() == this->basket);
      }
      return true;
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
          this->bvalue = b;
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
          this->ivalue = i;
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
          this->fvalue = f;
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
  private:
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

bool egg::ovum::SoftKey::operator<(const SoftKey& rhs) const {
  return &this->ptr < &rhs.ptr; // WIBBLE
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
