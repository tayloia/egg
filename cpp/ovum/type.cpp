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
    virtual const TypeShape* getObjectShape(size_t) const override {
      // By default, we are not shaped like objects
      return nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      // By default, we are not shaped like objects
      return 0;
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

  template<typename T, typename F>
  const T* queryObjectShape(ITypeFactory& factory, const Type& type, F field) {
    assert(type != nullptr);
    auto count = type->getObjectShapeCount();
    assert(count <= 1);
    if (count > 0) {
      auto* shape = type->getObjectShape(0);
      if (shape != nullptr) {
        auto* result = shape->*field;
        if (result != nullptr) {
          return result;
        }
      }
    }
    auto flags = type->getPrimitiveFlags();
    if (Bits::hasAnySet(flags, ValueFlags::Object)) {
      auto* result = factory.getObjectShape().*field;
      if (result != nullptr) {
        return result;
      }
    }
    if (Bits::hasAnySet(flags, ValueFlags::String)) {
      return factory.getStringShape().*field;
    }
    return nullptr;
  }

  Type::Assignability queryAssignableObject(const IType& ltype, const IType& rtype) {
    // WOBBLE shapes
    (void)ltype;
    (void)rtype;
    return Type::Assignability::Always;
  }

  Type::Assignability queryAssignablePrimitive(const IType& ltype, const IType& rtype, ValueFlags lflags, ValueFlags rflag) {
    assert(Bits::hasOneSet(rflag, ValueFlags::Void | ValueFlags::AnyQ));
    auto result = Type::Assignability::Never;
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (rflag) {
    case ValueFlags::Void:
      break;
    case ValueFlags::Null:
    case ValueFlags::Bool:
    case ValueFlags::Float:
    case ValueFlags::String:
      if (Bits::hasAnySet(lflags, rflag)) {
        result = Type::Assignability::Always;
      }
      break;
    case ValueFlags::Int:
      if (Bits::hasAnySet(lflags, ValueFlags::Int)) {
        result = Type::Assignability::Always;
      } else if (Bits::hasAnySet(lflags, ValueFlags::Float)) {
        // Permit type promotion int->float
        result = Type::Assignability::Sometimes;
      }
      break;
    case ValueFlags::Object:
      if (Bits::hasAnySet(lflags, ValueFlags::Object)) {
        result = queryAssignableObject(ltype, rtype);
      }
      break;
    default:
      assert(0); // not a type within 'any?'
      break;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    return result;
  }

  Type::Assignability queryAssignablePrimitives(const IType& ltype, const IType& rtype, ValueFlags lflags, ValueFlags rflags) {
    assert(Bits::hasAnySet(rflags, ValueFlags::Void | ValueFlags::AnyQ));
    if (Bits::hasOneSet(rflags, ValueFlags::Void | ValueFlags::AnyQ)) {
      return queryAssignablePrimitive(ltype, rtype, lflags, rflags);
    }
    auto rflag = Bits::topmost(rflags);
    assert(rflag != ValueFlags::None);
    auto assignability = queryAssignablePrimitive(ltype, rtype, lflags, rflag);
    if (assignability == Type::Assignability::Sometimes) {
      return Type::Assignability::Sometimes;
    }
    rflags = Bits::clear(rflags, rflag);
    rflag = Bits::topmost(rflags);
    while (rflag != ValueFlags::None) {
      if (queryAssignablePrimitive(ltype, rtype, lflags, rflag) != assignability) {
        return Type::Assignability::Sometimes;
      }
      rflags = Bits::clear(rflags, rflag);
      rflag = Bits::topmost(rflags);
    }
    return assignability;
  }

  Type::Assignability queryAssignableType(const IType& ltype, const IType& rtype) {
    // TODO complex
    // Composability is like this:
    //  ALWAYS + ALWAYS = ALWAYS
    //  ALWAYS + SOMETIMES = SOMETIMES
    //  ALWAYS + NEVER = SOMETIMES
    //  SOMETIMES + ALWAYS = SOMETIMES
    //  SOMETIMES + SOMETIMES = SOMETIMES
    //  SOMETIMES + NEVER = SOMETIMES
    //  NEVER + ALWAYS = SOMETIMES
    //  NEVER + SOMETIMES = SOMETIMES
    //  NEVER + NEVER = NEVER
    // which is:
    //  (a == b) ? a : SOMETIMES
    auto rflags = rtype.getPrimitiveFlags();
    if (Bits::hasAnySet(rflags, ValueFlags::Void | ValueFlags::AnyQ)) {
      // We have at least one primitive type and possibly object shapes
      auto lflags = ltype.getPrimitiveFlags();
      auto assignability = queryAssignablePrimitives(ltype, rtype, lflags, rflags);
      if (assignability == Type::Assignability::Sometimes) {
        return Type::Assignability::Sometimes;
      }
      if ((rtype.getObjectShapeCount() > 0) && (queryAssignableObject(ltype, rtype) != assignability)) {
        return Type::Assignability::Sometimes;
      }
      return assignability;
    }
    if (rtype.getObjectShapeCount() > 0) {
      // We have no primitive types, but at least one object shape
      return queryAssignableObject(ltype, rtype);
    }
    throw std::logic_error("Type has no known shape");
  }
}

bool TypeShape::equals(const TypeShape& rhs) const {
  auto result = Type::areEquivalent(*this, rhs);
  assert(result == Type::areEquivalent(rhs, *this)); // sanity check
  return result;
}

Type::Assignability Type::queryAssignable(const IType& from) const {
  auto* to = this->get();
  assert(to != nullptr);
  return queryAssignableType(*to, from);
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

const IFunctionSignature* ITypeFactory::queryCallable(const Type& type) {
  return queryObjectShape<IFunctionSignature>(*this, type, &TypeShape::callable);
}

const IPropertySignature* ITypeFactory::queryDotable(const Type& type) {
  return queryObjectShape<IPropertySignature>(*this, type, &TypeShape::dotable);
}

const IIndexSignature* ITypeFactory::queryIndexable(const Type& type) {
  return queryObjectShape<IIndexSignature>(*this, type, &TypeShape::indexable);
}

const IIteratorSignature* ITypeFactory::queryIterable(const Type& type) {
  return queryObjectShape<IIteratorSignature>(*this, type, &TypeShape::iterable);
}

const IPointerSignature* ITypeFactory::queryPointable(const Type& type) {
  return queryObjectShape<IPointerSignature>(*this, type, &TypeShape::pointable);
}

bool Type::areEquivalent(const IType& lhs, const IType& rhs) {
  return &lhs == &rhs;
}

bool Type::areEquivalent(const TypeShape& lhs, const TypeShape& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  return Type::areEquivalent(lhs.callable, rhs.callable) &&
         Type::areEquivalent(lhs.dotable, rhs.dotable) &&
         Type::areEquivalent(lhs.indexable, rhs.indexable) &&
         Type::areEquivalent(lhs.iterable, rhs.iterable) &&
         Type::areEquivalent(lhs.pointable, rhs.pointable);
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

Type::Assignment Type::promote(IAllocator& allocator, const Value& rhs, Value& out) const {
  // WOBBLE
  assert(rhs.validate());
  switch (rhs->getFlags()) {
  case ValueFlags::Void:
    return Assignment::Incompatible;
  case ValueFlags::Null:
    if (!this->hasPrimitiveFlag(ValueFlags::Null)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Bool:
    if (!this->hasPrimitiveFlag(ValueFlags::Bool)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Int:
    if (!this->hasPrimitiveFlag(ValueFlags::Int)) {
      if (this->hasPrimitiveFlag(ValueFlags::Float)) {
        egg::ovum::Int i;
        if (rhs->getInt(i)) {
          // Attempt int-to-float promotion
          auto f = egg::ovum::Float(i);
          if (egg::ovum::Int(f) == i) {
            // Can promote the integer to a float
            out = ValueFactory::createFloat(allocator, f);
            return Assignment::Success;
          }
          return Assignment::BadIntToFloat;
        }
      }
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Float:
    if (!this->hasPrimitiveFlag(ValueFlags::Float)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::String:
    if (!this->hasPrimitiveFlag(ValueFlags::String)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Object:
    // TODO
    if (this->isComplex()) {
      // TODO
      return Assignment::Incompatible;
    } else if (!this->hasPrimitiveFlag(ValueFlags::Object)) {
      // We cannot assign any object-like value to this type
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::None:
  case ValueFlags::Any:
  case ValueFlags::AnyQ:
  case ValueFlags::Arithmetic:
  case ValueFlags::FlowControl:
  case ValueFlags::Break:
  case ValueFlags::Continue:
  case ValueFlags::Return:
  case ValueFlags::Yield:
  case ValueFlags::Throw:
  default:
    return Assignment::Incompatible;
  }
  out = rhs;
  return Assignment::Success;
}

namespace {
  using namespace egg::ovum;

  Type::Assignment mutateDelta(IAllocator& allocator, const Value& lhs, Int rhs, Value& out) {
    Int i;
    if (!lhs->getInt(i)) {
      return Type::Assignment::Incompatible;
    }
    out = ValueFactory::createInt(allocator, i + rhs);
    return Type::Assignment::Success;
  }

  Type::Assignment mutateBool(IAllocator&, Bool lhs, Bool rhs, Mutation mutation, Value& out) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (mutation) {
    case Mutation::BitwiseAnd:
    case Mutation::LogicalAnd:
      // No harm in using the short-circuit form (MSVC complains less too!)
      out = ValueFactory::createBool(lhs && rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseOr:
    case Mutation::LogicalOr:
      // No harm in using the short-circuit form (MSVC complains less too!)
      out = ValueFactory::createBool(lhs || rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseXor:
      out = ValueFactory::createBool(lhs ^ rhs);
      return Type::Assignment::Success;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    throw std::logic_error("Unsupported bool mutation");
  }

  Type::Assignment mutateInt(IAllocator& allocator, Int lhs, Int rhs, Mutation mutation, Value& out) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (mutation) {
    case Mutation::Add:
      out = ValueFactory::createInt(allocator, lhs + rhs);
      return Type::Assignment::Success;
    case Mutation::Subtract:
      out = ValueFactory::createInt(allocator, lhs - rhs);
      return Type::Assignment::Success;
    case Mutation::Multiply:
      out = ValueFactory::createInt(allocator, lhs * rhs);
      return Type::Assignment::Success;
    case Mutation::Divide:
      out = ValueFactory::createInt(allocator, lhs / rhs);
      return Type::Assignment::Success;
    case Mutation::Remainder:
      out = ValueFactory::createInt(allocator, lhs % rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseAnd:
      out = ValueFactory::createInt(allocator, lhs & rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseOr:
      out = ValueFactory::createInt(allocator, lhs | rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseXor:
      out = ValueFactory::createInt(allocator, lhs ^ rhs);
      return Type::Assignment::Success;
    case Mutation::ShiftLeft:
      if (rhs < 0) {
        out = ValueFactory::createInt(allocator, lhs >> -rhs);
      } else {
        out = ValueFactory::createInt(allocator, lhs << rhs);
      }
      return Type::Assignment::Success;
    case Mutation::ShiftRight:
      if (rhs < 0) {
        out = ValueFactory::createInt(allocator, lhs << -rhs);
      } else {
        out = ValueFactory::createInt(allocator, lhs >> rhs);
      }
      return Type::Assignment::Success;
    case Mutation::ShiftRightUnsigned:
      if (rhs < 0) {
        out = ValueFactory::createInt(allocator, lhs << -rhs);
      } else {
        out = ValueFactory::createInt(allocator, Int(uint64_t(lhs) >> rhs));
      }
      return Type::Assignment::Success;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    throw std::logic_error("Unsupported int mutation");
  }

  Type::Assignment mutateFloat(IAllocator& allocator, Float lhs, Float rhs, Mutation mutation, Value& out) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (mutation) {
    case Mutation::Add:
      out = ValueFactory::createFloat(allocator, lhs + rhs);
      return Type::Assignment::Success;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    throw std::logic_error("Unsupported float mutation");
  }

  Type::Assignment mutateArithmetic(IAllocator& allocator, const Type& type, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) {
    Float flhs;
    if (lhs->getFloat(flhs)) {
      Float frhs;
      if (rhs->getFloat(frhs)) {
        assert(type.hasPrimitiveFlag(ValueFlags::Float));
        return mutateFloat(allocator, flhs, frhs, mutation, out);
      }
      Int irhs;
      if (rhs->getInt(irhs)) {
        assert(type.hasPrimitiveFlag(ValueFlags::Float));
        // Allow inaccurate promotion
        return mutateFloat(allocator, flhs, Float(irhs), mutation, out);
      }
      return Type::Assignment::Incompatible;
    }
    Int ilhs;
    if (lhs->getInt(ilhs)) {
      Int irhs;
      if (rhs->getInt(irhs)) {
        assert(type.hasPrimitiveFlag(ValueFlags::Int));
        return mutateInt(allocator, ilhs, irhs, mutation, out);
      }
      Float frhs;
      if (rhs->getFloat(frhs)) {
        if (!type.hasPrimitiveFlag(ValueFlags::Float)) {
          return Type::Assignment::BadIntToFloat;
        }
        // Allow inaccurate promotion
        return mutateFloat(allocator, Float(ilhs), frhs, mutation, out);
      }
    }
    return Type::Assignment::Incompatible;
  }

  Type::Assignment mutateBitwise(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) {
    Int ilhs;
    if (lhs->getInt(ilhs)) {
      Int irhs;
      if (rhs->getInt(irhs)) {
        return mutateInt(allocator, ilhs, irhs, mutation, out);
      }
    }
    Bool blhs;
    if (lhs->getBool(blhs)) {
      Bool brhs;
      if (rhs->getBool(brhs)) {
        return mutateBool(allocator, blhs, brhs, mutation, out);
      }
    }
    return Type::Assignment::Incompatible;
  }

  Type::Assignment mutateLogical(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) {
    Bool blhs;
    if (lhs->getBool(blhs)) {
      Bool brhs;
      if (rhs->getBool(brhs)) {
        return mutateBool(allocator, blhs, brhs, mutation, out);
      }
    }
    return Type::Assignment::Incompatible;
  }

  Type::Assignment mutateShift(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) {
    Int ilhs;
    if (lhs->getInt(ilhs)) {
      Int irhs;
      if (rhs->getInt(irhs)) {
        return mutateInt(allocator, ilhs, irhs, mutation, out);
      }
    }
    return Type::Assignment::Incompatible;
  }

  Type::Assignment mutateIfNull(IAllocator&, const Value& lhs, const Value& rhs, Value& out) {
    out = lhs->getNull() ? rhs : lhs;
    return Type::Assignment::Success;
  }
}

Type::Assignment Type::mutate(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) const {
  assert(lhs.validate());
  assert(rhs.validate());
  switch (mutation) {
  case Mutation::Assign:
    return this->promote(allocator, rhs, out);
  case Mutation::Noop:
    out = lhs;
    return Assignment::Success;
  case Mutation::Decrement:
    assert(rhs->getVoid());
    return mutateDelta(allocator, lhs, -1, out);
  case Mutation::Increment:
    assert(rhs->getVoid());
    return mutateDelta(allocator, lhs, +1, out);
  case Mutation::Add:
  case Mutation::Subtract:
  case Mutation::Multiply:
  case Mutation::Divide:
  case Mutation::Remainder:
    return mutateArithmetic(allocator, *this, lhs, rhs, mutation, out);
  case Mutation::BitwiseAnd:
  case Mutation::BitwiseOr:
  case Mutation::BitwiseXor:
    return mutateBitwise(allocator, lhs, rhs, mutation, out);
  case Mutation::LogicalAnd:
  case Mutation::LogicalOr:
    return mutateLogical(allocator, lhs, rhs, mutation, out);
  case Mutation::ShiftLeft:
  case Mutation::ShiftRight:
  case Mutation::ShiftRightUnsigned:
    return mutateShift(allocator, lhs, rhs, mutation, out);
  case Mutation::IfNull:
    return mutateIfNull(allocator, lhs, rhs, out);
  }
  throw std::logic_error("Unsupported value mutation");
}
