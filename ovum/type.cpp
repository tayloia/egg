#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/function.h"

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

  IType::AssignmentSuccess canBeAssignedFromBasalLegacy(BasalBits lhs, const IType& rtype) {
    assert(lhs != BasalBits::None);
    auto rhs = rtype.getBasalTypesLegacy();
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

  Variant tryAssignBasal(IExecution& execution, BasalBits basal, Variant& lhs, const Variant& rhs) {
    assert(basal != BasalBits::None);
    if (rhs.hasAny(static_cast<ValueFlags>(basal))) {
      // It's an exact type match (narrowing)
      lhs = rhs;
      return Variant::Void;
    }
    if (Bits::hasAnySet(basal, BasalBits::Float) && rhs.isInt()) {
      // We allow type promotion int->float
      lhs = Variant(Float(rhs.getInt())); // TODO overflows?
      return Variant::Void;
    }
    return execution.raiseFormat("Cannot assign a value of type '", rhs.getRuntimeType().toString(), "' to a target of type '", Type::getBasalString(basal), "'");
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
    virtual BasalBits getBasalTypesLegacy() const override {
      return BasalBits::None;
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
    virtual Node compile(IAllocator& memallocator, const NodeLocation& location) const override {
      return NodeFactory::createPointerType(memallocator, location, this->referenced);
    }
  };

  template<BasalBits BASAL>
  class TypeCommon : public NotReferenceCounted<TypeBase> {
    TypeCommon(const TypeCommon&) = delete;
    TypeCommon& operator=(const TypeCommon&) = delete;
  public:
    TypeCommon() = default;
    virtual BasalBits getBasalTypesLegacy() const override {
      return BASAL;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override {
      return canBeAssignedFromBasalLegacy(BASAL, rhs);
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return tagToStringPriority(BASAL);
    }
    virtual Variant tryAssign(IExecution& execution, Variant& lvalue, const Variant& rvalue) const override {
      return tryAssignBasal(execution, BASAL, lvalue, rvalue);
    }
  };
  const TypeCommon<BasalBits::Bool> typeBool{};
  const TypeCommon<BasalBits::Int> typeInt{};
  const TypeCommon<BasalBits::Float> typeFloat{};
  const TypeCommon<BasalBits::Int | BasalBits::Float> typeArithmetic{};

  class TypeVoid : public TypeCommon<BasalBits::Void> {
    TypeVoid(const TypeVoid&) = delete;
    TypeVoid& operator=(const TypeVoid&) = delete;
  public:
    TypeVoid() = default;
    virtual Type devoidedType() const override {
      // You cannot devoid void!
      return nullptr;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const override {
      return AssignmentSuccess::Never;
    }
    virtual Variant tryAssign(IExecution& execution, Variant&, const Variant&) const override {
      return execution.raiseFormat("Cannot assign to 'void' value");
    }
  };
  const TypeVoid typeVoid{};

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
    virtual Variant tryAssign(IExecution& execution, Variant&, const Variant&) const override {
      return execution.raiseFormat("Cannot assign to 'null' value");
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
    virtual BasalBits getBasalTypesLegacy() const override {
      return this->tag;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override {
      return canBeAssignedFromBasalLegacy(this->tag, rhs);
    }
    virtual Variant tryAssign(IExecution& execution, Variant& lvalue, const Variant& rvalue) const override {
      return tryAssignBasal(execution, this->tag, lvalue, rvalue);
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
    virtual Type devoidedType() const override {
      auto devoided = Bits::clear(this->tag, BasalBits::Void);
      if (this->tag != devoided) {
        // We need to clear the bit
        return Type::makeBasal(this->allocator, devoided);
      }
      return Type(this);
    }
    virtual Type denulledType() const override {
      auto denulled = Bits::clear(this->tag, BasalBits::Null);
      if (this->tag != denulled) {
        // We need to clear the bit
        return Type::makeBasal(this->allocator, denulled);
      }
      return Type(this);
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
    virtual BasalBits getBasalTypesLegacy() const override {
      return this->a->getBasalTypesLegacy() | this->b->getBasalTypesLegacy(); // TODO
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // TODO
      auto sa = this->a->toStringPrecedence().first;
      auto sb = this->b->toStringPrecedence().first;
      return std::make_pair(sa + "|" + sb, -1);
    }
  };

  const char* getBasalComponent(BasalBits basal) {
    switch (basal) {
    case BasalBits::None:
      return "var";
#define EGG_VM_BASAL_COMPONENT(name, text) case BasalBits::name: return text;
      EGG_VM_BASAL(EGG_VM_BASAL_COMPONENT)
#undef EGG_VM_BASAL_COMPONENT
    case BasalBits::Any:
      return "any";
    case BasalBits::AnyQ:
      // Non-trivial
      break;
    }
    return nullptr;
  }
}

const egg::ovum::IType* egg::ovum::Type::getBasalType(BasalBits basal) {
  switch (basal) {
  case BasalBits::None:
    break;
#define EGG_VM_BASAL_INSTANCE(name, text) case BasalBits::name: return &type##name;
    EGG_VM_BASAL(EGG_VM_BASAL_INSTANCE)
#undef EGG_VM_BASAL_INSTANCE
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
  auto sa = a.getBasalTypesLegacy();
  auto sb = b.getBasalTypesLegacy();
  if ((sa != BasalBits::None) && (sb != BasalBits::None)) {
    return Type::makeBasal(allocator, sa | sb);
  }
  return allocator.make<TypeUnion, Type>(a, b);
}

egg::ovum::Type egg::ovum::Type::makePointer(IAllocator& allocator, const IType& pointee) {
  // Return a new type 'Type*'
  return allocator.make<TypePointer, Type>(pointee);
}

egg::ovum::Variant egg::ovum::TypeBase::tryAssign(IExecution& execution, Variant& lvalue, const Variant& rvalue) const {
  // By default, call canBeAssignedFrom() but do not actually promote
  auto rtype = rvalue.getRuntimeType();
  if (this->canBeAssignedFrom(*rtype) == AssignmentSuccess::Never) {
    return execution.raiseFormat("Cannot assign a value of type '", rtype.toString(), "' to a target of type '", Type(this).toString(), "'");
  }
  lvalue = rvalue;
  return Variant::Void;
}

bool egg::ovum::TypeBase::hasBasalType(BasalBits basal) const {
  // By default, we interrogate the legacy functionality
  return Bits::hasAnySet(this->getBasalTypesLegacy(), basal);
}

egg::ovum::IType::AssignmentSuccess egg::ovum::TypeBase::canBeAssignedFrom(const IType&) const {
  // By default, we do not allow assignment
  return AssignmentSuccess::Never;
}

const egg::ovum::IFunctionSignature* egg::ovum::TypeBase::callable() const {
  // By default, we are not callable
  return nullptr;
}

const egg::ovum::IIndexSignature* egg::ovum::TypeBase::indexable() const {
  // By default, we are not indexable
  return nullptr;
}

egg::ovum::Type egg::ovum::TypeBase::dotable(const String&, String& error) const {
  // By default, we have no properties
  error = StringBuilder::concat("Values of type '", Type(this).toString(), "' do not support the '.' operator for property access");
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

egg::ovum::Type egg::ovum::TypeBase::devoidedType() const {
  // By default, we do not hold void values
  return Type(this);
}

egg::ovum::Type egg::ovum::TypeBase::denulledType() const {
  // By default, we do not hold null values
  return Type(this);
}

egg::ovum::Node egg::ovum::TypeBase::compile(IAllocator& allocator, const NodeLocation& location) const {
  // By default we construct a basal type node tree
  return NodeFactory::createBasalType(allocator, location, this->getBasalTypesLegacy());
}

egg::ovum::BasalBits egg::ovum::TypeBase::getBasalTypesLegacy() const {
  // By default, we are an object
  return BasalBits::Object;
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
