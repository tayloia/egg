#include "ovum/ovum.h"
#include "ovum/slot.h"

egg::ovum::Slot::Slot()
  : ptr(nullptr) {
  assert(this->validate(true));
}

egg::ovum::Slot::Slot(Slot&& rhs) noexcept
  : ptr(rhs.ptr.swap(nullptr)) {
  assert(this->validate(true));
}

egg::ovum::Slot::~Slot() {
  this->clear();
}

egg::ovum::Value egg::ovum::Slot::get() const {
  auto underlying = this->ptr.get();
  assert((underlying != nullptr) && underlying->validate());
  return Value(underlying);
}

void egg::ovum::Slot::set(const Value& value) {
  assert(this->validate(true));
  assert(value.validate());
  auto before = this->ptr.swap(value->softAcquire());
  if (before != nullptr) {
    before->softRelease();
  }
  assert(this->validate(false));
  assert(value.validate());
}

void egg::ovum::Slot::clear() {
  assert(this->validate(true));
  auto before = this->ptr.swap(nullptr);
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
