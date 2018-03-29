#include "yolk.h"

const egg::lang::Value egg::lang::Value::Void{ Discriminator::Void };
const egg::lang::Value egg::lang::Value::Null{ Discriminator::Null };

namespace {
  void cloneValue(egg::lang::Value& dst, const egg::lang::Value& src) {
    std::memcpy(&dst, &src, sizeof(egg::lang::Value));
  }
  void zeroValue(egg::lang::Value& dst) {
    std::memset(&dst, 0, sizeof(egg::lang::Value));
  }
}

void egg::lang::Value::addref() {
  if (this->is(Discriminator::String)) {
    this->s = new std::string(*this->s);
  } else if (this->is(Discriminator::Type | Discriminator::Object)) {
    this->o = this->o->acquire();
  } else if (this->is(Discriminator::FlowControl)) {
    this->v = new Value(this->v);
  }
}

egg::lang::Value::Value(const Value& value) {
  cloneValue(*this, value);
  this->addref();
}

egg::lang::Value::Value(Value&& value) {
  cloneValue(*this, value);
  zeroValue(value);
}

egg::lang::Value& egg::lang::Value::operator=(const Value& value) {
  cloneValue(*this, value);
  this->addref();
  return *this;
}

egg::lang::Value& egg::lang::Value::operator=(Value&& value) {
  cloneValue(*this, value);
  zeroValue(value);
  return *this;
}

egg::lang::Value::~Value() {
  if (this->is(Discriminator::String)) {
    delete this->s;
  } else if (this->is(Discriminator::Type | Discriminator::Object)) {
    this->o->release();
  } else if (this->is(Discriminator::FlowControl)) {
    delete this->v;
  }
}
