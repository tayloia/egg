#include "ovum/ovum.h"
#include "ovum/slot.h"

egg::ovum::Slot::Slot(IAllocator& allocator)
  : SoftReferenceCounted(allocator), ptr(nullptr) {
  assert(this->validate(true));
}

egg::ovum::Slot::Slot(IAllocator& allocator, const Value& value)
  : SoftReferenceCounted(allocator), ptr(value->softAcquire()) {
  assert(this->validate(false));
}

egg::ovum::Slot::Slot(Slot&& rhs) noexcept
  : SoftReferenceCounted(rhs.allocator), ptr(rhs.ptr.exchange(nullptr)) {
  assert(this->validate(true));
}

egg::ovum::Slot::~Slot() {
  this->clear();
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

egg::ovum::Type::Assignment egg::ovum::Slot::assign(const Type& type, const Value& value, Value& after) {
  assert(type != nullptr);
  assert(this->validate(true));
  auto retval = type.promote(this->allocator, value, after);
  if (retval == Type::Assignment::Success) {
    this->set(after);
  }
  return retval;
}

egg::ovum::Type::Assignment egg::ovum::Slot::mutate(const Type& type, Mutation mutation, const Value& value, Value& before) {
  assert(type != nullptr);
  assert(this->validate(false));
  for (;;) {
    auto* raw = this->get();
    while (raw == nullptr) {
      // Special case for '??' applied to initialized slot
      if (mutation != Mutation::IfNull) {
        return Type::Assignment::Uninitialized;
      }
      if (this->update(nullptr, value)) {
        return Type::Assignment::Success;
      }
      raw = this->get();
    }
    before = Value(*raw);
    Value after;
    auto retval = type.mutate(this->allocator, before, value, mutation, after);
    if (retval != Type::Assignment::Success) {
      return retval;
    }
    if (this->update(raw, after)) {
      // Successfully updated the slot
      assert(this->validate(true));
      return Type::Assignment::Success;
    }
  }
}

bool egg::ovum::Slot::validate(bool optional) const {
  if (!SoftReferenceCounted::validate()) {
    return false;
  }
  auto underlying = this->ptr.get();
  if (underlying == nullptr) {
    return optional;
  }
  return underlying->validate();
}

void egg::ovum::Slot::softVisitLinks(const Visitor&) const {
  // WIBBLE
}