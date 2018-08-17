#include "yolk/yolk.h"
#include "ovum/utf.h"

namespace {
  using namespace egg::lang;

  // Forward declaration
  const IType* getNativeBasal(Basal basal);

  egg::ovum::IAllocator& defaultAllocator() {
    static egg::ovum::AllocatorDefault allocator; // WIBBLE
    return allocator;
  }

  bool arithmeticEqual(double a, int64_t b) {
    // TODO
    return a == b;
  }

  std::pair<std::string, int> tagToStringPriority(Basal basal) {
    // Adjust the priority of the result if the string has '|' (union) in it
    // This is so that parentheses can be added appropriately to handle "(void|int)*" versus "void|int*"
    auto result = std::make_pair(Value::getBasalString(basal), 0);
    if (result.first.find('|') != std::string::npos) {
      result.second--;
    }
    return result;
  }

  IType::AssignmentSuccess canBeAssignedFromBasal(Basal lhs, const IType& rtype) {
    assert(lhs != Basal::None);
    auto rhs = rtype.getBasalTypes();
    assert(rhs != Basal::None);
    if (rhs == Basal::None) { // WIBBLE
      // The source is not basal
      return IType::AssignmentSuccess::Never;
    }
    if (egg::ovum::Bits::hasAllSet(lhs, rhs)) {
      // The assignment will always work (unless it includes 'void')
      if (egg::ovum::Bits::hasAnySet(rhs, Basal::Void)) {
        return IType::AssignmentSuccess::Sometimes;
      }
      return IType::AssignmentSuccess::Always;
    }
    if (egg::ovum::Bits::hasAnySet(lhs, rhs)) {
      // There's a possibility that the assignment might work
      return IType::AssignmentSuccess::Sometimes;
    }
    if (egg::ovum::Bits::hasAnySet(lhs, Basal::Float) && egg::ovum::Bits::hasAnySet(rhs, Basal::Int)) {
      // We allow type promotion int->float
      return IType::AssignmentSuccess::Sometimes;
    }
    return IType::AssignmentSuccess::Never;
  }

