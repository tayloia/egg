#include <map>
#include <set>

namespace egg::ovum {
  class Forge;
  class INode;

#define EGG_OVUM_VALUE_FLAGS(X) \
  X(Void, "void") \
  X(Null, "null") \
  X(Bool, "bool") \
  X(Int, "int") \
  X(Float, "float") \
  X(String, "string") \
  X(Object, "object") \
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

  class Type : public HardPtr<const IType> { // WIBBLE
  public:
    Type(std::nullptr_t = nullptr) : HardPtr() { // implicit
    }
    explicit Type(const IType* rhs) : HardPtr(rhs) {
    }
    template<typename T>
    explicit Type(const SoftPtr<T>& rhs) : HardPtr(rhs.get()) {
    }

    // Constants
    static const Type None;
    static const Type Void;
    static const Type Null;
    static const Type Bool;
    static const Type BoolInt;
    static const Type Int;
    static const Type IntQ;
    static const Type Float;
    static const Type String;
    static const Type Arithmetic;
    static const Type Object;
    static const Type Any;
    static const Type AnyQ;
  };
}
