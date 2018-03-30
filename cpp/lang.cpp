#include "yolk.h"

const egg::lang::Value egg::lang::Value::Void{ Discriminator::Void };
const egg::lang::Value egg::lang::Value::Null{ Discriminator::Null };
const egg::lang::Value egg::lang::Value::Break{ Discriminator::Break };
const egg::lang::Value egg::lang::Value::Continue{ Discriminator::Continue };

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

bool egg::lang::Value::equal(const Value& lhs, const Value& rhs) {
  if (lhs.tag != rhs.tag) {
    return false;
  }
  if (lhs.tag == Discriminator::Bool) {
    return lhs.b == rhs.b;
  }
  if (lhs.tag == Discriminator::Int) {
    return lhs.i == rhs.i;
  }
  if (lhs.tag == Discriminator::Float) {
    return lhs.f == rhs.f;
  }
  if (lhs.tag == Discriminator::String) {
    return *lhs.s == *rhs.s;
  }
  if ((lhs.tag == Discriminator::Type) || (lhs.tag == Discriminator::Object)) {
    return lhs.o == rhs.o;
  }
  return (lhs.v == rhs.v) || ((lhs.v != nullptr) && (rhs.v != nullptr) && Value::equal(*lhs.v, *rhs.v));
}

egg::lang::Value egg::lang::Value::raise(const std::string& exception) {
  return Value(Discriminator::Exception, new Value(exception));
}

std::string egg::lang::Value::getTagString(Discriminator tag) {
  static const egg::yolk::String::StringFromEnum table[] = {
    { int(Discriminator::Any), "any" },
    { int(Discriminator::Void), "void" },
    { int(Discriminator::Bool), "bool" },
    { int(Discriminator::Int), "int" },
    { int(Discriminator::Float), "float" },
    { int(Discriminator::String), "string" },
    { int(Discriminator::Type), "type" },
    { int(Discriminator::Object), "object" },
    { int(Discriminator::Break), "break" },
    { int(Discriminator::Continue), "continue" },
    { int(Discriminator::Return), "return" },
    { int(Discriminator::Yield), "yield" },
    { int(Discriminator::Exception), "exception" }
  };
  if (tag == Discriminator::Inferred) {
    return "var";
  }
  if (tag == Discriminator::Null) {
    return "null";
  }
  if (Bits::hasAnySet(tag, Discriminator::Null)) {
    return egg::yolk::String::fromEnum(Bits::clear(tag, Discriminator::Null), table) + "?";
  }
  return egg::yolk::String::fromEnum(tag, table);
}
