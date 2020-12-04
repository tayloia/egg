#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/vanilla.h"

egg::ovum::Slot::Slot(IAllocator& allocator, IBasket& basket)
  : SoftReferenceCounted(allocator),
    ptr(nullptr) {
  basket.take(*this);
  assert(this->validate(true));
}

egg::ovum::Slot::Slot(IAllocator& allocator, IBasket& basket, const Value& value)
  : SoftReferenceCounted(allocator),
    ptr(value->softAcquire()) {
  basket.take(*this);
  assert(this->validate(false));
}

egg::ovum::IValue* egg::ovum::Slot::get() const {
  auto* underlying = this->ptr.get();
  assert((underlying == nullptr) || underlying->validate());
  return underlying;
}

void egg::ovum::Slot::set(const Value& value) {
  assert(this->validate(true));
  auto* before = this->ptr.exchange(value->softAcquire());
  if (before != nullptr) {
    before->softRelease();
  }
  assert(this->validate(false));
}

bool egg::ovum::Slot::update(IValue* expected, const Value& desired) {
  // Update the value iff the current value is 'expected'
  assert(this->validate(true));
  auto* after = desired->softAcquire();
  auto* before = this->ptr.update(expected, after);
  if (before != expected) {
    // The update was not performed
    assert(this->ptr.get() == before); // TODO remove because not thread-safe
    after->softRelease();
    assert(this->validate(true));
    return false;
  }
  assert(this->ptr.get() == after); // TODO remove because not thread-safe
  before->softRelease();
  assert(this->validate(false));
  return true;
}

void egg::ovum::Slot::clear() {
  assert(this->validate(true));
  auto before = this->ptr.exchange(nullptr);
  if (before != nullptr) {
    before->softRelease();
  }
  assert(this->ptr.get() == nullptr); // TODO remove because not thread-safe
}

egg::ovum::Type::Assignment egg::ovum::Slot::mutate(ISlot& slot, IAllocator& allocator, const Type& type, Mutation mutation, const Value& value, Value& before) {
  assert(type != nullptr);
  for (;;) {
    auto* raw = slot.get();
    while (raw == nullptr) {
      // Special case for '=' and '??' applied to initialized slot
      if ((mutation == Mutation::Assign) || (mutation != Mutation::IfNull)) {
        return Type::Assignment::Uninitialized;
      }
      if (slot.update(nullptr, value)) {
        return Type::Assignment::Success;
      }
      raw = slot.get();
    }
    assert(raw->validate());
    before = Value(*raw);
    Value after;
    auto retval = type.mutate(allocator, before, value, mutation, after);
    if (retval != Type::Assignment::Success) {
      return retval;
    }
    if (slot.update(raw, after)) {
      // Successfully updated the slot
      assert(slot.validate());
      return Type::Assignment::Success;
    }
  }
}

egg::ovum::Value egg::ovum::Slot::reference(TypeFactory& factory, IBasket& trug, const Type& pointee, Modifiability modifiability) {
  assert(this->validate(false));
  auto pointer = factory.createPointer(pointee, modifiability);
  return ValueFactory::createObject(allocator, VanillaFactory::createPointer(factory.allocator, trug, *this, pointer));
}

bool egg::ovum::Slot::validate(bool optional) const {
  if (!SoftReferenceCounted::validate()) {
    return false;
  }
  if (this->softGetBasket() == nullptr) {
    return false;
  }
  auto underlying = this->ptr.get();
  if (underlying == nullptr) {
    return optional;
  }
  return underlying->validate();
}

void egg::ovum::Slot::softVisit(const Visitor& visitor) const {
  // WEBBLE
  assert(this->validate(true));
  auto underlying = this->ptr.get();
  if (underlying != nullptr) {
    underlying->softVisit(visitor);
  }
}
