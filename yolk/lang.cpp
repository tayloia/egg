#include "yolk/yolk.h"
#include "ovum/utf.h"

namespace {
  using namespace egg::lang;
  using namespace egg::ovum;

  // Forward declaration
  const IType* getNativeBasal(BasalBits basal);

  IAllocator& defaultAllocator() {
    static AllocatorDefault allocator; // WIBBLE
    return allocator;
  }

  bool arithmeticEqual(double a, int64_t b) {
    // TODO
    return a == b;
  }

  std::pair<std::string, int> tagToStringPriority(BasalBits basal) {
    // Adjust the priority of the result if the string has '|' (union) in it
    // This is so that parentheses can be added appropriately to handle "(void|int)*" versus "void|int*"
    auto result = std::make_pair(ValueLegacy::getBasalString(basal), 0);
    if (result.first.find('|') != std::string::npos) {
      result.second--;
    }
    return result;
  }

  IType::AssignmentSuccess canBeAssignedFromBasal(BasalBits lhs, const IType& rtype) {
    assert(lhs != BasalBits::None);
    auto rhs = rtype.getBasalTypes();
    assert(rhs != BasalBits::None);
    if (rhs == BasalBits::None) { // WIBBLE
      // The source is not basal
      return IType::AssignmentSuccess::Never;
    }
    if (Bits::hasAllSet(lhs, rhs)) {
      // The assignment will always work (unless it includes 'void')
      if (Bits::hasAnySet(rhs, BasalBits::Void)) {
        return IType::AssignmentSuccess::Sometimes;
      }
      return IType::AssignmentSuccess::Always;
    }
    if (Bits::hasAnySet(lhs, rhs)) {
      // There's a possibility that the assignment might work
      return IType::AssignmentSuccess::Sometimes;
    }
    if (Bits::hasAnySet(lhs, BasalBits::Float) && Bits::hasAnySet(rhs, BasalBits::Int)) {
      // We allow type promotion int->float
      return IType::AssignmentSuccess::Sometimes;
    }
    return IType::AssignmentSuccess::Never;
  }

  ValueLegacy promoteAssignmentBasal(BasalBits lhs, const ValueLegacy& rhs) {
    assert(lhs != BasalBits::None);
    assert(!rhs.hasIndirect());
    if (rhs.hasBasal(lhs)) {
      // It's an exact type match (narrowing)
      return rhs;
    }
    if (Bits::hasAnySet(lhs, BasalBits::Float) && rhs.isInt()) {
      // We allow type promotion int->float
      return ValueLegacy(double(rhs.getInt())); // TODO overflows?
    }
    egg::lang::ValueLegacy exception{ egg::ovum::StringBuilder::concat("Cannot assign a value of type '", rhs.getRuntimeType()->toString(), "' to a target of type '", ValueLegacy::getBasalString(lhs), "'") };
    exception.addFlowControl(egg::ovum::VariantBits::Throw);
    return exception;
  }

