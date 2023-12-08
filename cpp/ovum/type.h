#include <map>
#include <set>

namespace egg::ovum {
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
  X(Throw, "throw") \
  X(Type, "type") \

  enum class ValueFlagsShift {
    LBound = -1, // We want the next element to start at zero
#define EGG_OVUM_VALUE_FLAGS_SHIFT(name, text) name,
    EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_SHIFT)
#undef EGG_OVUM_VALUE_FLAGS_SHIFT
    UBound
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

  class Type {
  private:
    const IType* ptr;
  public:
    Type(const IType* rhs = nullptr) : ptr(rhs) { // implicit
    }
    operator const IType*() const {
      return this->ptr;
    }
    bool operator==(const IType* rhs) const {
      return this->ptr == rhs;
    }
    bool operator!=(const IType* rhs) const {
      return this->ptr != rhs;
    }
    const IType& operator*() const {
      auto* p = this->ptr;
      assert(p != nullptr);
      return *p;
    }
    const IType* operator->() const {
      auto* p = this->ptr;
      assert(p != nullptr);
      return p;
    }
    Type& operator=(const IType* rhs) {
      this->ptr = rhs;
      return *this;
    }
    const IType* get() const {
      return this->ptr;
    }
    bool validate() const {
      auto* p = this->ptr;
      return (p != nullptr) && p->validate();
    }
    size_t hash() const {
      return Hash::hash(this->ptr);
    }
    const IType::Shape* getOnlyShape() const {
      auto* p = this->ptr;
      assert((p != nullptr) && p->validate());
      return (p->getShapeCount() == 1) ? p->getShape(0) : nullptr;
    }
    const IFunctionSignature* getOnlyFunctionSignature() const {
      auto* shape = this->getOnlyShape();
      return (shape != nullptr) ? shape->callable : nullptr;
    }
    void print(Printer& printer) const;

    // Constants
    static const Type None;
    static const Type Void;
    static const Type Null;
    static const Type Bool;
    static const Type Int;
    static const Type Float;
    static const Type String;
    static const Type Arithmetic;
    static const Type Object;
    static const Type Any;
    static const Type AnyQ;

    // Helpers
    static void print(Printer& printer, const IType& type);
    static void print(Printer& printer, const IFunctionSignature& signature);
  };

  class TypeForgeFactory {
  public:
    static HardPtr<ITypeForge> createTypeForge(IAllocator& allocator, IBasket& basket);
  };
}
