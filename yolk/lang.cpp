#include "yolk/yolk.h"
#include "ovum/utf.h"

namespace {
  using namespace egg::lang;

  egg::ovum::IAllocator& defaultAllocator() {
    static egg::ovum::AllocatorDefault allocator; // WIBBLE
    return allocator;
  }

  bool arithmeticEqual(double a, int64_t b) {
    // TODO
    return a == b;
  }

  std::pair<std::string, int> tagToStringPriority(Discriminator tag) {
    // Adjust the priority of the result if the string has '|' (union) in it
    // This is so that parentheses can be added appropriately to handle "(void|int)*" versus "void|int*"
    auto result = std::make_pair(Value::getTagString(tag), 0);
    if (result.first.find('|') != std::string::npos) {
      result.second--;
    }
    return result;
  }

  IType::AssignmentSuccess canBeAssignedFromSimple(Discriminator lhs, const IType& rtype) {
    assert(lhs != Discriminator::Inferred);
    auto rhs = rtype.getSimpleTypes();
    assert(rhs != Discriminator::Inferred);
    if (rhs == Discriminator::None) {
      // The source is not simple
      return IType::AssignmentSuccess::Never;
    }
    if (Bits::hasAllSet(lhs, rhs)) {
      // The assignment will always work (unless it includes 'void')
      if (Bits::hasAnySet(rhs, Discriminator::Void)) {
        return IType::AssignmentSuccess::Sometimes;
      }
      return IType::AssignmentSuccess::Always;
    }
    if (Bits::hasAnySet(lhs, rhs)) {
      // There's a possibility that the assignment might work
      return IType::AssignmentSuccess::Sometimes;
    }
    if (Bits::hasAnySet(lhs, Discriminator::Float) && Bits::hasAnySet(rhs, Discriminator::Int)) {
      // We allow type promotion int->float
      return IType::AssignmentSuccess::Sometimes;
    }
    return IType::AssignmentSuccess::Never;
  }

  Value promoteAssignmentSimple(IExecution& execution, Discriminator lhs, const Value& rhs) {
    assert(lhs != Discriminator::Inferred);
    assert(!rhs.has(Discriminator::Indirect));
    if (rhs.has(lhs)) {
      // It's an exact type match (narrowing)
      return rhs;
    }
    if (Bits::hasAnySet(lhs, Discriminator::Float) && rhs.is(Discriminator::Int)) {
      // We allow type promotion int->float
      return Value(double(rhs.getInt())); // TODO overflows?
    }
    return execution.raiseFormat("Cannot assign a value of type '", rhs.getRuntimeType()->toString(), "' to a target of type '", Value::getTagString(lhs), "'");
  }

  void formatSourceLocation(egg::ovum::StringBuilder& sb, const LocationSource& location) {
    sb.add(location.file);
    if (location.column > 0) {
      sb.add('(', location.line, ',', location.column, ')');
    } else if (location.line > 0) {
      sb.add('(', location.line, ')');
    }
  }

