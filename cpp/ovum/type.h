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

  class TypeShape {
  private:
    const IType::Shape* ptr;
  public:
    explicit TypeShape(const IType::Shape& rhs) : ptr(&rhs) {
    }
    bool operator==(const IType::Shape& rhs) const {
      return this->ptr == &rhs;
    }
    bool operator!=(const IType::Shape& rhs) const {
      return this->ptr != &rhs;
    }
    const IType::Shape* operator->() const {
      auto* p = this->ptr;
      assert(p != nullptr);
      return p;
    }
    const IType::Shape& get() const {
      auto* p = this->ptr;
      assert(p != nullptr);
      return *p;
    }
    bool validate() const {
      return this->ptr != nullptr;
    }
    size_t hash() const {
      return Hash::hash(this->ptr);
    }
    struct Less {
      // Used by TypeShapeSet
      bool operator()(const TypeShape& lhs, const TypeShape& rhs) const {
        return &lhs.get() < &rhs.get();
      }
    };
  };

  class TypeShapeSet : public std::set<TypeShape, TypeShape::Less> {
    using Container = std::set<TypeShape, TypeShape::Less>;
  public:
    bool add(const TypeShape& shape) {
      return this->emplace(shape).second;
    }
    bool remove(const TypeShape& shape) {
      return this->erase(shape) > 0;
    }
  };

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
    void unionShapeSet(TypeShapeSet& shapeset) const {
      auto* p = this->ptr;
      assert((p != nullptr) && p->validate());
      size_t count = p->getShapeCount();
      for (size_t index = 0; index < count; ++index) {
        auto* s = p->getShape(index);
        assert(s != nullptr);
        shapeset.emplace(*s);
      }
    }
    TypeShapeSet getShapeSet() const {
      TypeShapeSet shapeset;
      this->unionShapeSet(shapeset);
      return shapeset;
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

    // Helpers WIBBLE retire
    static int print(Printer& printer, ValueFlags primitive); // returns precedence
    static int print(Printer& printer, const IType::Shape& shape); // returns precedence
    static void print(Printer& printer, const IType& type);
    static void print(Printer& printer, const IFunctionSignature& signature);
  };

  class TypeForgeFactory {
  public:
    static HardPtr<ITypeForge> createTypeForge(IAllocator& allocator, IBasket& basket);
  };
}
