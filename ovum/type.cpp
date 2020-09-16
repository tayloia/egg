#include "ovum/ovum.h"
#include "ovum/node.h"

namespace {
  using namespace egg::ovum;

  const char* flagsComponent(ValueFlags flags) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (flags) {
      case ValueFlags::None:
        return "var";
#define EGG_OVUM_VALUE_FLAGS_COMPONENT(name, text) case ValueFlags::name: return text;
        EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_COMPONENT)
#undef EGG_OVUM_VALUE_FLAGS_COMPONENT
      case ValueFlags::Any:
        return "any";
      }
    EGG_WARNING_SUPPRESS_SWITCH_END();
      return nullptr;
  }

  std::string flagsToString(ValueFlags flags) {
    auto* component = flagsComponent(flags);
    if (component != nullptr) {
      return component;
    }
    if (Bits::hasAnySet(flags, ValueFlags::Null)) {
      return flagsToString(Bits::clear(flags, ValueFlags::Null)) + "?";
    }
    auto head = Bits::topmost(flags);
    assert(head != ValueFlags::None);
    assert(head != ValueFlags::Pointer);
    component = flagsComponent(head);
    assert(component != nullptr);
    return flagsToString(Bits::clear(flags, head)) + '|' + component;
  }

  std::pair<std::string, int> flagsToStringPrecedence(ValueFlags flags) {
    // Adjust the precedence of the result if the string has '|' (union) in it
    // This is so that parentheses can be added appropriately to handle "(void|int)*" versus "void|int*"
    auto result = std::make_pair(flagsToString(flags), 0);
    if (result.first.find('|') != std::string::npos) {
      result.second--;
    }
    return result;
  }

  IType::Assignable assignableFlags(ValueFlags lhs, ValueFlags rhs) {
    if (Bits::hasAllSet(lhs, rhs)) {
      return IType::Assignable::Always;
    }
    if (Bits::hasAnySet(lhs, rhs)) {
      return IType::Assignable::Sometimes;
    }
    return IType::Assignable::Never;
  }

  Type makeUnionJoin(IAllocator& allocator, const IType& lhs, const IType& rhs);

  Type makeUnionFlags(IAllocator& allocator, ValueFlags flhs, const IType& lhs, const IType& rhs) {
    // Check for identity early
    if (&lhs == &rhs) {
      return Type(&lhs);
    }
    auto frhs = rhs.getFlags();
    if (Bits::hasAnySet(frhs, ValueFlags::Pointer)) {
      return makeUnionJoin(allocator, lhs, rhs);
    }
    if (Bits::hasAllSet(frhs, flhs)) {
      // The rhs is a superset of the types in lhs, just return rhs
      return Type(&rhs);
    }
    if (Bits::hasAnySet(frhs, ValueFlags::Object)) {
      // The superset check above should ensure this will not result in infinite recursion
      return rhs.makeUnion(allocator, lhs);
    }
    return Type::makeSimple(allocator, flhs | frhs);
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

  class TypeBase : public IType {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  public:
    TypeBase() = default;
    virtual const IFunctionSignature* callable() const override {
      // By default, we are not callable
      return nullptr;
    }
    virtual const IIndexSignature* indexable() const override {
      // By default, we are not indexable
      return nullptr;
    }
    virtual const IDotSignature* dotable() const override {
      // By default, we are not dotable
      return nullptr;
    }
    virtual const IPointSignature* pointable() const override {
      // By default, we are not pointable
      return nullptr;
    }
  };

  template<ValueFlags FLAGS>
  class TypeCommon : public NotReferenceCounted<TypeBase> {
    TypeCommon(const TypeCommon&) = delete;
    TypeCommon& operator=(const TypeCommon&) = delete;
  public:
    TypeCommon() = default;
    virtual ValueFlags getFlags() const override {
      return FLAGS;
    }
    virtual Type makeUnion(IAllocator& allocator, const IType& rhs) const override {
      return makeUnionFlags(allocator, FLAGS, *this, rhs);
    }
    virtual Assignable assignable(const IType& rhs) const override {
      return assignableFlags(FLAGS, rhs.getFlags());
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return flagsToStringPrecedence(FLAGS);
    }
  };
  const TypeCommon<ValueFlags::Bool> typeBool{};
  const TypeCommon<ValueFlags::Int> typeInt{};
  const TypeCommon<ValueFlags::Float> typeFloat{};
  const TypeCommon<ValueFlags::Int | ValueFlags::Float> typeArithmetic{};

  class TypeVoid : public TypeCommon<ValueFlags::Void> {
    TypeVoid(const TypeVoid&) = delete;
    TypeVoid& operator=(const TypeVoid&) = delete;
  public:
    TypeVoid() = default;
    virtual Assignable assignable(const IType&) const override {
      return Assignable::Never;
    }
  };
  const TypeVoid typeVoid{};

  class TypeNull : public TypeCommon<ValueFlags::Null> {
    TypeNull(const TypeNull&) = delete;
    TypeNull& operator=(const TypeNull&) = delete;
  public:
    TypeNull() = default;
    virtual Assignable assignable(const IType&) const override {
      return Assignable::Never;
    }
  };
  const TypeNull typeNull{};

  class TypeString : public TypeCommon<ValueFlags::String> {
    TypeString(const TypeString&) = delete;
    TypeString& operator=(const TypeString&) = delete;
  public:
    TypeString() = default;
  };
  const TypeString typeString{};

  class TypeObject : public TypeCommon<ValueFlags::Object> {
    TypeObject(const TypeObject&) = delete;
    TypeObject& operator=(const TypeObject&) = delete;
  public:
    TypeObject() = default;
    virtual const IFunctionSignature* callable() const override {
      return &omniFunctionSignature;
    }
  };
  const TypeObject typeObject{};

  class TypeMemory : public TypeCommon<ValueFlags::Memory> {
    TypeMemory(const TypeMemory&) = delete;
    TypeMemory& operator=(const TypeMemory&) = delete;
  public:
    TypeMemory() = default;
  };
  const TypeMemory typeMemory{};

  class TypeAny : public TypeCommon<ValueFlags::Any> {
    TypeAny(const TypeAny&) = delete;
    TypeAny& operator=(const TypeAny&) = delete;
  public:
    TypeAny() = default;
    virtual const IFunctionSignature* callable() const override {
      return &omniFunctionSignature;
    }
  };
  const TypeAny typeAny{};

  class TypeAnyQ : public TypeCommon<ValueFlags::AnyQ> {
    TypeAnyQ(const TypeAnyQ&) = delete;
    TypeAnyQ& operator=(const TypeAnyQ&) = delete;
  public:
    TypeAnyQ() = default;
    virtual const IFunctionSignature* callable() const override {
      return &omniFunctionSignature;
    }
  };
  const TypeAnyQ typeAnyQ{};

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
    virtual ValueFlags getFlags() const override {
      return this->a->getFlags() | this->b->getFlags();
    }
    virtual Type makeUnion(IAllocator& allocator2, const IType& rhs) const override {
      assert(&this->allocator == &allocator2);
      return makeUnionJoin(allocator2, *this, rhs);
    }
    virtual Assignable assignable(const IType& rhs) const override {
      // The logic is:
      //  Assignable to a Assignable to b Result
      //  =============== =============== ======
      //  Never           Never           Never
      //  Never           Sometimes       Sometimes
      //  Never           Always          Always
      //  Sometimes       Never           Sometimes
      //  Sometimes       Sometimes       Sometimes
      //  Sometimes       Always          Always
      //  Always          Never           Always
      //  Always          Sometimes       Always
      //  Always          Always          Always
      switch (this->a->assignable(rhs)) {
      case Assignable::Never:
        return this->b->assignable(rhs);
      case Assignable::Sometimes:
        break;
      case Assignable::Always:
        return Assignable::Always;
      }
      if (this->b->assignable(rhs) == Assignable::Always) {
        return Assignable::Always;
      }
      return Assignable::Sometimes;
    }
    virtual const IFunctionSignature* callable() const override {
      // TODO What if both have signatures?
      auto* signature = this->a->callable();
      if (signature != nullptr) {
        return signature;
      }
      return this->b->callable();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // TODO
      auto ab = StringBuilder().add(this->a.toString(0), '|', this->b.toString(0)).toUTF8();
      return std::make_pair(ab, 0);
    }
  };

  class TypeSimple : public HardReferenceCounted<TypeBase> {
    TypeSimple(const TypeSimple&) = delete;
    TypeSimple& operator=(const TypeSimple&) = delete;
  private:
    ValueFlags flags;
  public:
    TypeSimple(IAllocator& allocator, ValueFlags flags)
      : HardReferenceCounted(allocator, 0), flags(flags) {
    }
    virtual ValueFlags getFlags() const override {
      return this->flags;
    }
    virtual Type makeUnion(IAllocator& allocator2, const IType& rhs) const override {
      assert(&this->allocator == &allocator2);
      return makeUnionFlags(allocator2, this->flags, *this, rhs);
    }
    virtual Assignable assignable(const IType& rhs) const override {
      return assignableFlags(this->flags, rhs.getFlags());
    }
    virtual const IFunctionSignature* callable() const override {
      if (Bits::hasAnySet(this->flags, ValueFlags::Object)) {
        return &omniFunctionSignature;
      }
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return flagsToStringPrecedence(this->flags);
    }
  };

  class TypePointer : public HardReferenceCounted<TypeBase> {
    TypePointer(const TypePointer&) = delete;
    TypePointer& operator=(const TypePointer&) = delete;
  private:
    Type referenced;
  public:
    TypePointer(IAllocator& allocator, const IType& referenced)
      : HardReferenceCounted(allocator, 0), referenced(&referenced) {
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Pointer;
    }
    virtual Type makeUnion(IAllocator& allocator2, const IType& rhs) const override {
      // TODO elide if similar
      assert(&this->allocator == &allocator2);
      return makeUnionJoin(allocator2, *this, rhs);
    }
    virtual Assignable assignable(const IType&) const override {
      return Assignable::Never; // TODO
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto p = StringBuilder().add(this->referenced.toString(0), '*').toUTF8();
      return std::make_pair(p, 0);
    }
  };

  Type makeUnionJoin(IAllocator& allocator, const IType& lhs, const IType& rhs) {
    return allocator.make<TypeUnion, Type>(lhs, rhs);
  }

}

