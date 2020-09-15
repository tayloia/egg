namespace egg::ovum {
  class INode;

#define EGG_OVUM_VALUE_FLAGS(X) \
  EGG_VM_BASAL(X) \
  X(Memory, "memory") \
  X(Pointer, "pointer") \
  X(Break, "break") \
  X(Continue, "continue") \
  X(Return, "return") \
  X(Yield, "yield") \
  X(Throw, "throw")

  enum class ValueFlagsShift {
    _ = -1 // We want the next element to start at zero
#define EGG_OVUM_VALUE_FLAGS_SHIFT(name, text) , name
    EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_SHIFT)
#undef EGG_OVUM_VALUE_FLAGS_SHIFT
  };

  enum class ValueFlags {
    None = 0,
#define EGG_OVUM_VALUE_FLAGS_ENUM(name, text) name = 1 << int(ValueFlagsShift::name),
    EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_ENUM)
#undef EGG_OVUM_VALUE_FLAGS_ENUM
    Arithmetic = Int | Float,
    Any = Bool | Int | Float | String | Object | Pointer,
    AnyQ = Null | Any,
    FlowControl = Break | Continue | Return | Yield | Throw
  };
  inline constexpr ValueFlags operator|(ValueFlags lhs, ValueFlags rhs) {
    return Bits::set(lhs, rhs);
  }

  // None, Void, Null, Bool, Int, Float, String, Object (plus Arithmetic, Any, AnyQ)
  enum class BasalBits {
    None = 0,
#define EGG_VM_BASAL_ENUM(name, text) name = 1 << int(ValueFlagsShift::name),
    EGG_VM_BASAL(EGG_VM_BASAL_ENUM)
#undef EGG_VM_BASAL_ENUM
    Any = Bool | Int | Float | String | Object,
    AnyQ = Any | Null
  };
  inline constexpr BasalBits operator|(BasalBits lhs, BasalBits rhs) {
    return Bits::set(lhs, rhs);
  }

  class Type : public HardPtr<const IType> {
  public:
    Type(std::nullptr_t = nullptr) : HardPtr() {
      // Implicit
    }
    explicit Type(const IType* rhs) : HardPtr(rhs) {
    }
    // We need to qualify the return type because 'String' is a member further down!
    egg::ovum::String toString(int precedence = -1) const {
      auto* type = this->get();
      if (type == nullptr) {
        return "<unknown>";
      }
      auto pair = type->toStringPrecedence();
      if (pair.second < precedence) {
        return "(" + pair.first + ")";
      }
      return pair.first;
    }
    // Constants
    static const Type Void;
    static const Type Null;
    static const Type Bool;
    static const Type Int;
    static const Type Float;
    static const Type String;
    static const Type Object;
    static const Type Memory;
    static const Type Arithmetic;
    static const Type Any;
    static const Type AnyQ;
    // Helpers
    static const IType* getBasalType(BasalBits basal);
    static std::string getBasalString(BasalBits basal);
    static Type makeBasal(IAllocator& allocator, BasalBits basal);
    static Type makeUnion(IAllocator& allocator, const IType& lhs, const IType& rhs);
    static Type makePointer(IAllocator& allocator, const IType& pointee);
    static Type makePointer(const IType& pointee); // TODO always supply an allocator
  };

  class TypeBase : public IType {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  public:
    TypeBase() = default;
    virtual bool hasBasalType(BasalBits basal) const override;

    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override;
    virtual const IFunctionSignature* callable() const override;
    virtual const IIndexSignature* indexable() const override;
    virtual Type dotable(const String& property, String& error) const override;
    virtual Type iterable() const override;
    virtual Type pointeeType() const override;
    virtual Type devoidedType() const override;
    virtual Type denulledType() const override;
    virtual Node compile(IAllocator& allocator, const NodeLocation& location) const override;

    virtual BasalBits getBasalTypesLegacy() const override;
  };

  class Object : public HardPtr<IObject> {
  public:
    Object() : HardPtr(nullptr) {
      assert(this->get() == nullptr);
    }
    explicit Object(const IObject& rhs) : HardPtr(&rhs) {
      assert(this->get() != nullptr);
    }
    bool validate() const {
      auto* underlying = this->get();
      return (underlying != nullptr) && underlying->validate();
    }
  };
}
