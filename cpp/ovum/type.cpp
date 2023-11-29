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

  int simpleToStringPrecedence(std::ostream& os, ValueFlags flags) {
    auto* component = simpleComponent(flags);
    if (component != nullptr) {
      os << component;
      return 0;
    }
    if (Bits::hasAnySet(flags, ValueFlags::Null)) {
      auto nonnull = simpleToStringPrecedence(os, Bits::clear(flags, ValueFlags::Null));
      os << '?';
      return std::max(nonnull, 1);
    }
    if (Bits::hasAnySet(flags, ValueFlags::Void)) {
      os << "void|";
      (void)simpleToStringPrecedence(os, Bits::clear(flags, ValueFlags::Void));
      return 2;
    }
    auto head = Bits::topmost(flags);
    assert(head != ValueFlags::None);
    component = simpleComponent(head);
    assert(component != nullptr);
    os << component << '|';
    (void)simpleToStringPrecedence(os, Bits::clear(flags, head));
    return 2;
  }

  template<enum ValueFlags FLAGS>
  class TypePrimitive final : public SoftReferenceCountedNone<IType> {
    TypePrimitive(const TypePrimitive&) = delete;
    TypePrimitive& operator=(const TypePrimitive&) = delete;
  public:
    TypePrimitive() {}
    virtual ValueFlags getPrimitiveFlags() const override {
      return FLAGS;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      std::stringstream ss;
      auto precedence = simpleToStringPrecedence(ss, FLAGS);
      return std::make_pair(ss.str(), precedence);
    }
    virtual void print(Printer& printer) const override {
      (void)simpleToStringPrecedence(printer.stream(), FLAGS);
    }
  };

  class TypeForgeDefault : public HardReferenceCountedAllocator<ITypeForge> {
    TypeForgeDefault(const TypeForgeDefault&) = delete;
    TypeForgeDefault& operator=(const TypeForgeDefault&) = delete;
  private:
    HardPtr<IBasket> basket;
  public:
    explicit TypeForgeDefault(IAllocator& allocator, IBasket& basket)
      : HardReferenceCountedAllocator(allocator),
        basket(&basket) {
    }
    virtual Assignability isAssignable(const IType& dst, const IType& src) override {
      // WIBBLE
      if (&dst == &src) {
        return Assignability::Always;
      }
      if ((&dst == &*Type::Float) && (&src == &*Type::Int)) {
        return Assignability::Always;
      }
      return Assignability::Never;
    }
    virtual const IType& getPrimitive(ValueFlags flags) override {
      // WIBBLE
      (void)flags;
      return *Type::AnyQ;
    }
    virtual const IType& setNullability(const IType& type, bool nullable) override {
      auto before = type.getPrimitiveFlags();
      if (nullable) {
        if (!Bits::hasAnySet(before, ValueFlags::Null)) {
          return this->getPrimitive(Bits::set(before, ValueFlags::Null));
        }
      } else {
        if (Bits::hasAnySet(before, ValueFlags::Null)) {
          return this->getPrimitive(Bits::clear(before, ValueFlags::Null));
        }
      }
      return type;
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

egg::ovum::HardPtr<egg::ovum::ITypeForge> egg::ovum::TypeForgeFactory::createTypeForge(IAllocator& allocator, IBasket& basket) {
  return allocator.makeHard<TypeForgeDefault>(basket);

}