  class TypePointer : public egg::ovum::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypePointer);
  private:
    ITypeRef referenced;
  public:
    TypePointer(egg::ovum::IAllocator& allocator, const IType& referenced)
      : HardReferenceCounted(allocator, 0), referenced(&referenced) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto s = this->referenced->toString(0);
      return std::make_pair(s.toUTF8() + "*", 0);
    }
    virtual egg::lang::Discriminator getSimpleTypes() const override {
      return egg::lang::Discriminator::None;
    }
    virtual ITypeRef pointeeType() const override {
      return this->referenced;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return this->referenced->canBeAssignedFrom(*rhs.pointeeType());
    }
  };

  class TypeNull : public egg::ovum::NotReferenceCounted<IType>{
  public:
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      static std::string name{ "null" };
      return std::make_pair(name, 0);
    }
    virtual Discriminator getSimpleTypes() const override {
      return Discriminator::Null;
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto simple = other.getSimpleTypes();
      if (Bits::hasAnySet(simple, Discriminator::Null)) {
        // The other type supports Null anyway
        return ITypeRef(&other);
      }
      return Type::makeUnion(*this, other);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never;
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("Cannot assign to 'null' value");
    }
  };
  const TypeNull typeNull{};

  template<Discriminator TAG>
  class TypeNative : public egg::ovum::NotReferenceCounted<IType> {
  public:
    TypeNative() {
      assert(!Bits::hasAnySet(TAG, Discriminator::Null));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(TAG);
    }
    virtual Discriminator getSimpleTypes() const override {
      return TAG;
    }
    virtual ITypeRef denulledType() const override {
      auto denulled = Bits::clear(TAG, Discriminator::Null);
      if (denulled == TAG) {
        // It's the identical native type
        return ITypeRef(this);
      }
      return ITypeRef(Type::getNative(denulled));
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      if (other.getSimpleTypes() == TAG) {
        // It's the identical native type
        return ITypeRef(this);
      }
      return Type::makeUnion(*this, other);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromSimple(TAG, rhs);
    }
    virtual Value promoteAssignment(IExecution& execution, const Value& rhs) const override {
      return promoteAssignmentSimple(execution, TAG, rhs);
    }
  };
  const TypeNative<Discriminator::Void> typeVoid{};
  const TypeNative<Discriminator::Bool> typeBool{};
  const TypeNative<Discriminator::Int> typeInt{};
  const TypeNative<Discriminator::Float> typeFloat{};
  const TypeNative<Discriminator::Arithmetic> typeArithmetic{};
  const TypeNative<Discriminator::Type> typeType{};

  class TypeString : public TypeNative<Discriminator::String> {
  public:
    // TODO callable and indexable
    virtual bool iterable(ITypeRef& type) const override {
      // When strings are iterated, they iterate through strings (of codepoints)
      type = Type::String;
      return true;
    }
  };
  const TypeString typeString{};

  // An 'omni' function looks like this: 'any?(...any?[])'
  class OmniFunctionSignature : public IFunctionSignature {
  private:
    class Parameter : public IFunctionSignatureParameter {
    public:
      virtual String getName() const override {
        return String();
      }
      virtual ITypeRef getType() const override {
        return Type::AnyQ;
      }
      virtual size_t getPosition() const override {
        return 0;
      }
      virtual Flags getFlags() const override {
        return Flags::Variadic;
      }
    };
    Parameter parameter;
  public:
    virtual String getFunctionName() const override {
      return String();
    }
    virtual ITypeRef getReturnType() const override {
      return Type::AnyQ;
    }
    virtual size_t getParameterCount() const override {
      return 1;
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t) const override {
      return this->parameter;
    }
  };
  const OmniFunctionSignature omniFunctionSignature{};

  class TypeSimple : public egg::ovum::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeSimple);
  private:
    Discriminator tag;
  public:
    TypeSimple(egg::ovum::IAllocator& allocator, Discriminator tag)
      : HardReferenceCounted(allocator, 0), tag(tag) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(this->tag);
    }
    virtual Discriminator getSimpleTypes() const override {
      return this->tag;
    }
    virtual ITypeRef denulledType() const override {
      auto denulled = Bits::clear(this->tag, Discriminator::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return Type::makeSimple(denulled);
      }
      return ITypeRef(this);
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto simple = other.getSimpleTypes();
      if (simple == Discriminator::None) {
        // The other type is not simple
        return Type::makeUnion(*this, other);
      }
      auto both = Bits::set(this->tag, simple);
      if (both != this->tag) {
        // There's a new simple type that we don't support, so create a new type
        return Type::makeSimple(both);
      }
      return ITypeRef(this);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromSimple(this->tag, rhs);
    }
    virtual Value promoteAssignment(IExecution& execution, const Value& rhs) const override {
      return promoteAssignmentSimple(execution, this->tag, rhs);
    }
    virtual const IFunctionSignature* callable() const override {
      if (Bits::hasAnySet(this->tag, Discriminator::Object)) {
        return &omniFunctionSignature;
      }
      return nullptr;
    }
    virtual bool iterable(ITypeRef& type) const override {
      if (Bits::hasAnySet(this->tag, Discriminator::Object)) {
        type = Type::Any;
        return true;
      }
      if (Bits::hasAnySet(this->tag, Discriminator::String)) {
        type = Type::String;
        return true;
      }
      return false;
    }
  };

  class TypeUnion : public egg::ovum::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeUnion);
  private:
    ITypeRef a;
    ITypeRef b;
  public:
    TypeUnion(egg::ovum::IAllocator& allocator, const IType& a, const IType& b)
      : HardReferenceCounted(allocator, 0), a(&a), b(&b) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto sa = this->a->toStringPrecedence().first;
      auto sb = this->b->toStringPrecedence().first;
      return std::make_pair(sa + "|" + sb, -1);
    }
    virtual egg::lang::Discriminator getSimpleTypes() const override {
      return this->a->getSimpleTypes() | this->b->getSimpleTypes();
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never; // TODO
    }
    virtual const IFunctionSignature* callable() const override {
      EGG_THROW("TODO: Cannot yet call to union value"); // TODO
    }
  };

  class ValueOnHeap : public egg::ovum::HardReferenceCounted<egg::lang::ValueReferenceCounted> {
    ValueOnHeap(const ValueOnHeap&) = delete;
    ValueOnHeap& operator=(const ValueOnHeap&) = delete;
  public:
    ValueOnHeap(egg::ovum::IAllocator& allocator, Value&& value) noexcept
      : egg::ovum::HardReferenceCounted<egg::lang::ValueReferenceCounted>(allocator, 0, std::move(value)) {
    }
  };
}