  Value promoteAssignmentBasal(egg::ovum::IExecution& execution, Basal lhs, const Value& rhs) {
    assert(lhs != Basal::None);
    assert(!rhs.hasIndirect());
    if (rhs.hasBasal(lhs)) {
      // It's an exact type match (narrowing)
      return rhs;
    }
    if (egg::ovum::Bits::hasAnySet(lhs, Basal::Float) && rhs.isInt()) {
      // We allow type promotion int->float
      return Value(double(rhs.getInt())); // TODO overflows?
    }
    return execution.raiseFormat("Cannot assign a value of type '", rhs.getRuntimeType()->toString(), "' to a target of type '", Value::getBasalString(lhs), "'");
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
    virtual egg::lang::Basal getBasalTypes() const override {
      return egg::lang::Basal::None;
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
    virtual Basal getBasalTypes() const override {
      return Basal::Null;
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto basal = other.getBasalTypes();
      if (egg::ovum::Bits::hasAnySet(basal, Basal::Null)) {
        // The other type supports Null anyway
        return ITypeRef(&other);
      }
      return Type::makeUnion(*this, other);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never;
    }
    virtual Value promoteAssignment(egg::ovum::IExecution& execution, const Value&) const override {
      return execution.raiseFormat("Cannot assign to 'null' value");
    }
  };
  const TypeNull typeNull{};

  template<Basal BASAL>
  class TypeNative : public egg::ovum::NotReferenceCounted<IType> {
  public:
    TypeNative() {
      assert(!egg::ovum::Bits::hasAnySet(BASAL, Basal::Null));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(BASAL);
    }
    virtual Basal getBasalTypes() const override {
      return BASAL;
    }
    virtual ITypeRef denulledType() const override {
      auto denulled = egg::ovum::Bits::clear(BASAL, Basal::Null);
      if (denulled == BASAL) {
        // It's the identical native type
        return ITypeRef(this);
      }
      return ITypeRef(getNativeBasal(denulled));
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      if (other.getBasalTypes() == BASAL) {
        // It's the identical native type
        return ITypeRef(this);
      }
      return Type::makeUnion(*this, other);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromBasal(BASAL, rhs);
    }
    virtual Value promoteAssignment(egg::ovum::IExecution& execution, const Value& rhs) const override {
      return promoteAssignmentBasal(execution, BASAL, rhs);
    }
  };
  const TypeNative<Basal::Void> typeVoid{};
  const TypeNative<Basal::Bool> typeBool{};
  const TypeNative<Basal::Int> typeInt{};
  const TypeNative<Basal::Float> typeFloat{};
  const TypeNative<Basal::Arithmetic> typeArithmetic{};
  const TypeNative<Basal::Type> typeType{};

  class TypeString : public TypeNative<Basal::String> {
  public:
    // TODO callable and indexable
    virtual bool iterable(ITypeRef& type) const override {
      // When strings are iterated, they iterate through strings (of codepoints)
      type = Type::String;
      return true;
    }
  };
  const TypeString typeString{};

  const IType* getNativeBasal(Basal basal) {
    // OPTIMIZE
    if (basal == egg::lang::Basal::Void) {
      return &typeVoid;
    }
    if (basal == egg::lang::Basal::Null) {
      return &typeNull;
    }
    if (basal == egg::lang::Basal::Bool) {
      return &typeBool;
    }
    if (basal == egg::lang::Basal::Int) {
      return &typeInt;
    }
    if (basal == egg::lang::Basal::Float) {
      return &typeFloat;
    }
    if (basal == egg::lang::Basal::String) {
      return &typeString;
    }
    if (basal == egg::lang::Basal::Arithmetic) {
      return &typeArithmetic;
    }
    return nullptr;
  }

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

  class TypeBasal : public egg::ovum::HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeBasal);
  private:
    Basal tag;
  public:
    TypeBasal(egg::ovum::IAllocator& allocator, Basal basal)
      : HardReferenceCounted(allocator, 0), tag(basal) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(this->tag);
    }
    virtual Basal getBasalTypes() const override {
      return this->tag;
    }
    virtual ITypeRef denulledType() const override {
      auto denulled = egg::ovum::Bits::clear(this->tag, Basal::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return Type::makeBasal(denulled);
      }
      return ITypeRef(this);
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto basal = other.getBasalTypes();
      if (basal == Basal::None) {
        // The other type is not basal
        return Type::makeUnion(*this, other);
      }
      auto both = egg::ovum::Bits::set(this->tag, basal);
      if (both != this->tag) {
        // There's a new basal type that we don't support, so create a new type
        return Type::makeBasal(both);
      }
      return ITypeRef(this);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromBasal(this->tag, rhs);
    }
    virtual Value promoteAssignment(egg::ovum::IExecution& execution, const Value& rhs) const override {
      return promoteAssignmentBasal(execution, this->tag, rhs);
    }
    virtual const IFunctionSignature* callable() const override {
      if (egg::ovum::Bits::hasAnySet(this->tag, Basal::Object)) {
        return &omniFunctionSignature;
      }
      return nullptr;
    }
    virtual bool iterable(ITypeRef& type) const override {
      if (egg::ovum::Bits::hasAnySet(this->tag, Basal::Object)) {
        type = Type::Any;
        return true;
      }
      if (egg::ovum::Bits::hasAnySet(this->tag, Basal::String)) {
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
    virtual Basal getBasalTypes() const override {
      return this->a->getBasalTypes() | this->b->getBasalTypes();
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
const egg::lang::Type egg::lang::Type::Any{ *defaultAllocator().make<TypeBasal>(egg::lang::Basal::Any) };
const egg::lang::Type egg::lang::Type::AnyQ{ *defaultAllocator().make<TypeBasal>(egg::lang::Basal::Any | egg::lang::Basal::Null) };

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
  if (this->hasObject()) {
    // We can only copy the reference as a hard pointer
    this->tag = egg::ovum::Bits::clear(other.tag, Discriminator::Pointer);
    this->o = other.o->hardAcquire<IObject>();
  } else if (this->hasLegacy(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v = other.v->hardAcquire<ValueReferenceCounted>();
  } else if (this->hasString()) {
    this->s = String::hardAcquire(other.s);
  } else if (this->hasFloat()) {
    this->f = other.f;
  } else if (this->hasInt()) {
    this->i = other.i;
  } else if (this->hasBool()) {
    this->b = other.b;
  } else if (this->hasType()) {
    this->t = other.t->hardAcquire<IType>();
  }
}

void egg::lang::Value::moveInternals(Value& other) {
  this->tag = other.tag;
  if (this->hasObject()) {
    auto* ptr = other.o;
    if (this->hasPointer()) {
      // Convert the soft pointer to a hard one
      this->tag = egg::ovum::Bits::clear(this->tag, Discriminator::Pointer);
      this->o = ptr->hardAcquire<IObject>();
    } else {
      // Not need to increment/decrement the reference count
      this->o = ptr;
    }
  } else if (this->hasLegacy(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v = other.v;
  } else if (this->hasString()) {
    this->s = other.s;
  } else if (this->hasFloat()) {
    this->f = other.f;
  } else if (this->hasInt()) {
    this->i = other.i;
  } else if (this->hasBool()) {
    this->b = other.b;
  } else if (this->hasType()) {
    this->t = other.t;
  }
  other.tag = Discriminator::None;
}

void egg::lang::Value::destroyInternals() {
  if (this->hasObject()) {
    if (!this->hasPointer()) {
      this->o->hardRelease();
    }
  } else if (this->hasLegacy(Discriminator::Indirect | Discriminator::Pointer)) {
    this->v->hardRelease();
  } else if (this->hasString()) {
    if (this->s != nullptr) {
      this->s->hardRelease();
    }
  } else if (this->hasType()) {
    this->t->hardRelease();
  }
}

egg::lang::ValueLegacy::ValueLegacy(const ValueLegacyReferenceCounted& vrc)
  : tag(Discriminator::Pointer) {
  this->v = vrc.hardAcquire<ValueLegacyReferenceCounted>();
}

egg::lang::ValueLegacy::ValueLegacy(const ValueLegacy& value) {
  this->copyInternals(value);
}

egg::lang::ValueLegacy::ValueLegacy(ValueLegacy&& value) noexcept {
  this->moveInternals(value);
}

egg::lang::ValueLegacy& egg::lang::ValueLegacy::operator=(const ValueLegacy& value) {
  if (this != &value) {
    this->destroyInternals();
    this->copyInternals(value);
  }
  return *this;
}

egg::lang::ValueLegacy& egg::lang::ValueLegacy::operator=(ValueLegacy&& value) noexcept {
  if (this != &value) {
    this->destroyInternals();
    this->moveInternals(value);
  }
  return *this;
}

egg::lang::ValueLegacy::~ValueLegacy() {
  this->destroyInternals();
}

const egg::lang::ValueLegacy& egg::lang::ValueLegacy::direct() const {
  auto* p = this;
  while (p->hasIndirect()) {
    p = p->v;
    assert(p != nullptr);
    assert(!p->hasFlowControl());
  }
  return *p;
}

egg::lang::ValueLegacy& egg::lang::ValueLegacy::direct() {
  auto* p = this;
  while (p->hasIndirect()) {
    p = p->v;
    assert(p != nullptr);
    assert(!p->hasFlowControl());
  }
  return *p;
}

egg::lang::ValueLegacyReferenceCounted& egg::lang::ValueLegacy::indirect(egg::ovum::IAllocator& allocator) {
  // Make this value indirect (i.e. heap-based)
  if (!this->hasIndirect()) {
    auto* heap = allocator.create<ValueOnHeap>(0, allocator, std::move(*this));
    assert(this->tag == Discriminator::None); // as a side-effect of the move
    this->tag = Discriminator::Indirect;
    this->v = heap->hardAcquire<ValueLegacyReferenceCounted>();
  }
  return *this->v;
}

egg::lang::ValueLegacy& egg::lang::ValueLegacy::soft(egg::ovum::ICollectable& container) {
  if (this->hasObject() && !this->hasPointer()) {
    // This is a hard pointer to an IObject, make it a soft pointer, if possible
    assert(this->o != nullptr);
    if (container.softLink(*this->o)) {
      // Successfully linked in the basket, so release our hard reference
      this->tag = egg::ovum::Bits::set(this->tag, Discriminator::Pointer);
      this->o->hardRelease();
    }
  }
  return *this;
}

bool egg::lang::ValueLegacy::equals(const ValueLegacy& lhs, const ValueLegacy& rhs) {
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
  assert(egg::ovum::Bits::hasAnySet(a.tag, Discriminator::Object));
  return a.o == b.o;
}

std::string egg::lang::ValueLegacy::getDiscriminatorString() const {
  if (this->tag == Discriminator::Indirect) {
    return this->v->getDiscriminatorString();
  }
  if (this->tag == Discriminator::Pointer) {
    return this->v->getDiscriminatorString() + "*";
  }
  return ValueLegacy::getDiscriminatorString(this->tag);
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

void egg::lang::ValueLegacy::softVisitLink(const egg::ovum::ICollectable::Visitor& visitor) const {
  if (this->isSoftReference()) {
    // Soft reference to an object
    assert(this->o != nullptr);
    visitor(*this->o);
  }
}

void egg::lang::ValueLegacy::addFlowControl(Discriminator bits) {
  assert(egg::ovum::Bits::mask(bits, Discriminator::FlowControl) == bits);
  assert(!this->hasFlowControl());
  this->tag = this->tag | bits;
  assert(this->hasFlowControl());
}

bool egg::lang::ValueLegacy::stripFlowControl(Discriminator bits) {
  assert(egg::ovum::Bits::mask(bits, Discriminator::FlowControl) == bits);
  if (egg::ovum::Bits::hasAnySet(this->tag, bits)) {
    assert(this->hasFlowControl());
    this->tag = egg::ovum::Bits::clear(this->tag, bits);
    assert(!this->hasFlowControl());
    return true;
  }
  return false;
}

std::string egg::lang::ValueLegacy::getBasalString(Basal tag) {
  static const egg::yolk::String::StringFromEnum table[] = {
    { int(Basal::Any), "any" },
    { int(Basal::Void), "void" },
    { int(Basal::Bool), "bool" },
    { int(Basal::Int), "int" },
    { int(Basal::Float), "float" },
    { int(Basal::String), "string" },
    { int(Basal::Object), "object" }
  };
  if (tag == Basal::None) {
    return "var";
  }
  if (tag == Basal::Null) {
    return "null";
  }
  if (egg::ovum::Bits::hasAnySet(tag, Basal::Null)) {
    return ValueLegacy::getBasalString(egg::ovum::Bits::clear(tag, Basal::Null)) + "?";
  }
  return egg::yolk::String::fromEnum(tag, table);
}

std::string egg::lang::ValueLegacy::getDiscriminatorString(Discriminator tag) {
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
  if (egg::ovum::Bits::hasAnySet(tag, Discriminator::Null)) {
    return ValueLegacy::getDiscriminatorString(egg::ovum::Bits::clear(tag, Discriminator::Null)) + "?";
  }
  return egg::yolk::String::fromEnum(tag, table);
}

egg::lang::ITypeRef egg::lang::ValueLegacy::getRuntimeType() const {
  assert(!this->hasIndirect());
  if (this->hasObject()) {
    return this->o->getRuntimeType();
  }
  if (this->hasPointer()) {
    return this->v->getRuntimeType()->pointerType();
  }
  if (this->hasType()) {
    // TODO Is a type's type itself?
    return ITypeRef(this->t);
  }
  auto basal = static_cast<Basal>(this->tag);
  auto* native = getNativeBasal(basal);
  if (native != nullptr) {
    return ITypeRef(native);
  }
  EGG_THROW("Internal type error: Unknown runtime type");
}

egg::ovum::String egg::lang::ValueLegacy::toString() const {
  // OPTIMIZE
  if (this->hasObject()) {
    auto str = this->getObject()->toString();
    if (str.tag == Discriminator::String) {
      return str.getString();
    }
    return "<invalid>";
  }
  if (this->hasType()) {
    return this->t->toString();
  }
  return this->toUTF8();
}

std::string egg::lang::ValueLegacy::toUTF8() const {
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
  if (this->hasObject()) {
    auto str = this->getObject()->toString();
    if (str.tag == Discriminator::String) {
      return str.getString().toUTF8();
    }
    return "<invalid>";
  }
  if (this->tag == Discriminator::Type) {
    return this->t->toString().toUTF8();
  }
  return "<" + ValueLegacy::getDiscriminatorString(this->tag) + ">";
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

bool egg::lang::IFunctionSignature::validateCall(egg::ovum::IExecution& execution, const IParameters& runtime, Value& problem) const {
  // The default implementation just calls validateCallDefault()
  return this->validateCallDefault(execution, runtime, problem);
}

bool egg::lang::IFunctionSignature::validateCallDefault(egg::ovum::IExecution& execution, const IParameters& parameters, Value& problem) const {
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

egg::lang::Value egg::lang::IType::promoteAssignment(egg::ovum::IExecution& execution, const Value& rhs) const {
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

egg::lang::Basal egg::lang::IType::getBasalTypes() const {
  // The default implementation is to say we are an object
  return Basal::Object;
}

egg::lang::ITypeRef egg::lang::IType::unionWith(const IType& other) const {
  // The default implementation is to simply make a new union (native and basal types can be more clever)
  return Type::makeUnion(*this, other);
}

egg::lang::ITypeRef egg::lang::Type::makeBasal(Basal basal) {
  // Try to use non-reference-counted globals
  auto* native = getNativeBasal(basal);
  if (native != nullptr) {
    return ITypeRef(native);
  }
  return defaultAllocator().make<TypeBasal>(basal);
}

egg::lang::ITypeRef egg::lang::Type::makeUnion(const egg::lang::IType& a, const egg::lang::IType& b) {
  // If they are both basal types, just union the tags
  auto sa = a.getBasalTypes();
  auto sb = b.getBasalTypes();
  if ((sa != Basal::None) && (sb != Basal::None)) {
    return Type::makeBasal(sa | sb);
  }
  return defaultAllocator().make<TypeUnion>(a, b);
}
