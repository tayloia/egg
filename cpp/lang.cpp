#include "yolk.h"

namespace {
  class StringBufferUTF8 : public egg::gc::HardReferenceCounted<egg::lang::IString> {
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
    inline StringEmpty() {
      // We're not reference counted
    }
    virtual IString* acquireHard() const override {
      return const_cast<StringEmpty*>(this);
    }
    virtual void releaseHard() const override {
      // We're never destroyed
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
  const StringEmpty stringEmpty{};

  class TypeSimple : public egg::lang::IType {
  private:
    egg::lang::Discriminator tag;
    egg::lang::String name;
  public:
    inline TypeSimple(egg::lang::Discriminator tag, const std::string& utf8)
      : tag(tag), name(egg::lang::String::fromUTF8(utf8)) {
    }
    virtual IType* acquireHard() const override {
      // We're not reference-counted
      return const_cast<TypeSimple*>(this);
    }
    virtual void releaseHard() const override {
      // We're not reference-counted
    }
    virtual egg::lang::String toString() const override {
      return this->name;
    }
    virtual bool hasSimpleType(egg::lang::Discriminator) const override {
      EGG_THROW("WIBBLE" __FUNCTION__);
    }
    virtual egg::lang::Discriminator arithmeticTypes() const override {
      return egg::lang::Bits::mask(this->tag, egg::lang::Discriminator::Arithmetic);
    }
    virtual egg::gc::HardRef<const IType> dereferencedType() const override {
      EGG_THROW("WIBBLE" __FUNCTION__);
    }
    virtual egg::gc::HardRef<const IType> nullableType(bool) const override {
      EGG_THROW("WIBBLE" __FUNCTION__);
    }
    virtual egg::gc::HardRef<const IType> unionWith(const IType&) const override {
      EGG_THROW("WIBBLE" __FUNCTION__);
    }
    virtual egg::gc::HardRef<const IType> unionWithSimple(egg::lang::Discriminator) const override {
      EGG_THROW("WIBBLE" __FUNCTION__);
    }
    virtual egg::lang::Value decantParameters(const egg::lang::IParameters&, Setter) const override {
      EGG_THROW("WIBBLE" __FUNCTION__);
    }
    virtual bool canAssignFrom(const IType&, egg::lang::String&) const override {
      EGG_THROW("WIBBLE" __FUNCTION__);
    }
    virtual bool tryAssignFrom(egg::lang::Value&, const egg::lang::Value&, egg::lang::String&) const override {
      EGG_THROW("WIBBLE" __FUNCTION__);
    }
  };
  const TypeSimple typeVoid(egg::lang::Discriminator::Void, "void");
  const TypeSimple typeNull(egg::lang::Discriminator::Null, "null");
}

// Empty constants
const egg::lang::IString& egg::lang::String::emptyBuffer = stringEmpty;
const egg::lang::String egg::lang::String::Empty{ stringEmpty };

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
  } else if (this->is(Discriminator::Type)) {
    this->t = other.t->acquireHard();
  } else if (this->is(Discriminator::Object)) {
    this->o = other.o->acquireHard();
  } else if (this->is(Discriminator::String)) {
    this->s = other.s->acquireHard();
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
  } else if (this->is(Discriminator::Type)) {
    this->t = other.t;
  } else if (this->is(Discriminator::Object)) {
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
  } else if (this->is(Discriminator::Type)) {
    this->t->releaseHard();
  } else if (this->is(Discriminator::Object)) {
    this->o->releaseHard();
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
  if (lhs.tag == Discriminator::Type) {
    return lhs.t == rhs.t;
  }
  if (lhs.tag == Discriminator::Object) {
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

const egg::lang::IType& egg::lang::Value::getRuntimeType() const {
  if (this->tag == Discriminator::Type) {
    // TODO Is the runtime type of a type just the type itself?
    return *this->t;
  }
  if (this->tag == Discriminator::Object) {
    // Ask the object for its type
    auto runtime = this->o->getRuntimeType();
    if (runtime.is(Discriminator::Type)) {
      return *runtime.t;
    }
  }
  EGG_THROW("TODO No egg::lang::Value::getType()"); // TODO
}

std::string egg::lang::Value::toUTF8() const {
  if (this->tag == Discriminator::Null) {
    return "null";
  }
  if (this->tag == Discriminator::Bool) {
    return this->b ? "true" : "false";
  }
  if (this->tag == Discriminator::Int) {
    return egg::yolk::String::fromSigned(this->i);
  }
  if (this->tag == Discriminator::Float) {
    return egg::yolk::String::fromFloat(this->f);
  }
  if (this->tag == Discriminator::String) {
    return this->s->toUTF8();
  }
  if (this->tag == Discriminator::Type) {
    return "[type]"; // TODO
  }
  if (this->tag == Discriminator::Object) {
    auto str = this->o->toString();
    if (str.is(Discriminator::String)) {
      return str.toUTF8();
    }
    return "[invalid]";
  }
  return "[" + Value::getTagString(this->tag) + "]";
}

const egg::lang::Type egg::lang::Type::Void{ typeVoid };
const egg::lang::Type egg::lang::Type::Null{ typeNull };

