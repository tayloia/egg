#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  std::pair<std::string, int> tagToStringPriority(BasalBits basal) {
    // Adjust the priority of the result if the string has '|' (union) in it
    // This is so that parentheses can be added appropriately to handle "(void|int)*" versus "void|int*"
    auto result = std::make_pair(Type::getBasalString(basal), 0);
    if (result.first.find('|') != std::string::npos) {
      result.second--;
    }
    return result;
  }

  IType::AssignmentSuccess canBeAssignedFromBasal(BasalBits lhs, const IType& rtype) {
    assert(lhs != BasalBits::None);
    auto rhs = rtype.getBasalTypes();
    assert(rhs != BasalBits::None);
    if (rhs == BasalBits::None) {
      // TODO The source is not basal
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

  Variant promoteAssignmentBasal(BasalBits lhs, const Variant& rhs) {
    assert(lhs != BasalBits::None);
    assert(!rhs.hasIndirect());
    if (rhs.hasAny(static_cast<VariantBits>(lhs))) {
      // It's an exact type match (narrowing)
      return rhs;
    }
    if (Bits::hasAnySet(lhs, BasalBits::Float) && rhs.isInt()) {
      // We allow type promotion int->float
      return Variant(double(rhs.getInt())); // TODO overflows?
    }
    Variant exception{ StringBuilder::concat("Cannot assign a value of type '", rhs.getRuntimeType()->toString(), "' to a target of type '", Type::getBasalString(lhs), "'") };
    exception.addFlowControl(VariantBits::Throw);
    return exception;
  }

  // An 'omni' function looks like this: 'any?(...any?[])'
  class OmniFunctionSignature : public IFunctionSignature {
    OmniFunctionSignature(const OmniFunctionSignature&) = delete;
    OmniFunctionSignature& operator=(const OmniFunctionSignature&) = delete;
  private:
    class Parameter : public IFunctionSignatureParameter {
    public:
      virtual String getName() const override {
        return String();
      }
      virtual Type getType() const override {
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
    OmniFunctionSignature() = default;
    virtual String getFunctionName() const override {
      return String();
    }
    virtual Type getReturnType() const override {
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

  class TypePointer : public HardReferenceCounted<IType> {
    TypePointer(const TypePointer&) = delete;
    TypePointer& operator=(const TypePointer&) = delete;
  private:
    Type referenced;
  public:
    TypePointer(IAllocator& allocator, const IType& referenced)
      : HardReferenceCounted(allocator, 0), referenced(&referenced) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair(this->referenced->toString(0).toUTF8() + "*", 0);
    }
    virtual BasalBits getBasalTypes() const override {
      return BasalBits::None;
    }
    virtual Type pointeeType() const override {
      return this->referenced;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return this->referenced->canBeAssignedFrom(*rhs.pointeeType());
    }
  };

  template<BasalBits BASAL>
  class TypeCommon : public NotReferenceCounted<IType> {
    TypeCommon(const TypeCommon&) = delete;
    TypeCommon& operator=(const TypeCommon&) = delete;
  public:
    TypeCommon() = default;
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(BASAL);
    }
    virtual BasalBits getBasalTypes() const override {
      return BASAL;
    }
    virtual Type unionWith(IAllocator& allocator, const IType& other) const override {
      auto basal = other.getBasalTypes();
      if (basal == BasalBits::None) {
        // The other type is not basal
        return Type::makeUnion(allocator, *this, other);
      }
      if (Bits::hasAllSet(BASAL, basal)) {
        // We are a superset already
        return Type(this);
      }
      if (Bits::hasAllSet(basal, BASAL)) {
        // The other type is a superset anyway
        return Type(&other);
      }
      return Type::makeUnion(allocator, *this, other);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromBasal(BASAL, rhs);
    }
    virtual Variant promoteAssignment(const Variant& rhs) const override {
      return promoteAssignmentBasal(BASAL, rhs);
    }
  };
  const TypeCommon<BasalBits::Void> typeVoid{};
  const TypeCommon<BasalBits::Bool> typeBool{};
  const TypeCommon<BasalBits::Int> typeInt{};
  const TypeCommon<BasalBits::Float> typeFloat{};
  const TypeCommon<BasalBits::Arithmetic> typeArithmetic{};

  class TypeNull : public TypeCommon<BasalBits::Null> {
    TypeNull(const TypeNull&) = delete;
    TypeNull& operator=(const TypeNull&) = delete;
  public:
    TypeNull() = default;
    virtual Type denulledType() const override {
      // You cannot denull null!
      return Type(nullptr);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never;
    }
    virtual Variant promoteAssignment(const Variant&) const override {
      // WIBBLE factory
      Variant exception{ String("Cannot assign to 'null' value") };
      exception.addFlowControl(VariantBits::Throw);
      return exception;
    }
  };
  const TypeNull typeNull{};

  class TypeString : public TypeCommon<BasalBits::String> {
    TypeString(const TypeString&) = delete;
    TypeString& operator=(const TypeString&) = delete;
  public:
    TypeString() = default;
    // TODO callable and indexable
    virtual bool iterable(Type& type) const override {
      // When strings are iterated, they iterate through strings (of codepoints)
      type = Type::String;
      return true;
    }
  };
  const TypeString typeString{};

  class TypeObject : public TypeCommon<BasalBits::Object> {
    TypeObject(const TypeObject&) = delete;
    TypeObject& operator=(const TypeObject&) = delete;
  public:
    TypeObject() = default;
    virtual const IFunctionSignature* callable() const override {
      return &omniFunctionSignature;
    }
    virtual bool iterable(Type& type) const override {
      type = Type::AnyQ;
      return true;
    }
  };
  const TypeObject typeObject{};

  class TypeAny : public TypeCommon<BasalBits::Any> {
    TypeAny(const TypeAny&) = delete;
    TypeAny& operator=(const TypeAny&) = delete;
  public:
    TypeAny() = default;
    virtual const IFunctionSignature* callable() const override {
      return &omniFunctionSignature;
    }
    virtual bool iterable(Type& type) const override {
      type = Type::AnyQ;
      return true;
    }
  };
  const TypeAny typeAny{};

  class TypeAnyQ : public TypeCommon<BasalBits::AnyQ> {
    TypeAnyQ(const TypeAnyQ&) = delete;
    TypeAnyQ& operator=(const TypeAnyQ&) = delete;
  public:
    TypeAnyQ() = default;
    virtual const IFunctionSignature* callable() const override {
      return &omniFunctionSignature;
    }
    virtual bool iterable(Type& type) const override {
      type = Type::AnyQ;
      return true;
    }
  };
  const TypeAnyQ typeAnyQ{};

  class TypeBasal : public HardReferenceCounted<IType> {
    TypeBasal(const TypeBasal&) = delete;
    TypeBasal& operator=(const TypeBasal&) = delete;
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
    virtual Type denulledType() const override {
      auto denulled = Bits::clear(this->tag, BasalBits::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return Type::makeBasal(this->allocator, denulled);
      }
      return Type(this);
    }
    virtual Type unionWith(IAllocator& alloc, const IType& other) const override {
      auto basal = other.getBasalTypes();
      if (basal == BasalBits::None) {
        // The other type is not basal
        return Type::makeUnion(alloc, *this, other);
      }
      auto both = Bits::set(this->tag, basal);
      if (both != this->tag) {
        // There's a new basal type that we don't support, so create a new type
        return Type::makeBasal(alloc, both);
      }
      return Type(this);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const {
      return canBeAssignedFromBasal(this->tag, rhs);
    }
    virtual Variant promoteAssignment(const Variant& rhs) const override {
      return promoteAssignmentBasal(this->tag, rhs);
    }
    virtual const IFunctionSignature* callable() const override {
      if (Bits::hasAnySet(this->tag, BasalBits::Object)) {
        return &omniFunctionSignature;
      }
      return nullptr;
    }
    virtual bool iterable(Type& type) const override {
      if (Bits::hasAnySet(this->tag, BasalBits::Object)) {
        type = Type::AnyQ;
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
    TypeUnion(const TypeUnion&) = delete;
    TypeUnion& operator=(const TypeUnion&) = delete;
  private:
    Type a;
    Type b;
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
      throw std::runtime_error("TODO: Cannot yet call to union value"); // TODO
    }
  };

  class FunctionSignatureParameter : public IFunctionSignatureParameter {
  private:
    String name; // may be empty
    Type type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    FunctionSignatureParameter(const String& name, const Type& type, size_t position, Flags flags)
      : name(name), type(type), position(position), flags(flags) {
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual Type getType() const override {
      return this->type;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
  };

  class FunctionSignature : public IFunctionSignature {
  private:
    String name;
    Type returnType;
    std::vector<FunctionSignatureParameter> parameters;
  public:
    FunctionSignature(const String& name, const Type& returnType)
      : name(name), returnType(returnType) {
    }
    void addSignatureParameter(const String& parameterName, const Type& parameterType, size_t position, FunctionSignatureParameter::Flags flags) {
      this->parameters.emplace_back(parameterName, parameterType, position, flags);
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->returnType;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters.size());
      return this->parameters[index];
    }
  };

  const char* getBasalComponent(egg::ovum::BasalBits basal) {
    switch (basal) {
    case egg::ovum::BasalBits::None:
      return "var";
#define EGG_OVUM_BASAL_COMPONENT(name, text) case egg::ovum::BasalBits::name: return text;
      EGG_OVUM_BASAL(EGG_OVUM_BASAL_COMPONENT)
#undef EGG_OVUM_BASAL_COMPONENT
    case egg::ovum::BasalBits::Any:
      return "any";
    case egg::ovum::BasalBits::Arithmetic:
    case egg::ovum::BasalBits::AnyQ:
      // Non-trivial
      break;
    }
    return nullptr;
  }

  IAllocator& defaultAllocator() {
    // WIBBLE retire
    static AllocatorDefault allocator;
    return allocator;
  }
}

egg::ovum::String egg::ovum::IType::toString(int priority) const {
  auto pair = this->toStringPrecedence();
  if (pair.second < priority) {
    return StringBuilder::concat("(", pair.first, ")");
  }
  return pair.first;
}

egg::ovum::BasalBits egg::ovum::IType::getBasalTypes() const {
  // WIBBLE return nullptr
  // The default implementation is to return 'Object'
  return BasalBits::Object;
}

egg::ovum::Type egg::ovum::IType::pointeeType() const {
  // The default implementation is to return nullptr indicating that this is NOT dereferencable
  return Type(nullptr);
}

egg::ovum::Type egg::ovum::IType::denulledType() const {
  // The default implementation is to return self
  return Type(this);
}

egg::ovum::String egg::ovum::IFunctionSignature::toString(Parts parts) const {
  StringBuilder sb;
  this->buildStringDefault(sb, parts);
  return sb.str();
}

bool egg::ovum::IFunctionSignature::validateCall(IExecution& execution, const IParameters& runtime, Variant& problem) const {
  // The default implementation just calls validateCallDefault()
  return this->validateCallDefault(execution, runtime, problem);
}

egg::ovum::String egg::ovum::IIndexSignature::toString() const {
  return StringBuilder::concat(this->getResultType()->toString(), "[", this->getIndexType()->toString(), "]");
}

const egg::ovum::IFunctionSignature* egg::ovum::IType::callable() const {
  // The default implementation is to say we don't support calling with '()'
  return nullptr;
}

const egg::ovum::IIndexSignature* egg::ovum::IType::indexable() const {
  // The default implementation is to say we don't support indexing with '[]'
  return nullptr;
}

bool egg::ovum::IType::dotable(const String*, Type&, String& reason) const {
  // The default implementation is to say we don't support properties with '.'
  reason = StringBuilder::concat("Values of type '", this->toString(), "' do not support the '.' operator for property access");
  return false;
}

bool egg::ovum::IType::iterable(Type&) const {
  // The default implementation is to say we don't support iteration
  return false;
}

egg::ovum::Type egg::ovum::IType::unionWith(IAllocator& allocator, const IType& other) const {
  // The default implementation is to simply make a new union (basal types can be more clever)
  return Type::makeUnion(allocator, *this, other);
}

void egg::ovum::IFunctionSignature::buildStringDefault(StringBuilder& sb, IFunctionSignature::Parts parts) const {
  // TODO better formatting of named/variadic etc.
  if (Bits::hasAnySet(parts, IFunctionSignature::Parts::ReturnType)) {
    // Use precedence zero to get any necessary parentheses
    sb.add(this->getReturnType()->toString(0));
  }
  if (Bits::hasAnySet(parts, IFunctionSignature::Parts::FunctionName)) {
    auto name = this->getFunctionName();
    if (!name.empty()) {
      sb.add(' ', name);
    }
  }
  if (Bits::hasAnySet(parts, IFunctionSignature::Parts::ParameterList)) {
    sb.add('(');
    auto n = this->getParameterCount();
    for (size_t i = 0; i < n; ++i) {
      if (i > 0) {
        sb.add(", ");
      }
      auto& parameter = this->getParameter(i);
      assert(parameter.getPosition() != SIZE_MAX);
      if (Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
        sb.add("...");
      } else {
        sb.add(parameter.getType()->toString());
        if (Bits::hasAnySet(parts, IFunctionSignature::Parts::ParameterNames)) {
          auto pname = parameter.getName();
          if (!pname.empty()) {
            sb.add(' ', pname);
          }
        }
        if (!Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Required)) {
          sb.add(" = null");
        }
      }
    }
    sb.add(')');
  }
}

egg::ovum::Type egg::ovum::IType::pointerType() const {
  // The default implementation is to return a new type 'Type*'
  return defaultAllocator().make<TypePointer, Type>(*this);
}

Variant egg::ovum::IType::promoteAssignment(const Variant& rhs) const {
  // The default implementation calls IType::canBeAssignedFrom() but does not actually promote
  auto& direct = rhs.direct();
  auto rtype = direct.getRuntimeType();
  if (this->canBeAssignedFrom(*rtype) == AssignmentSuccess::Never) {
    Variant exception{ StringBuilder::concat("Cannot assign a value of type '", rtype->toString(), "' to a target of type '", this->toString(), "'") };
    exception.addFlowControl(VariantBits::Throw);
    return exception;
  }
  return direct;
}

bool egg::ovum::IFunctionSignature::validateCallDefault(IExecution& execution, const IParameters& parameters, Variant& problem) const {
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

const egg::ovum::IType* egg::ovum::Type::getBasalType(BasalBits basal) {
  switch (basal) {
  case BasalBits::None:
    break;
#define EGG_OVUM_BASAL_INSTANCE(name, text) case BasalBits::name: return &type##name;
    EGG_OVUM_BASAL(EGG_OVUM_BASAL_INSTANCE)
#undef EGG_OVUM_BASAL_INSTANCE
  case BasalBits::Arithmetic:
    return &typeArithmetic;
  case BasalBits::Any:
    return &typeAny;
  case BasalBits::AnyQ:
    return &typeAnyQ;
  }
  return nullptr;
}

std::string egg::ovum::Type::getBasalString(BasalBits basal) {
  auto* component = getBasalComponent(basal);
  if (component != nullptr) {
    return component;
  }
  if (Bits::hasAnySet(basal, BasalBits::Null)) {
    return Type::getBasalString(Bits::clear(basal, BasalBits::Null)) + "?";
  }
  auto head = Bits::topmost(basal);
  assert(head != BasalBits::None);
  component = getBasalComponent(head);
  assert(component != nullptr);
  return getBasalString(Bits::clear(basal, head)) + '|' + component;
}

egg::ovum::Type egg::ovum::Type::makeBasal(IAllocator& allocator, BasalBits basal) {
  // Try to use non-reference-counted globals
  auto* common = Type::getBasalType(basal);
  if (common != nullptr) {
    return Type(common);
  }
  return allocator.make<TypeBasal, Type>(basal);
}

egg::ovum::Type egg::ovum::Type::makeUnion(IAllocator& allocator, const IType& a, const IType& b) {
  // If they are both basal types, just union the tags
  auto sa = a.getBasalTypes();
  auto sb = b.getBasalTypes();
  if ((sa != BasalBits::None) && (sb != BasalBits::None)) {
    return Type::makeBasal(allocator, sa | sb);
  }
  return allocator.make<TypeUnion, Type>(a, b);
}

// Common types
const egg::ovum::Type egg::ovum::Type::Void{ &typeVoid };
const egg::ovum::Type egg::ovum::Type::Null{ &typeNull };
const egg::ovum::Type egg::ovum::Type::Bool{ &typeBool };
const egg::ovum::Type egg::ovum::Type::Int{ &typeInt };
const egg::ovum::Type egg::ovum::Type::Float{ &typeFloat };
const egg::ovum::Type egg::ovum::Type::String{ &typeString };
const egg::ovum::Type egg::ovum::Type::Arithmetic{ &typeArithmetic };
const egg::ovum::Type egg::ovum::Type::Any{ &typeAny };
const egg::ovum::Type egg::ovum::Type::AnyQ{ &typeAnyQ };
