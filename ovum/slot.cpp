#include "ovum/ovum.h"

egg::ovum::Slot::Slot() : ptr(nullptr) {
  assert(this->validate());
}

egg::ovum::Slot::Slot(const Value& value) : ptr(value->softAcquire()) {
  assert(this->validate());
}

egg::ovum::Slot::~Slot() {
  assert(this->validate());
  auto* before = this->ptr.swap(nullptr);
  if (before != nullptr) {
    before->softRelease();
  }
}

egg::ovum::Value egg::ovum::Slot::get() const {
  // Return 'Void' if empty
  assert(this->validate());
  auto* value = this->ptr.get();
  if (value == nullptr) {
    return Value();
  }
  return Value(value);
}

void egg::ovum::Slot::set(const egg::ovum::Value& value) {
  assert(this->validate());
  assert(value.validate());
  auto* before = this->ptr.swap(value->softAcquire());
  if (before != nullptr) {
    before->softRelease();
  }
  assert(this->validate());
  assert(value.validate());
}

void egg::ovum::Slot::softVisitLink(const ICollectable::Visitor&) const {
  assert(this->validate());
}

bool egg::ovum::Slot::validate() const {
  auto* value = ptr.get();
  if (value != nullptr) {
    return value->validate();
  }
  return true;
}
