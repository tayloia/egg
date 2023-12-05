#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  class TypePrimitive final : public SoftReferenceCountedNone<IType> {
    TypePrimitive(const TypePrimitive&) = delete;
    TypePrimitive& operator=(const TypePrimitive&) = delete;
  private:
    static const size_t trivials = size_t(1 << int(ValueFlagsShift::UBound));
    static TypePrimitive trivial[trivials];
    Atomic<ValueFlags> flags;
    TypePrimitive()
      : flags(ValueFlags::None) {
      // Private construction used for TypePrimitive::trivial
    }
  public:
    virtual bool isPrimitive() const override {
      return true;
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return this->flags.get();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      std::stringstream ss;
      auto precedence = Print::describe(ss, this->flags.get(), Print::Options::DEFAULT);
      return std::make_pair(ss.str(), precedence);
    }
    virtual void print(Printer& printer) const override {
      (void)Print::describe(printer.stream, this->flags.get(), Print::Options::DEFAULT);
    }
    static const IType* forge(ValueFlags flags) {
      auto index = size_t(flags);
      if (index < TypePrimitive::trivials) {
        auto& entry = TypePrimitive::trivial[index];
        (void)entry.flags.exchange(flags);
        return &entry;
      }
      return nullptr;
    }
  };
  TypePrimitive TypePrimitive::trivial[] = {};

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
    virtual Assignability isTypeAssignable(const Type& dst, const Type& src) override {
      // TODO complex types
      auto dstFlags = dst->getPrimitiveFlags();
      auto srcFlags = src->getPrimitiveFlags();
      if (Bits::hasAllSet(dstFlags, srcFlags)) {
        return Assignability::Always;
      }
      if (Bits::hasAnySet(srcFlags, ValueFlags::Int) && Bits::hasAnySet(dstFlags, ValueFlags::Float)) {
        // Promote integers to floats
        srcFlags = Bits::set(Bits::clear(srcFlags, ValueFlags::Int), ValueFlags::Float);
        if (Bits::hasAllSet(dstFlags, srcFlags)) {
          return Assignability::Always;
        }
      }
      if (Bits::hasAnySet(dstFlags, srcFlags)) {
        return Assignability::Sometimes;
      }
      return Assignability::Never;
    }
    virtual Type forgePrimitiveType(ValueFlags flags) override {
      auto trivial = TypePrimitive::forge(flags);
      assert(trivial != nullptr);
      return trivial;
    }
    virtual Type forgeNullableType(const Type& type, bool nullable) override {
      auto before = type->getPrimitiveFlags();
      if (nullable) {
        if (!Bits::hasAnySet(before, ValueFlags::Null)) {
          return this->forgePrimitiveType(Bits::set(before, ValueFlags::Null));
        }
      } else {
        if (Bits::hasAnySet(before, ValueFlags::Null)) {
          return this->forgePrimitiveType(Bits::clear(before, ValueFlags::Null));
        }
      }
      return type;
    }
  };
}

// Common types
const Type Type::None{ TypePrimitive::forge(ValueFlags::None) };
const Type Type::Void{ TypePrimitive::forge(ValueFlags::Void) };
const Type Type::Null{ TypePrimitive::forge(ValueFlags::Null) };
const Type Type::Bool{ TypePrimitive::forge(ValueFlags::Bool) };
const Type Type::Int{ TypePrimitive::forge(ValueFlags::Int) };
const Type Type::Float{ TypePrimitive::forge(ValueFlags::Float) };
const Type Type::String{ TypePrimitive::forge(ValueFlags::String) };
const Type Type::Arithmetic{ TypePrimitive::forge(ValueFlags::Arithmetic) };
const Type Type::Object{ TypePrimitive::forge(ValueFlags::Object) };
const Type Type::Any{ TypePrimitive::forge(ValueFlags::Any) };
const Type Type::AnyQ{ TypePrimitive::forge(ValueFlags::AnyQ) };

egg::ovum::HardPtr<egg::ovum::ITypeForge> egg::ovum::TypeForgeFactory::createTypeForge(IAllocator& allocator, IBasket& basket) {
  return allocator.makeHard<TypeForgeDefault>(basket);
}