// Native types
const egg::lang::Type egg::lang::Type::Void{ typeVoid };
const egg::lang::Type egg::lang::Type::Null{ typeNull };
const egg::lang::Type egg::lang::Type::Bool{ typeBool };
const egg::lang::Type egg::lang::Type::Int{ typeInt };
const egg::lang::Type egg::lang::Type::Float{ typeFloat };
const egg::lang::Type egg::lang::Type::String{ typeString };
const egg::lang::Type egg::lang::Type::Arithmetic{ typeArithmetic };
const egg::lang::Type egg::lang::Type::Type_{ typeType };
const egg::lang::Type egg::lang::Type::Any{ *defaultAllocator().make<TypeSimple>(egg::lang::Discriminator::Any) };
const egg::lang::Type egg::lang::Type::AnyQ{ *defaultAllocator().make<TypeSimple>(egg::lang::Discriminator::Any | egg::lang::Discriminator::Null) };

// Trivial constant values
const egg::lang::Value egg::lang::Value::Void{ Discriminator::Void };
const egg::lang::Value egg::lang::Value::Null{ Discriminator::Null };
const egg::lang::Value egg::lang::Value::False{ false };
const egg::lang::Value egg::lang::Value::True{ true };
const egg::lang::Value egg::lang::Value::EmptyString{ String() };
const egg::lang::Value egg::lang::Value::Break{ Discriminator::Break };
const egg::lang::Value egg::lang::Value::Continue{ Discriminator::Continue };
const egg::lang::Value egg::lang::Value::Rethrow{ Discriminator::Exception | Discriminator::Void };
const egg::lang::Value egg::lang::Value::ReturnVoid{ Discriminator::Return | Discriminator::Void };