  class TypePointer : public HardReferenceCounted<IType> {
    EGG_NO_COPY(TypePointer);
  private:
    ITypeRef referenced;
  public:
    TypePointer(IAllocator& allocator, const IType& referenced)
      : HardReferenceCounted(allocator, 0), referenced(&referenced) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair(this->referenced->toString(0).toUTF8() + "*", 0);
    }
    virtual egg::ovum::BasalBits getBasalTypes() const override {
      return egg::ovum::BasalBits::None;
    }
    virtual ITypeRef pointeeType() const override {
      return this->referenced;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return this->referenced->canBeAssignedFrom(*rhs.pointeeType());
    }
  };

  class TypeNull : public NotReferenceCounted<IType>{
  public:
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("null", 0);
    }
    virtual BasalBits getBasalTypes() const override {
      return BasalBits::Null;
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto basal = other.getBasalTypes();
      if (Bits::hasAnySet(basal, BasalBits::Null)) {
        // The other type supports Null anyway
        return ITypeRef(&other);
      }
      return Type::makeUnion(*this, other);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never;
    }
    virtual ValueLegacy promoteAssignment(const ValueLegacy&) const override {
      egg::lang::ValueLegacy exception{ egg::ovum::String("Cannot assign to 'null' value") };
      exception.addFlowControl(egg::ovum::VariantBits::Throw);
      return exception;
    }
  };
  const TypeNull typeNull{};

  template<BasalBits BASAL>
  class TypeNative : public NotReferenceCounted<IType> {
  public:
    TypeNative() {
      assert(!Bits::hasAnySet(BASAL, BasalBits::Null));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(BASAL);
    }
    virtual BasalBits getBasalTypes() const override {
      return BASAL;
    }
    virtual ITypeRef denulledType() const override {
      auto denulled = Bits::clear(BASAL, BasalBits::Null);
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
    virtual ValueLegacy promoteAssignment(const ValueLegacy& rhs) const override {
      return promoteAssignmentBasal(BASAL, rhs);
    }
  };
  const TypeNative<BasalBits::Void> typeVoid{};
  const TypeNative<BasalBits::Bool> typeBool{};
  const TypeNative<BasalBits::Int> typeInt{};
  const TypeNative<BasalBits::Float> typeFloat{};
  const TypeNative<BasalBits::Arithmetic> typeArithmetic{};

  class TypeString : public TypeNative<BasalBits::String> {
  public:
    // TODO callable and indexable
    virtual bool iterable(ITypeRef& type) const override {
      // When strings are iterated, they iterate through strings (of codepoints)
      type = Type::String;
      return true;
    }
  };
  const TypeString typeString{};

  const IType* getNativeBasal(BasalBits basal) {
    // OPTIMIZE
    if (basal == egg::ovum::BasalBits::Void) {
      return &typeVoid;
    }
    if (basal == egg::ovum::BasalBits::Null) {
      return &typeNull;
    }
    if (basal == egg::ovum::BasalBits::Bool) {
      return &typeBool;
    }
    if (basal == egg::ovum::BasalBits::Int) {
      return &typeInt;
    }
    if (basal == egg::ovum::BasalBits::Float) {
      return &typeFloat;
    }
    if (basal == egg::ovum::BasalBits::String) {
      return &typeString;
    }
    if (basal == egg::ovum::BasalBits::Arithmetic) {
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

  class TypeBasal : public HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeBasal);
  private:
    BasalBits tag;
  public:
    TypeBasal(IAllocator& allocator, BasalBits basal)
      : HardReferenceCounted(allocator, 0), tag(basal) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(this->tag);
    }
    virtual BasalBits getBasalTypes() const override {
      return this->tag;
    }
    virtual ITypeRef denulledType() const override {
      auto denulled = Bits::clear(this->tag, BasalBits::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return Type::makeBasal(denulled);
      }
      return ITypeRef(this);
    }
    virtual ITypeRef unionWith(const IType& other) const override {
      auto basal = other.getBasalTypes();
      if (basal == BasalBits::None) {
        // The other type is not basal
        return Type::makeUnion(*this, other);
      }
      auto both = Bits::set(this->tag, basal);
      if (both != this->tag) {
        // There's a new basal type that we don't support, so create a new type
        return Type::makeBasal(both);
      }
      return ITypeRef(this);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromBasal(this->tag, rhs);
    }
    virtual ValueLegacy promoteAssignment(const ValueLegacy& rhs) const override {
      return promoteAssignmentBasal(this->tag, rhs);
    }
    virtual const IFunctionSignature* callable() const override {
      if (Bits::hasAnySet(this->tag, BasalBits::Object)) {
        return &omniFunctionSignature;
      }
      return nullptr;
    }
    virtual bool iterable(ITypeRef& type) const override {
      if (Bits::hasAnySet(this->tag, BasalBits::Object)) {
        type = Type::Any;
        return true;
      }
      if (Bits::hasAnySet(this->tag, BasalBits::String)) {
        type = Type::String;
        return true;
      }
      return false;
    }
  };

  class TypeUnion : public HardReferenceCounted<IType> {
    EGG_NO_COPY(TypeUnion);
  private:
    ITypeRef a;
    ITypeRef b;
  public:
    TypeUnion(IAllocator& allocator, const IType& a, const IType& b)
      : HardReferenceCounted(allocator, 0), a(&a), b(&b) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto sa = this->a->toStringPrecedence().first;
      auto sb = this->b->toStringPrecedence().first;
      return std::make_pair(sa + "|" + sb, -1);
    }
    virtual BasalBits getBasalTypes() const override {
      return this->a->getBasalTypes() | this->b->getBasalTypes();
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never; // TODO
    }
    virtual const IFunctionSignature* callable() const override {
      EGG_THROW("TODO: Cannot yet call to union value"); // TODO
    }
  };

  class ValueOnHeap : public HardReferenceCounted<egg::lang::ValueLegacyReferenceCounted> {
    ValueOnHeap(const ValueOnHeap&) = delete;
    ValueOnHeap& operator=(const ValueOnHeap&) = delete;
  public:
    ValueOnHeap(IAllocator& allocator, ValueLegacy&& value) noexcept
      : HardReferenceCounted<egg::lang::ValueLegacyReferenceCounted>(allocator, 0, std::move(value)) {
    }
  };
}