egg::ovum::Type egg::ovum::Type::makeSimple(IAllocator& allocator, ValueFlags flags) {
  // OPTIMIZE for known types
  return allocator.make<TypeSimple, Type>(flags);
}

egg::ovum::Type egg::ovum::Type::makePointer(IAllocator& allocator, const IType& pointee) {
  // OPTIMIZE with lazy pointer types in values instantiated when address-of is invoked
  return allocator.make<TypePointer, Type>(pointee);
}

egg::ovum::Type egg::ovum::Type::makeUnion(IAllocator& allocator, const IType& a, const IType& b) {
  if (&a == &b) {
    return Type(&a);
  }
  return a.makeUnion(allocator, b);
}

egg::ovum::ITypeFunction* egg::ovum::Type::makeFunction(IAllocator& allocator, const egg::ovum::String& name, const Type& rettype) {
  // WIBBLE
  (void)allocator;
  (void)name;
  (void)rettype;
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
const egg::ovum::Type egg::ovum::Type::Memory{ &typeMemory };
const egg::ovum::Type egg::ovum::Type::Arithmetic{ &typeArithmetic };
const egg::ovum::Type egg::ovum::Type::Any{ &typeAny };
const egg::ovum::Type egg::ovum::Type::AnyQ{ &typeAnyQ };

