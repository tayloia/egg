namespace egg::ovum {
  class INode;

#define EGG_OVUM_BASAL(X) \
  EGG_VM_TYPES(X)
#define EGG_OVUM_VARIANT(X) \
  EGG_VM_TYPES(X) \
  X(Memory, "memory") \
  X(Pointer, "pointer") \
  X(Indirect, "indirect") \
  X(Break, "break") \
  X(Continue, "continue") \
  X(Return, "return") \
  X(Yield, "yield") \
  X(Throw, "throw") \
  X(Hard, "hard")

  namespace impl {
    enum {
      _ = -1 // We want the next element to start at zero
#define EGG_OVUM_VARIANT_ENUM(name, text) , name
      EGG_OVUM_VARIANT(EGG_OVUM_VARIANT_ENUM)
#undef EGG_OVUM_VARIANT_ENUM
    };
  }

  // None, Void, Null, Bool, Int, Float, String, Object (plus Arithmetic, Any, AnyQ)
  enum class BasalBits {
    None = 0,
#define EGG_OVUM_BASAL_ENUM(name, text) name = 1 << impl::name,
    EGG_OVUM_BASAL(EGG_OVUM_BASAL_ENUM)
#undef EGG_OVUM_BASAL_ENUM
    Arithmetic = Int | Float,
    Any = Bool | Int | Float | String | Object,
    AnyQ = Any | Null
  };
  inline BasalBits operator|(BasalBits lhs, BasalBits rhs) {
    return Bits::set(lhs, rhs);
  }

  class Type : public HardPtr<const IType> {
  public:
    Type(nullptr_t = nullptr) : HardPtr() {
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
    static const Type Arithmetic;
    static const Type Any;
    static const Type AnyQ;
    // Helpers
    static const IType* getBasalType(BasalBits basal);
    static std::string getBasalString(BasalBits basal);
    static Type makeBasal(IAllocator& allocator, BasalBits basal);
    static Type makeUnion(IAllocator& allocator, const IType& lhs, const IType& rhs);
    static Type makePointer(IAllocator& allocator, const IType& pointee);
  };

  class TypeBase : public IType {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  public:
    TypeBase() = default;
    virtual BasalBits getBasalTypes() const override;
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rhs) const override;
    virtual const IFunctionSignature* callable() const override;
    virtual const IIndexSignature* indexable() const override;
    virtual Type dotable(const String* property, String& error) const override;
    virtual Type iterable() const override;
    virtual Type pointeeType() const override;
    virtual Type denulledType() const override;
    virtual Type unionWithBasal(IAllocator& allocator, BasalBits other) const override;
    virtual Variant tryAssign(IExecution& execution, Variant& lvalue, const egg::ovum::Variant& rvalue) const override;
    virtual Node compile(IAllocator& allocator, const NodeLocation& location) const override;
  };

  class Object : public HardPtr<IObject> {
  public:
    explicit Object(const IObject& rhs) : HardPtr(&rhs) {
      assert(this->get() != nullptr);
    }
  };
}