// Native types
const egg::ovum::Type egg::ovum::Type::Void{ typeVoid };
const egg::ovum::Type egg::ovum::Type::Null{ typeNull };
const egg::ovum::Type egg::ovum::Type::Bool{ typeBool };
const egg::ovum::Type egg::ovum::Type::Int{ typeInt };
const egg::ovum::Type egg::ovum::Type::Float{ typeFloat };
const egg::ovum::Type egg::ovum::Type::String{ typeString };
const egg::ovum::Type egg::ovum::Type::Arithmetic{ typeArithmetic };
const egg::ovum::Type egg::ovum::Type::Any{ *defaultAllocator().make<TypeBasal>(egg::ovum::BasalBits::Any) };
const egg::ovum::Type egg::ovum::Type::AnyQ{ *defaultAllocator().make<TypeBasal>(egg::ovum::BasalBits::Any | egg::ovum::BasalBits::Null) };

// Trivial constant values
const egg::lang::ValueLegacy egg::lang::ValueLegacy::Void{ Tag::Void };
const egg::lang::ValueLegacy egg::lang::ValueLegacy::Null{ Tag::Null };
const egg::lang::ValueLegacy egg::lang::ValueLegacy::False{ false };
const egg::lang::ValueLegacy egg::lang::ValueLegacy::True{ true };
const egg::lang::ValueLegacy egg::lang::ValueLegacy::EmptyString{ String() };
const egg::lang::ValueLegacy egg::lang::ValueLegacy::Break{ Tag::Break };
const egg::lang::ValueLegacy egg::lang::ValueLegacy::Continue{ Tag::Continue };
const egg::lang::ValueLegacy egg::lang::ValueLegacy::Rethrow{ Tag::Throw | Tag::Void };
const egg::lang::ValueLegacy egg::lang::ValueLegacy::ReturnVoid{ Tag::Return | Tag::Void };

void egg::lang::ValueLegacy::copyInternals(const ValueLegacy& other) {
  assert(this != &other);
  this->tag = other.tag;
  if (this->hasObject()) {
    // We can only copy the reference as a hard pointer
    this->tag = egg::ovum::Bits::clear(other.tag, Tag::Pointer);
    this->o = other.o->hardAcquire<IObject>();
  } else if (this->hasReference()) {
    this->v = other.v->hardAcquire<ValueLegacyReferenceCounted>();
  } else if (this->hasString()) {
    this->s = String::hardAcquire(other.s);
  } else if (this->hasFloat()) {
    this->f = other.f;
  } else if (this->hasInt()) {
    this->i = other.i;
  } else if (this->hasBool()) {
    this->b = other.b;
  }
}