void egg::lang::Value::copyInternals(const Value& other) {
  assert(this != &other);
  this->tag = other.tag;
  if (this->has(Discriminator::Object)) {
    // We can only copy the reference as a hard pointer
    this->tag = Bits::clear(other.tag, Discriminator::Pointer);
    this->o = other.o->hardAcquire<IObject>();
  } else if (this->has(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v = other.v->hardAcquire<ValueReferenceCounted>();
  } else if (this->has(Discriminator::String)) {
    this->s = String::hardAcquire(other.s);
  } else if (this->has(Discriminator::Float)) {
    this->f = other.f;
  } else if (this->has(Discriminator::Int)) {
    this->i = other.i;
  } else if (this->has(Discriminator::Bool)) {
    this->b = other.b;
  } else if (this->has(Discriminator::Type)) {
    this->t = other.t->hardAcquire<IType>();
  }
}

void egg::lang::Value::moveInternals(Value& other) {
  this->tag = other.tag;
  if (this->has(Discriminator::Object)) {
    auto* ptr = other.o;
    if (this->has(Discriminator::Pointer)) {
      // Convert the soft pointer to a hard one
      this->tag = Bits::clear(this->tag, Discriminator::Pointer);
      this->o = ptr->hardAcquire<IObject>();
    } else {
      // Not need to increment/decrement the reference count
      this->o = ptr;
    }
  } else if (this->has(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v = other.v;
  } else if (this->has(Discriminator::String)) {
    this->s = other.s;
  } else if (this->has(Discriminator::Float)) {
    this->f = other.f;
  } else if (this->has(Discriminator::Int)) {
    this->i = other.i;
  } else if (this->has(Discriminator::Bool)) {
    this->b = other.b;
  } else if (this->has(Discriminator::Type)) {
    this->t = other.t;
  }
  other.tag = Discriminator::None;
}

void egg::lang::Value::destroyInternals() {
  if (this->has(Discriminator::Object)) {
    if (!this->has(Discriminator::Pointer)) {
      this->o->hardRelease();
    }
  } else if (this->has(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v->hardRelease();
  } else if (this->has(Discriminator::String)) {
    if (this->s != nullptr) {
      this->s->hardRelease();
    }
  } else if (this->has(Discriminator::Type)) {
    this->t->hardRelease();
  }
}

egg::lang::Value::Value(const ValueReferenceCounted& vrc)
  : tag(Discriminator::Pointer) {
  this->v = vrc.hardAcquire<ValueReferenceCounted>();
}

egg::lang::Value::Value(const Value& value) {
  this->copyInternals(value);
}

egg::lang::Value::Value(Value&& value) noexcept {
  this->moveInternals(value);
}

egg::lang::Value& egg::lang::Value::operator=(const Value& value) {
  if (this != &value) {
    this->destroyInternals();
    this->copyInternals(value);
  }
  return *this;
}

egg::lang::Value& egg::lang::Value::operator=(Value&& value) noexcept {
  if (this != &value) {
    this->destroyInternals();
    this->moveInternals(value);
  }
  return *this;
}

egg::lang::Value::~Value() {
  this->destroyInternals();
}

const egg::lang::Value& egg::lang::Value::direct() const {
  auto* p = this;
  while (p->has(Discriminator::Indirect)) {
    p = p->v;
    assert(p != nullptr);
    assert(!p->has(egg::lang::Discriminator::FlowControl));
  }
  return *p;
}

egg::lang::Value& egg::lang::Value::direct() {
  auto* p = this;
  while (p->has(Discriminator::Indirect)) {
    p = p->v;
    assert(p != nullptr);
    assert(!p->has(egg::lang::Discriminator::FlowControl));
  }
  return *p;
}

egg::lang::ValueReferenceCounted& egg::lang::Value::indirect(egg::ovum::IAllocator& allocator) {
  // Make this value indirect (i.e. heap-based)
  if (!this->has(Discriminator::Indirect)) {
    auto* heap = allocator.create<ValueOnHeap>(0, allocator, std::move(*this));
    assert(this->tag == Discriminator::None); // as a side-effect of the move
    this->tag = Discriminator::Indirect;
    this->v = heap->hardAcquire<ValueReferenceCounted>();
  }
  return *this->v;
}

egg::lang::Value& egg::lang::Value::soft(egg::ovum::ICollectable& container) {
  if (this->has(Discriminator::Object) && !this->has(Discriminator::Pointer)) {
    // This is a hard pointer to an IObject, make it a soft pointer, if possible
    assert(this->o != nullptr);
    if (container.softLink(*this->o)) {
      // Successfully linked in the basket, so release our hard reference
      this->tag = Bits::set(this->tag, Discriminator::Pointer);
      this->o->hardRelease();
    }
  }
  return *this;
}

bool egg::lang::Value::equals(const Value& lhs, const Value& rhs) {
  auto& a = lhs.direct();
  auto& b = rhs.direct();
  if (a.tag != b.tag) {
    // Need to worry about expressions like (0 == 0.0)
    if ((a.tag == Discriminator::Float) && (b.tag == Discriminator::Int)) {
      return arithmeticEqual(a.f, b.i);
    }
    if ((a.tag == Discriminator::Int) && (b.tag == Discriminator::Float)) {
      return arithmeticEqual(b.f, a.i);
    }
    return false;
  }
  if (a.tag == Discriminator::Bool) {
    return a.b == b.b;
  }
  if (a.tag == Discriminator::Int) {
    return a.i == b.i;
  }
  if (a.tag == Discriminator::Float) {
    return a.f == b.f;
  }
  if (a.tag == Discriminator::String) {
    return egg::ovum::IMemory::equals(a.s, b.s);
  }
  if (a.tag == Discriminator::Type) {
    return a.t == b.t;
  }
  if (a.tag == Discriminator::Pointer) {
    return a.v == b.v;
  }
  assert(Bits::hasAnySet(a.tag, Discriminator::Object));
  return a.o == b.o;
}

std::string egg::lang::Value::getTagString() const {
  if (this->tag == Discriminator::Indirect) {
    return this->v->getTagString();
  }
  if (this->tag == Discriminator::Pointer) {
    return this->v->getTagString() + "*";
  }
  return Value::getTagString(this->tag);
}

egg::ovum::String egg::lang::LocationSource::toSourceString() const {
  egg::ovum::StringBuilder sb;
  formatSourceLocation(sb, *this);
  return sb.str();
}

egg::ovum::String egg::lang::LocationRuntime::toRuntimeString() const {
  egg::ovum::StringBuilder sb;
  formatSourceLocation(sb, *this);
  if (!this->function.empty()) {
    if (sb.empty()) {
      sb.add(' ');
    }
    sb.add('<', this->function, '>');
  }
  return sb.str();
}

void egg::lang::Value::softVisitLink(const egg::ovum::ICollectable::Visitor& visitor) const {
  if (this->is(Discriminator::Object | Discriminator::Pointer)) {
    // Soft reference to an object
    assert(this->o != nullptr);
    visitor(*this->o);
  }
}

void egg::lang::Value::addFlowControl(Discriminator bits) {
  assert(Bits::mask(bits, Discriminator::FlowControl) == bits);
  assert(!this->has(Discriminator::FlowControl));
  this->tag = this->tag | bits;
  assert(this->has(Discriminator::FlowControl));
}

bool egg::lang::Value::stripFlowControl(Discriminator bits) {
  assert(Bits::mask(bits, Discriminator::FlowControl) == bits);
  if (Bits::hasAnySet(this->tag, bits)) {
    assert(this->has(egg::lang::Discriminator::FlowControl));
    this->tag = Bits::clear(this->tag, bits);
    assert(!this->has(egg::lang::Discriminator::FlowControl));
    return true;
  }
  return false;
}

std::string egg::lang::Value::getTagString(Discriminator tag) {
  static const egg::yolk::String::StringFromEnum table[] = {
    { int(Discriminator::Any), "any" },
    { int(Discriminator::Void), "void" },
    { int(Discriminator::Bool), "bool" },
    { int(Discriminator::Int), "int" },
    { int(Discriminator::Float), "float" },
    { int(Discriminator::String), "string" },
    { int(Discriminator::Object), "object" },
    { int(Discriminator::Indirect), "indirect" },
    { int(Discriminator::Pointer), "pointer" },
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
    return Value::getTagString(Bits::clear(tag, Discriminator::Null)) + "?";
  }
  return egg::yolk::String::fromEnum(tag, table);
}

egg::lang::ITypeRef egg::lang::Value::getRuntimeType() const {
  assert(this->tag != Discriminator::Indirect);
  if (this->has(Discriminator::Object)) {
    return this->o->getRuntimeType();
  }
  if (this->has(Discriminator::Pointer)) {
    return this->v->getRuntimeType()->pointerType();
  }
  if (this->has(Discriminator::Type)) {
    // TODO Is a type's type itself?
    return ITypeRef(this->t);
  }
  auto* native = Type::getNative(this->tag);
  if (native != nullptr) {
    return ITypeRef(native);
  }
  EGG_THROW("Internal type error: Unknown runtime type");
}

egg::ovum::String egg::lang::Value::toString() const {
  // OPTIMIZE
  if (this->has(Discriminator::Object)) {
    auto str = this->getObject()->toString();
    if (str.tag == Discriminator::String) {
      return str.getString();
    }
    return "<invalid>";
  }
  if (this->has(Discriminator::Type)) {
    return this->t->toString();
  }
  return this->toUTF8();
}

std::string egg::lang::Value::toUTF8() const {
  // OPTIMIZE
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
    return String(this->s).toUTF8();
  }
  if (this->has(Discriminator::Object)) {
    auto str = this->getObject()->toString();
    if (str.tag == Discriminator::String) {
      return str.getString().toUTF8();
    }
    return "<invalid>";
  }
  if (this->tag == Discriminator::Type) {
    return this->t->toString().toUTF8();
  }
  return "<" + Value::getTagString(this->tag) + ">";
}

egg::ovum::String egg::lang::IType::toString(int priority) const {
  auto pair = this->toStringPrecedence();
  if (pair.second < priority) {
    return egg::ovum::StringBuilder::concat("(", pair.first, ")");
  }
  return pair.first;
}

egg::lang::ITypeRef egg::lang::IType::pointerType() const {
  // The default implementation is to return a new type 'Type*'
  return defaultAllocator().make<TypePointer>(*this);
}

egg::lang::ITypeRef egg::lang::IType::pointeeType() const {
  // The default implementation is to return 'Void' indicating that this is NOT dereferencable
  return Type::Void;
}

egg::lang::ITypeRef egg::lang::IType::denulledType() const {
  // The default implementation is to return 'Void'
  return Type::Void;
}

egg::ovum::String egg::lang::IFunctionSignature::toString(Parts parts) const {
  egg::ovum::StringBuilder sb;
  this->buildStringDefault(sb, parts);
  return sb.str();
}

bool egg::lang::IFunctionSignature::validateCall(IExecution& execution, const IParameters& runtime, Value& problem) const {
  // The default implementation just calls validateCallDefault()
  return this->validateCallDefault(execution, runtime, problem);
}

bool egg::lang::IFunctionSignature::validateCallDefault(IExecution& execution, const IParameters& parameters, Value& problem) const {
  // TODO type checking, etc
  if (parameters.getNamedCount() > 0) {
    problem = execution.raiseFormat(this->toString(Parts::All), ": Named parameters are not yet supported"); // TODO
    return false;
  }
  auto maxPositional = this->getParameterCount();
  auto minPositional = maxPositional;
  while ((minPositional > 0) && !this->getParameter(minPositional - 1).isRequired()) {
    minPositional--;
  }
  auto actual = parameters.getPositionalCount();
  if (actual < minPositional) {
    if (minPositional == 1) {
      problem = execution.raiseFormat(this->toString(Parts::All), ": At least 1 parameter was expected");
    } else {
      problem = execution.raiseFormat(this->toString(Parts::All), ": At least ", minPositional, " parameters were expected, not ", actual);
    }
    return false;
  }
  if ((maxPositional > 0) && this->getParameter(maxPositional - 1).isVariadic()) {
    // TODO Variadic
  } else if (actual > maxPositional) {
    // Not variadic
    if (maxPositional == 1) {
      problem = execution.raiseFormat(this->toString(Parts::All), ": Only 1 parameter was expected, not ", actual);
    } else {
      problem = execution.raiseFormat(this->toString(Parts::All), ": No more than ", maxPositional, " parameters were expected, not ", actual);
    }
    return false;
  }
  return true;
}

egg::ovum::String egg::lang::IIndexSignature::toString() const {
  return egg::ovum::StringBuilder::concat(this->getResultType()->toString(), "[", this->getIndexType()->toString(), "]");
}

egg::lang::Value egg::lang::IType::promoteAssignment(IExecution& execution, const Value& rhs) const {
  // The default implementation calls IType::canBeAssignedFrom() but does not actually promote
  auto& direct = rhs.direct();
  auto rtype = direct.getRuntimeType();
  if (this->canBeAssignedFrom(*rtype) == AssignmentSuccess::Never) {
    return execution.raiseFormat("Cannot assign a value of type '", rtype->toString(), "' to a target of type '", this->toString(), "'");
  }
  return direct;
}

const egg::lang::IFunctionSignature* egg::lang::IType::callable() const {
  // The default implementation is to say we don't support calling with '()'
  return nullptr;
}

const egg::lang::IIndexSignature* egg::lang::IType::indexable() const {
  // The default implementation is to say we don't support indexing with '[]'
  return nullptr;
}

bool egg::lang::IType::dotable(const String*, ITypeRef&, String& reason) const {
  // The default implementation is to say we don't support properties with '.'
  reason = egg::ovum::StringBuilder::concat("Values of type '", this->toString(), "' do not support the '.' operator for property access");
  return false;
}

bool egg::lang::IType::iterable(ITypeRef&) const {
  // The default implementation is to say we don't support iteration
  return false;
}

egg::lang::Discriminator egg::lang::IType::getSimpleTypes() const {
  // The default implementation is to say we are an object
  return egg::lang::Discriminator::Object;
}

egg::lang::ITypeRef egg::lang::IType::unionWith(const IType& other) const {
  // The default implementation is to simply make a new union (native and simple types can be more clever)
  return Type::makeUnion(*this, other);
}

const egg::lang::IType* egg::lang::Type::getNative(egg::lang::Discriminator tag) {
  // OPTIMIZE
  if (tag == egg::lang::Discriminator::Void) {
    return &typeVoid;
  }
  if (tag == egg::lang::Discriminator::Null) {
    return &typeNull;
  }
  if (tag == egg::lang::Discriminator::Bool) {
    return &typeBool;
  }
  if (tag == egg::lang::Discriminator::Int) {
    return &typeInt;
  }
  if (tag == egg::lang::Discriminator::Float) {
    return &typeFloat;
  }
  if (tag == egg::lang::Discriminator::String) {
    return &typeString;
  }
  if (tag == egg::lang::Discriminator::Arithmetic) {
    return &typeArithmetic;
  }
  return nullptr;
}

egg::lang::ITypeRef egg::lang::Type::makeSimple(Discriminator simple) {
  // Try to use non-reference-counted globals
  auto* native = Type::getNative(simple);
  if (native != nullptr) {
    return ITypeRef(native);
  }
  return defaultAllocator().make<TypeSimple>(simple);
}

egg::lang::ITypeRef egg::lang::Type::makeUnion(const egg::lang::IType& a, const egg::lang::IType& b) {
  // If they are both simple types, just union the tags
  auto sa = a.getSimpleTypes();
  auto sb = b.getSimpleTypes();
  if ((sa != Discriminator::None) && (sb != Discriminator::None)) {
    return Type::makeSimple(sa | sb);
  }
  return defaultAllocator().make<TypeUnion>(a, b);
}
