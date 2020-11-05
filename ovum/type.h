namespace egg::ovum {
  class INode;

#define EGG_OVUM_VALUE_FLAGS(X) \
  X(Void, "void") \
  X(Null, "null") \
  X(Bool, "bool") \
  X(Int, "int") \
  X(Float, "float") \
  X(String, "string") \
  X(Object, "object") \
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
    Any = Bool | Int | Float | String | Object,
    AnyQ = Null | Any,
    FlowControl = Break | Continue | Return | Yield | Throw
  };
  inline constexpr ValueFlags operator|(ValueFlags lhs, ValueFlags rhs) {
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
    egg::ovum::String toString(int precedence = -1) const;
    static std::string toString(const IType& type) {
      return type.toStringPrecedence().first;
    }
    // Helpers
    bool hasAnyFlags(ValueFlags flags) const {
      return Bits::hasAnySet(this->get()->getFlags(), flags);
    }
    // Constants
    static const Type Void;
    static const Type Null;
    static const Type Bool;
    static const Type Int;
    static const Type Float;
    static const Type String;
    static const Type Memory;
    static const Type Arithmetic;
    static const Type Any;
    static const Type AnyQ;
  };

  class ITypeBuilder : public IHardAcquireRelease {
    public:
      virtual void addPositionalParameter(const Type& type, const String& name, IFunctionSignatureParameter::Flags flags) = 0;
      virtual void addNamedParameter(const Type& type, const String& name, IFunctionSignatureParameter::Flags flags) = 0;
      virtual Type build() = 0;
  };
  using TypeBuilder = HardPtr<ITypeBuilder>;

  class TypeFactory {
  public:
    static Type createSimple(IAllocator& allocator, ValueFlags flags);
    static Type createPointer(IAllocator& allocator, const IType& pointee);
    static Type createUnion(IAllocator& allocator, const IType& a, const IType& b);
    static Type createUnionJoin(IAllocator& allocator, const IType& a, const IType& b);
    static TypeBuilder createFunctionBuilder(IAllocator& allocator, const Type& rettype, const String& name);
    static TypeBuilder createGeneratorBuilder(IAllocator& allocator, const Type& rettype, const String& name);

    static const IParameters& ParametersNone;
    static const IFunctionSignature& OmniFunctionSignature; // any?(...any?)
  };
}