void egg::lang::ValueLegacy::moveInternals(ValueLegacy& other) {
  this->tag = other.tag;
  if (this->hasObject()) {
    auto* ptr = other.o;
    if (this->hasPointer()) {
      // Convert the soft pointer to a hard one
      this->tag = egg::ovum::Bits::clear(this->tag, Tag::Pointer);
      this->o = ptr->hardAcquire<IObject>();
    } else {
      // Not need to increment/decrement the reference count
      this->o = ptr;
    }
  } else if (this->hasReference()) {
    this->v = other.v;
  } else if (this->hasString()) {
    this->s = other.s;
  } else if (this->hasFloat()) {
    this->f = other.f;
  } else if (this->hasInt()) {
    this->i = other.i;
  } else if (this->hasBool()) {
    this->b = other.b;
  }
  other.tag = Tag::None;
}

void egg::lang::ValueLegacy::destroyInternals() {
  if (this->hasObject()) {
    if (!this->hasPointer()) {
      this->o->hardRelease();
    }
  } else if (this->hasReference()) {
    this->v->hardRelease();
  } else if (this->hasString()) {
    if (this->s != nullptr) {
      this->s->hardRelease();
    }
  }
}

egg::lang::ValueLegacy::ValueLegacy(const ValueLegacyReferenceCounted& vrc)
  : tag(VariantBits::Pointer) {
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
    assert(this->tag == Tag::None); // as a side-effect of the move
    this->tag = Tag::Indirect;
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
      this->tag = egg::ovum::Bits::set(this->tag, Tag::Pointer);
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
    if ((a.tag == Tag::Float) && (b.tag == Tag::Int)) {
      return arithmeticEqual(a.f, b.i);
    }
    if ((a.tag == Tag::Int) && (b.tag == Tag::Float)) {
      return arithmeticEqual(b.f, a.i);
    }
    return false;
  }
  if (a.tag == Tag::Bool) {
    return a.b == b.b;
  }
  if (a.tag == Tag::Int) {
    return a.i == b.i;
  }
  if (a.tag == Tag::Float) {
    return a.f == b.f;
  }
  if (a.tag == Tag::String) {
    return egg::ovum::IMemory::equals(a.s, b.s);
  }
  if (a.tag == Tag::Pointer) {
    return a.v == b.v;
  }
  assert(egg::ovum::Bits::hasAnySet(a.tag, Tag::Object));
  return a.o == b.o;
}

void egg::lang::ValueLegacy::softVisitLink(const egg::ovum::ICollectable::Visitor& visitor) const {
  if (this->isSoftReference()) {
    // Soft reference to an object
    assert(this->o != nullptr);
    visitor(*this->o);
  }
}

void egg::lang::ValueLegacy::addFlowControl(VariantBits bits) {
  assert(egg::ovum::Bits::mask(bits, Tag::FlowControl) == bits);
  assert(!this->hasFlowControl());
  this->tag = this->tag | bits;
  assert(this->hasFlowControl());
}

bool egg::lang::ValueLegacy::stripFlowControl(VariantBits bits) {
  assert(egg::ovum::Bits::mask(bits, Tag::FlowControl) == bits);
  if (egg::ovum::Bits::hasAnySet(this->tag, bits)) {
    assert(this->hasFlowControl());
    this->tag = egg::ovum::Bits::clear(this->tag, bits);
    assert(!this->hasFlowControl());
    return true;
  }
  return false;
}

std::string egg::lang::ValueLegacy::getBasalString(BasalBits tag) {
  static const egg::yolk::String::StringFromEnum table[] = {
    { int(BasalBits::Any), "any" },
    { int(BasalBits::Void), "void" },
    { int(BasalBits::Bool), "bool" },
    { int(BasalBits::Int), "int" },
    { int(BasalBits::Float), "float" },
    { int(BasalBits::String), "string" },
    { int(BasalBits::Object), "object" }
  };
  if (tag == BasalBits::None) {
    return "var";
  }
  if (tag == BasalBits::Null) {
    return "null";
  }
  if (egg::ovum::Bits::hasAnySet(tag, BasalBits::Null)) {
    return ValueLegacy::getBasalString(egg::ovum::Bits::clear(tag, BasalBits::Null)) + "?";
  }
  return egg::yolk::String::fromEnum(tag, table);
}

