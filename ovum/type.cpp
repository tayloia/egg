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

  template<typename... ARGS>
  static Variant makeException(ARGS&&... args) {
    // TODO: proper exception object
    Variant exception{ StringBuilder::concat(std::forward<ARGS>(args)...) };
    exception.addFlowControl(VariantBits::Throw);
    return exception;
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
    return makeException("Cannot assign a value of type '", rhs.getRuntimeType().toString(), "' to a target of type '", Type::getBasalString(lhs), "'");
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

  class TypePointer : public HardReferenceCounted<TypeBase> {
    TypePointer(const TypePointer&) = delete;
    TypePointer& operator=(const TypePointer&) = delete;
  private:
    Type referenced;
  public:
    TypePointer(IAllocator& allocator, const IType& referenced)
      : HardReferenceCounted(allocator, 0), referenced(&referenced) {
    }
    virtual BasalBits getBasalTypes() const override {
      return BasalBits::None; // TODO
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override {
      return this->referenced->canBeAssignedFrom(*rhs.pointeeType());
    }
    virtual Type pointeeType() const override {
      return this->referenced;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair(this->referenced.toString(0).toUTF8() + "*", 0);
    }
  };

  template<BasalBits BASAL>
  class TypeCommon : public NotReferenceCounted<TypeBase> {
    TypeCommon(const TypeCommon&) = delete;
    TypeCommon& operator=(const TypeCommon&) = delete;
  public:
    TypeCommon() = default;
    virtual BasalBits getBasalTypes() const override {
      return BASAL;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override {
      return canBeAssignedFromBasal(BASAL, rhs);
    }
    virtual Variant promoteAssignment(const Variant& rhs) const override {
      return promoteAssignmentBasal(BASAL, rhs);
    }
    virtual Type unionWithBasal(IAllocator& allocator, BasalBits other) const override {
      assert(other != BasalBits::None);
      auto superset = Bits::set(BASAL, other);
      if (superset == BASAL) {
        // We are a superset already
        return Type(this);
      }
      return Type::makeBasal(allocator, superset);
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(BASAL);
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
      return nullptr;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const override {
      return AssignmentSuccess::Never;
    }
    virtual Variant promoteAssignment(const Variant&) const override {
      return makeException("Cannot assign to 'null' value");
    }
  };
  const TypeNull typeNull{};

  class TypeString : public TypeCommon<BasalBits::String> {
    TypeString(const TypeString&) = delete;
    TypeString& operator=(const TypeString&) = delete;
  public:
    TypeString() = default;
    // TODO callable and indexable
    virtual Type iterable() const override {
      // When strings are iterated, they iterate through strings (of codepoints)
      return Type::String;
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
    virtual Type iterable() const override {
      return Type::AnyQ;
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
    virtual Type iterable() const override {
      return Type::AnyQ;
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
    virtual Type iterable() const override {
      return Type::AnyQ;
    }
  };
  const TypeAnyQ typeAnyQ{};

  class TypeBasal : public HardReferenceCounted<TypeBase> {
    TypeBasal(const TypeBasal&) = delete;
    TypeBasal& operator=(const TypeBasal&) = delete;
  private:
    BasalBits tag;
  public:
    TypeBasal(IAllocator& allocator, BasalBits basal)
      : HardReferenceCounted(allocator, 0), tag(basal) {
    }
    virtual BasalBits getBasalTypes() const override {
      return this->tag;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override {
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
    virtual Type iterable() const override {
      if (Bits::hasAnySet(this->tag, BasalBits::Object)) {
        return Type::AnyQ;
      }
      if (Bits::hasAnySet(this->tag, BasalBits::String)) {
        return Type::String;
      }
      return nullptr;
    }
    virtual Type denulledType() const override {
      auto denulled = Bits::clear(this->tag, BasalBits::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return Type::makeBasal(this->allocator, denulled);
      }
      return Type(this);
    }
    virtual Type unionWithBasal(IAllocator& alloc, BasalBits other) const override {
      assert(other != BasalBits::None);
      auto superset = Bits::set(this->tag, other);
      if (superset == this->tag) {
        // We are a superset already
        return Type(this);
      }
      return Type::makeBasal(alloc, superset);
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(this->tag);
    }
  };

  class TypeUnion : public HardReferenceCounted<TypeBase> {
    TypeUnion(const TypeUnion&) = delete;
    TypeUnion& operator=(const TypeUnion&) = delete;
  private:
    Type a;
    Type b;
  public:
    TypeUnion(IAllocator& allocator, const IType& a, const IType& b)
      : HardReferenceCounted(allocator, 0), a(&a), b(&b) {
    }
    virtual BasalBits getBasalTypes() const override {
      return this->a->getBasalTypes() | this->b->getBasalTypes(); // TODO
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // TODO
      auto sa = this->a->toStringPrecedence().first;
      auto sb = this->b->toStringPrecedence().first;
      return std::make_pair(sa + "|" + sb, -1);
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
}

egg::ovum::String egg::ovum::Function::signatureToString(const IFunctionSignature& signature, Parts parts) {
  // TODO better formatting of named/variadic etc.
  StringBuilder sb;
  if (Bits::hasAnySet(parts, Parts::ReturnType)) {
    // Use precedence zero to get any necessary parentheses
    sb.add(signature.getReturnType().toString(0));
  }
  if (Bits::hasAnySet(parts, Parts::FunctionName)) {
    auto name = signature.getFunctionName();
    if (!name.empty()) {
      sb.add(' ', name);
    }
  }
  if (Bits::hasAnySet(parts, Parts::ParameterList)) {
    sb.add('(');
    auto n = signature.getParameterCount();
    for (size_t i = 0; i < n; ++i) {
      if (i > 0) {
        sb.add(", ");
      }
      auto& parameter = signature.getParameter(i);
      assert(parameter.getPosition() != SIZE_MAX);
      if (Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
        sb.add("...");
      } else {
        sb.add(parameter.getType().toString());
        if (Bits::hasAnySet(parts, Parts::ParameterNames)) {
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
  return sb.str();
}

egg::ovum::Variant egg::ovum::Function::validateCall(IExecution& execution, const IFunctionSignature& signature, const IParameters& parameters) {
  // TODO type checking, etc
  if (parameters.getNamedCount() > 0) {
    return execution.raiseFormat(Function::signatureToString(signature, Parts::All), ": Named parameters are not yet supported"); // TODO
  }
  auto maxPositional = signature.getParameterCount();
  auto minPositional = maxPositional;
  while ((minPositional > 0) && !Bits::hasAnySet(signature.getParameter(minPositional - 1).getFlags(), IFunctionSignatureParameter::Flags::Required)) {
    minPositional--;
  }
  auto actual = parameters.getPositionalCount();
  if (actual < minPositional) {
    if (minPositional == 1) {
      return execution.raiseFormat(Function::signatureToString(signature, Parts::All), ": At least 1 parameter was expected");
    }
    return execution.raiseFormat(Function::signatureToString(signature, Parts::All), ": At least ", minPositional, " parameters were expected, not ", actual);
  }
  if ((maxPositional > 0) && Bits::hasAnySet(signature.getParameter(maxPositional - 1).getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
    // TODO Variadic
  } else if (actual > maxPositional) {
    // Not variadic
    if (maxPositional == 1) {
      return execution.raiseFormat(Function::signatureToString(signature, Parts::All), ": Only 1 parameter was expected, not ", actual);
    }
    return execution.raiseFormat(Function::signatureToString(signature, Parts::All), ": No more than ", maxPositional, " parameters were expected, not ", actual);
  }
  return Variant::Void;
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

egg::ovum::Type egg::ovum::Type::makePointer(IAllocator& allocator, const IType& pointee) {
  // The default implementation is to return a new type 'Type*'
  return allocator.make<TypePointer, Type>(pointee);
}

egg::ovum::BasalBits egg::ovum::TypeBase::getBasalTypes() const {
  // By default, we are an object
  return BasalBits::Object;
}

egg::ovum::IType::AssignmentSuccess egg::ovum::TypeBase::canBeAssignedFrom(const IType&) const {
  // By default, we do not allow assignment
  return AssignmentSuccess::Never;
}

egg::ovum::Variant egg::ovum::TypeBase::promoteAssignment(const Variant& rhs) const {
  // By default, call canBeAssignedFrom() but do not actually promote
  auto& rvalue = rhs.direct();
  auto rtype = rvalue.getRuntimeType();
  if (this->canBeAssignedFrom(*rtype) == AssignmentSuccess::Never) {
    return makeException("Cannot assign a value of type '", rtype.toString(), "' to a target of type '", Type(this).toString(), "'");
  }
  return rvalue;
}

const egg::ovum::IFunctionSignature* egg::ovum::TypeBase::callable() const {
  // By default, we are not callable
  return nullptr;
}

const egg::ovum::IIndexSignature* egg::ovum::TypeBase::indexable() const {
  // By default, we are not indexable
  return nullptr;
}

egg::ovum::Type egg::ovum::TypeBase::dotable(const String*, String& error) const {
  // By default, we have no properties
  error = StringBuilder::concat("Values of type '", Type(this).toString(), "*' do not support the '.' operator for property access");
  return nullptr;
}

egg::ovum::Type egg::ovum::TypeBase::iterable() const {
  // By default, we are not iterable
  return nullptr;
}

egg::ovum::Type egg::ovum::TypeBase::pointeeType() const {
  // By default, we do not point to anything
  return nullptr;
}

egg::ovum::Type egg::ovum::TypeBase::denulledType() const {
  // By default, we do not hold null values
  return Type(this);
}

egg::ovum::Type egg::ovum::TypeBase::unionWithBasal(IAllocator&, BasalBits) const {
  // By default we cannot simple union with basal types
  return nullptr;
}

// Common types
const egg::ovum::Type egg::ovum::Type::Void{ &typeVoid };
const egg::ovum::Type egg::ovum::Type::Null{ &typeNull };
const egg::ovum::Type egg::ovum::Type::Bool{ &typeBool };
const egg::ovum::Type egg::ovum::Type::Int{ &typeInt };
const egg::ovum::Type egg::ovum::Type::Float{ &typeFloat };
const egg::ovum::Type egg::ovum::Type::String{ &typeString };
const egg::ovum::Type egg::ovum::Type::Object{ &typeObject };
const egg::ovum::Type egg::ovum::Type::Arithmetic{ &typeArithmetic };
const egg::ovum::Type egg::ovum::Type::Any{ &typeAny };
const egg::ovum::Type egg::ovum::Type::AnyQ{ &typeAnyQ };
