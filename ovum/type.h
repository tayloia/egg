namespace egg::ovum {
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
    explicit Type(const IType* rhs = nullptr) : HardPtr(rhs) {
    }
    static const Type Void;
    static const Type Null;
    static const Type Bool;
    static const Type Int;
    static const Type Float;
    static const Type String;
    static const Type Arithmetic;
    static const Type Any;
    static const Type AnyQ;

    static const IType* getBasalType(BasalBits basal);
    static std::string getBasalString(BasalBits basal);
    static Type makeBasal(IAllocator& allocator, BasalBits basal);
    static Type makeUnion(IAllocator& allocator, const IType& lhs, const IType& rhs);
    static Type makePointer(IAllocator& allocator, const IType& pointee);
  };

  class Object : public HardPtr<IObject> {
  public:
    explicit Object(const IObject& rhs) : HardPtr(&rhs) {
      assert(this->get() != nullptr);
    }
  };
}