egg::ovum::ITypeRef egg::lang::ValueLegacy::getRuntimeType() const {
  assert(!this->hasIndirect());
  if (this->hasObject()) {
    return this->o->getRuntimeType();
  }
  if (this->hasPointer()) {
    return this->v->getRuntimeType()->pointerType();
  }
  auto basal = static_cast<BasalBits>(this->tag);
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
    if (str.tag == Tag::String) {
      return str.getString();
    }
    return "<invalid>";
  }
  return this->toUTF8();
}

std::string egg::lang::ValueLegacy::toUTF8() const {
  // OPTIMIZE
  if (this->tag == Tag::Null) {
    return "null";
  }
  if (this->tag == Tag::Bool) {
    return this->b ? "true" : "false";
  }
  if (this->tag == Tag::Int) {
    return egg::yolk::String::fromSigned(this->i);
  }
  if (this->tag == Tag::Float) {
    return egg::yolk::String::fromFloat(this->f);
  }
  if (this->tag == Tag::String) {
    return String(this->s).toUTF8();
  }
  if (this->hasObject()) {
    auto str = this->getObject()->toString();
    if (str.tag == Tag::String) {
      return str.getString().toUTF8();
    }
    return "<invalid>";
  }
  return "<" + this->getRuntimeType()->toString().toUTF8() + ">";
}

egg::ovum::ITypeRef egg::ovum::IType::pointerType() const {
  // WIBBLE The default implementation is to return a new type 'Type*'
  return defaultAllocator().make<TypePointer>(*this);
}

egg::lang::ValueLegacy egg::ovum::IType::promoteAssignment(const egg::lang::ValueLegacy& rhs) const {
  // WIBBLE The default implementation calls IType::canBeAssignedFrom() but does not actually promote
  auto& direct = rhs.direct();
  auto rtype = direct.getRuntimeType();
  if (this->canBeAssignedFrom(*rtype) == AssignmentSuccess::Never) {
    egg::lang::ValueLegacy exception{ StringBuilder::concat("Cannot assign a value of type '", rtype->toString(), "' to a target of type '", this->toString(), "'") };
    exception.addFlowControl(egg::ovum::VariantBits::Throw);
    return exception;
  }
  return direct;
}

bool egg::ovum::IFunctionSignature::validateCallDefault(egg::ovum::IExecution& execution, const IParameters& parameters, egg::lang::ValueLegacy& problem) const {
  // TODO type checking, etc
  if (parameters.getNamedCount() > 0) {
    problem = execution.raiseFormat(this->toString(Parts::All), ": Named parameters are not yet supported"); // TODO
    return false;
  }
  auto maxPositional = this->getParameterCount();
  auto minPositional = maxPositional;
  while ((minPositional > 0) && !Bits::hasAnySet(this->getParameter(minPositional - 1).getFlags(), IFunctionSignatureParameter::Flags::Required)) {
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
  if ((maxPositional > 0) && Bits::hasAnySet(this->getParameter(maxPositional - 1).getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
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

egg::ovum::ITypeRef egg::ovum::Type::makeBasal(egg::ovum::BasalBits basal) {
  // Try to use non-reference-counted globals
  auto* native = getNativeBasal(basal);
  if (native != nullptr) {
    return ITypeRef(native);
  }
  return defaultAllocator().make<TypeBasal>(basal);
}

egg::ovum::ITypeRef egg::ovum::Type::makeUnion(const egg::ovum::IType& a, const egg::ovum::IType& b) {
  // If they are both basal types, just union the tags
  auto sa = a.getBasalTypes();
  auto sb = b.getBasalTypes();
  if ((sa != BasalBits::None) && (sb != BasalBits::None)) {
    return Type::makeBasal(sa | sb);
  }
  return defaultAllocator().make<TypeUnion>(a, b);
}
