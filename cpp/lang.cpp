#include "yolk.h"

namespace {
  class StringBufferUTF8 : public egg::lang::IString {
    EGG_NO_COPY(StringBufferUTF8);
  private:
    std::string utf8;
  public:
    explicit StringBufferUTF8(const std::string& other)
      : utf8(other) {
    }
    virtual size_t length() const override {
      return this->utf8.size(); // TODO UTF8 encoding
    }
    virtual bool empty() const override {
      return this->utf8.empty();
    }
    virtual bool equal(const IString& other) const override {
      auto rhs = other.toUTF8();
      return this->utf8 == rhs;
    }
    virtual bool less(const IString& other) const override {
      auto rhs = other.toUTF8();
      return this->utf8 < rhs;
    }
    virtual std::string toUTF8() const override {
      return this->utf8;
    }
  };

  class StringEmpty : public egg::lang::IString {
    EGG_NO_COPY(StringEmpty);
  public:
    StringEmpty() {
      // Increment our hard reference count so we're never released
      this->acquireHard();
    }
    virtual size_t length() const override {
      return 0;
    }
    virtual bool empty() const override {
      return true;
    }
    virtual bool equal(const IString& other) const override {
      return other.empty();
    }
    virtual bool less(const IString& other) const override {
      return !other.empty();
    }
    virtual std::string toUTF8() const override {
      return std::string();
    }
  };
  const StringEmpty empty{};
}

// Empty constants
const egg::lang::IString& egg::lang::String::emptyBuffer = empty;
const egg::lang::String egg::lang::String::Empty{ empty };

egg::lang::String egg::lang::String::fromUTF8(const std::string& utf8) {
  return String(*new StringBufferUTF8(utf8));
}

egg::lang::String egg::lang::StringBuilder::str() const {
  return String::fromUTF8(this->ss.str());
}

std::ostream& operator<<(std::ostream& os, const egg::lang::String& text) {
  return os << text.toUTF8();
}

const egg::lang::Value egg::lang::Value::Void{ Discriminator::Void };
const egg::lang::Value egg::lang::Value::Null{ Discriminator::Null };
const egg::lang::Value egg::lang::Value::False{ false };
const egg::lang::Value egg::lang::Value::True{ true };
const egg::lang::Value egg::lang::Value::Break{ Discriminator::Break };
const egg::lang::Value egg::lang::Value::Continue{ Discriminator::Continue };
const egg::lang::Value egg::lang::Value::Rethrow{ Discriminator::Exception, new Value{ Discriminator::Void } };

void egg::lang::Value::copyInternals(const Value& other) {
  this->tag = other.tag;
  if (this->is(Discriminator::FlowControl)) {
    this->v = (other.v == nullptr) ? nullptr : new Value(*other.v);
  } else if (this->is(Discriminator::Type | Discriminator::Object)) {
    this->o = other.o->acquire();
  } else if (this->is(Discriminator::String)) {
    this->s = other.s;
    this->s->acquireHard();
  } else if (this->is(Discriminator::Float)) {
    this->f = other.f;
  } else if (this->is(Discriminator::Int)) {
    this->i = other.i;
  } else if (this->is(Discriminator::Bool)) {
    this->b = other.b;
  } else {
    this->v = nullptr;
  }
}

void egg::lang::Value::moveInternals(Value& other) {
  this->tag = other.tag;
  if (this->is(Discriminator::FlowControl)) {
    this->v = other.v;
  } else if (this->is(Discriminator::Type | Discriminator::Object)) {
    this->o = other.o;
  } else if (this->is(Discriminator::String)) {
    this->s = other.s;
  } else if (this->is(Discriminator::Float)) {
    this->f = other.f;
  } else if (this->is(Discriminator::Int)) {
    this->i = other.i;
  } else if (this->is(Discriminator::Bool)) {
    this->b = other.b;
  } else {
    this->v = nullptr;
  }
  other.tag = Discriminator::None;
}

egg::lang::Value::Value(const Value& value) {
  this->copyInternals(value);
}

egg::lang::Value::Value(Value&& value) {
  this->moveInternals(value);
}

egg::lang::Value& egg::lang::Value::operator=(const Value& value) {
  if (this != &value) {
    this->copyInternals(value);
  }
  return *this;
}

egg::lang::Value& egg::lang::Value::operator=(Value&& value) {
  if (this != &value) {
    this->moveInternals(value);
  }
  return *this;
}

egg::lang::Value::~Value() {
  if (this->is(Discriminator::String)) {
    this->s->releaseHard();
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
    return lhs.s->equal(*rhs.s);
  }
  if ((lhs.tag == Discriminator::Type) || (lhs.tag == Discriminator::Object)) {
    return lhs.o == rhs.o;
  }
  return (lhs.v == rhs.v) || ((lhs.v != nullptr) && (rhs.v != nullptr) && Value::equal(*lhs.v, *rhs.v));
}

egg::lang::Value egg::lang::Value::makeFlowControl(egg::lang::Discriminator tag, egg::lang::Value* value) {
  Value result{ tag, value };
  assert(result.is(Discriminator::FlowControl));
  return result;
}
  
egg::lang::Value egg::lang::Value::raise(const egg::lang::String& exception) {
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
