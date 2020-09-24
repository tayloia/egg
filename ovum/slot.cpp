#include "ovum/ovum.h"
#include "ovum/slot.h"

egg::ovum::Slot::Slot(IAllocator& allocator)
  : SoftReferenceCounted(allocator), ptr(nullptr) {
  assert(this->validate(true));
}

egg::ovum::Slot::Slot(Slot&& rhs) noexcept
  : SoftReferenceCounted(rhs.allocator), ptr(rhs.ptr.exchange(nullptr)) {
  assert(this->validate(true));
}

egg::ovum::Slot::~Slot() {
  this->clear();
}

egg::ovum::Value egg::ovum::Slot::getValue() const {
  auto* underlying = this->ptr.get();
  assert((underlying != nullptr) && underlying->validate());
  return Value(*underlying);
}

const egg::ovum::IValue* egg::ovum::Slot::getReference() const {
  auto* underlying = this->ptr.get();
  assert((underlying == nullptr) || underlying->validate());
  return underlying;
}

egg::ovum::IValue* egg::ovum::Slot::getReference() {
  auto underlying = this->ptr.get();
  assert((underlying == nullptr) || underlying->validate());
  return underlying;
}

void egg::ovum::Slot::clobber(const Value& value) {
  // Unconditionally update the value
  assert(this->validate(true));
  auto before = this->ptr.exchange(value->softAcquire());
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

egg::ovum::Error egg::ovum::Slot::mutate(const IType& type, Mutation mutation, const Value& value) {
  assert(this->validate(true));
  assert(value.validate());
  return type.tryMutate(this->allocator, *this, mutation, value);
}

void egg::ovum::Slot::clear() {
  assert(this->validate(true));
  auto before = this->ptr.exchange(nullptr);
  if (before != nullptr) {
    before->softRelease();
  }
  assert(this->ptr.get() == nullptr);
}

bool egg::ovum::Slot::validate(bool optional) const {
  auto underlying = this->ptr.get();
  if (underlying == nullptr) {
    return optional;
  }
  return underlying->validate();
}

void egg::ovum::Slot::softVisitLinks(const Visitor&) const {
  // WIBBLE
}
