#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  const char* simpleComponent(ValueFlags flags) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (flags) {
      case ValueFlags::None:
        return "var";
#define EGG_OVUM_VALUE_FLAGS_COMPONENT(name, text) case ValueFlags::name: return text;
        EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_COMPONENT)
#undef EGG_OVUM_VALUE_FLAGS_COMPONENT
      case ValueFlags::Any:
        return "any";
      }
    EGG_WARNING_SUPPRESS_SWITCH_END
      return nullptr;
  }

  std::pair<std::string, int> simpleToStringPrecedence(ValueFlags flags) {
    auto* component = simpleComponent(flags);
    if (component != nullptr) {
      return { component, 0 };
    }
    if (Bits::hasAnySet(flags, ValueFlags::Null)) {
      auto nonnull = simpleToStringPrecedence(Bits::clear(flags, ValueFlags::Null));
      return { nonnull.first + "?", std::max(nonnull.second, 1) };
    }
    if (Bits::hasAnySet(flags, ValueFlags::Void)) {
      return { "void|" + simpleToStringPrecedence(Bits::clear(flags, ValueFlags::Void)).first, 2 };
    }
    auto head = Bits::topmost(flags);
    assert(head != ValueFlags::None);
    component = simpleComponent(head);
    assert(component != nullptr);
    return { simpleToStringPrecedence(Bits::clear(flags, head)).first + '|' + component, 2 };
  }

  template<enum ValueFlags FLAGS>
  class TypePrimitive final : public NotHardReferenceCounted<IType> {
    TypePrimitive(const TypePrimitive&) = delete;
    TypePrimitive& operator=(const TypePrimitive&) = delete;
  public:
    TypePrimitive() {}
    virtual ValueFlags getPrimitiveFlags() const override {
      return FLAGS;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return simpleToStringPrecedence(FLAGS);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  TypePrimitive<ValueFlags::None> typeNone{};
  TypePrimitive<ValueFlags::Void> typeVoid{};
  TypePrimitive<ValueFlags::Null> typeNull{};
  TypePrimitive<ValueFlags::Bool> typeBool{};
  TypePrimitive<ValueFlags::Bool | ValueFlags::Int> typeBoolInt{};
  TypePrimitive<ValueFlags::Int> typeInt{};
  TypePrimitive<ValueFlags::Null | ValueFlags::Int> typeIntQ{};
  TypePrimitive<ValueFlags::Float> typeFloat{};
  TypePrimitive<ValueFlags::Arithmetic> typeArithmetic{};
  TypePrimitive<ValueFlags::String> typeString{};
  TypePrimitive<ValueFlags::Object> typeObject{};
  TypePrimitive<ValueFlags::Any> typeAny{};
  TypePrimitive<ValueFlags::AnyQ> typeAnyQ{};
}

String Type::describeValue() const {
  auto* type = this->get();
  if (type == nullptr) {
    return "Value of unknown type";
  }
  return type->describeValue();
}

String Type::toString() const {
  auto* type = this->get();
  if (type == nullptr) {
    return "<unknown>";
  }
  return type->toStringPrecedence().first;
}

bool Type::areEquivalent(const IType& lhs, const IType& rhs) {
  return &lhs == &rhs;
}

bool Type::areEquivalent(const IFunctionSignature& lhs, const IFunctionSignature& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  auto parameters = lhs.getParameterCount();
  if ((parameters != rhs.getParameterCount()) || !lhs.getReturnType().isEquivalent(rhs.getReturnType())) {
    return false;
  }
  for (size_t parameter = 0; parameter < parameters; ++parameter) {
    // TODO optional parameter relaxation?
    if (!Type::areEquivalent(lhs.getParameter(parameter), rhs.getParameter(parameter))) {
      return false;
    }
  }
  return true;
}

bool Type::areEquivalent(const IFunctionSignatureParameter& lhs, const IFunctionSignatureParameter& rhs) {
  // TODO variadic/optional/etc
  if (&lhs == &rhs) {
    return true;
  }
  return (lhs.getPosition() == rhs.getPosition()) && (lhs.getFlags() == rhs.getFlags()) && (lhs.getType().isEquivalent(rhs.getType()));
}

bool Type::areEquivalent(const IPropertySignature& lhs, const IPropertySignature& rhs) {
  // TODO
  if (&lhs == &rhs) {
    return true;
  }
  return true;
}

bool Type::areEquivalent(const IIndexSignature& lhs, const IIndexSignature& rhs) {
  // TODO modifiability
  if (&lhs == &rhs) {
    return true;
  }
  return areEquivalent(lhs.getIndexType(), rhs.getIndexType()) && areEquivalent(lhs.getResultType(), rhs.getResultType());
}

bool Type::areEquivalent(const IPointerSignature& lhs, const IPointerSignature& rhs) {
  // TODO modifiability
  if (&lhs == &rhs) {
    return true;
  }
  return lhs.getType().isEquivalent(rhs.getType());
}

bool Type::areEquivalent(const IIteratorSignature& lhs, const IIteratorSignature& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  return lhs.getType().isEquivalent(rhs.getType());
}

// Common types
const Type Type::None{ &typeNone };
const Type Type::Void{ &typeVoid };
const Type Type::Null{ &typeNull };
const Type Type::Bool{ &typeBool };
const Type Type::BoolInt{ &typeBoolInt };
const Type Type::Int{ &typeInt };
const Type Type::IntQ{ &typeIntQ };
const Type Type::Float{ &typeFloat };
const Type Type::String{ &typeString };
const Type Type::Arithmetic{ &typeArithmetic };
const Type Type::Object{ &typeObject };
const Type Type::Any{ &typeAny };
const Type Type::AnyQ{ &typeAnyQ };
