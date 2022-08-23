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

egg::ovum::Slot::~Slot() {
  // Like 'clear()' but without validation
  auto before = this->ptr.exchange(nullptr);
  if (before != nullptr) {
    before->softRelease();
  }
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

egg::ovum::Type::Assignment egg::ovum::Slot::mutate(const Type& type, Mutation mutation, const Value& value, Value& before) {
  return Slot::mutate(*this, this->allocator, type, mutation, value, before);
}

egg::ovum::Value egg::ovum::Slot::value(const Value& empty) const {
  auto* value = this->get();
  return (value == nullptr) ? empty : Value(*value);
}

egg::ovum::Value egg::ovum::Slot::reference(ITypeFactory& factory, IBasket& trug, const Type& pointee, Modifiability modifiability) {
  assert(this->validate(false));
  auto pointer = factory.createPointer(pointee, modifiability);
  return ValueFactory::createObject(allocator, VanillaFactory::createPointer(factory.getAllocator(), trug, *this, pointer, modifiability));
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
  assert(this->validate(true));
  auto underlying = this->ptr.get();
  if (underlying != nullptr) {
    underlying->softVisit(visitor);
  }
}

void egg::ovum::Slot::print(Printer& printer) const {
  auto underlying = this->ptr.get();
  if (underlying != nullptr) {
    printer << "SLOT: ";
    underlying->print(printer);
  } else {
    printer << "SLOT: <empty>";
  }
}

egg::ovum::SlotArray::SlotArray(size_t size)
  : vec(size) {
}

bool egg::ovum::SlotArray::empty() const {
  return this->vec.empty();
}

size_t egg::ovum::SlotArray::length() const {
  return this->vec.size();
}

egg::ovum::Slot* egg::ovum::SlotArray::get(size_t index) const {
  // Returns the address of the slot or nullptr if not present
  if (index < this->vec.size()) {
    return this->vec[index].get();
  }
  return nullptr;
}

egg::ovum::Slot* egg::ovum::SlotArray::set(IAllocator& allocator, IBasket& basket, size_t index, const Value& value) {
  // Updates a slot
  if (index < this->vec.size()) {
    auto& slot = this->vec[index];
    if (slot != nullptr) {
      slot->set(value);
    } else {
      slot.set(basket, allocator.makeRaw<Slot>(allocator, basket, value));
    }
    return slot.get();
  }
  return nullptr;
}

void egg::ovum::SlotArray::resize(IAllocator& allocator, IBasket& basket, size_t size, const Value& fill) {
  auto before = this->vec.size();
  this->vec.resize(size);
  while (before < size) {
    this->vec[before++].set(basket, allocator.makeRaw<Slot>(allocator, basket, fill));
  }
}

void egg::ovum::SlotArray::foreach(std::function<void(const Slot* slot)> visitor) const {
  // Iterate in order
  for (auto& entry : this->vec) {
    visitor(entry.get());
  }
}

void egg::ovum::SlotArray::softVisit(const ICollectable::Visitor& visitor) const {
  for (auto& entry : this->vec) {
    entry.visit(visitor);
  }
}

egg::ovum::ISlot& egg::ovum::SlotFactory::createSlot(IAllocator& allocator, IBasket& basket) {
  auto* slot = allocator.makeRaw<Slot>(allocator, basket);
  assert(slot != nullptr);
  return *slot;
}

egg::ovum::ISlot& egg::ovum::SlotFactory::createSlot(IAllocator& allocator, IBasket& basket, const Value& value) {
  auto* slot = allocator.makeRaw<Slot>(allocator, basket, value);
  assert(slot != nullptr);
  return *slot;
}
