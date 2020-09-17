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
  };

  class TypeFactory {
  public:
    static Type createSimple(IAllocator& allocator, ValueFlags flags);
    static Type createPointer(IAllocator& allocator, const IType& pointee);
    static Type createUnion(IAllocator& allocator, const IType& a, const IType& b);
    static ITypeFunction* createFunction(IAllocator& allocator, const egg::ovum::String& name, const Type& rettype);
    static ITypeFunction* createGenerator(IAllocator& allocator, const egg::ovum::String& name, const Type& rettype);
  };
}
