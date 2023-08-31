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

  struct TypeShape {
  private: // WIBBLE
    friend class egg::ovum::IAllocator;
    TypeShape(const IFunctionSignature* callable, const IPropertySignature* dotable, const IIndexSignature* indexable, const IIteratorSignature* iterable, const IPointerSignature* pointable)
      : callable(callable),
        dotable(dotable),
        indexable(indexable),
        iterable(iterable),
        pointable(pointable) {
    }
  public:
    const IFunctionSignature* callable;
    const IPropertySignature* dotable;
    const IIndexSignature* indexable;
    const IIteratorSignature* iterable;
    const IPointerSignature* pointable;
    bool equals(const TypeShape& rhs) const;
    void print(Printer& printer) const;
  };

  class Type : public HardPtr<const IType> {
  public:
    Type(std::nullptr_t = nullptr) : HardPtr() { // implicit
    }
    explicit Type(const IType* rhs) : HardPtr(rhs) {
    }
    template<typename T>
    explicit Type(const SoftPtr<T>& rhs) : HardPtr(rhs.get()) {
    }
    // We need to qualify the return type because 'String' is a member further down!
    egg::ovum::String describeValue() const;
    egg::ovum::String toString() const;
    static std::string toString(const IType& type) {
      return type.toStringPrecedence().first;
    }

    // Equivalence
    bool isEquivalent(const Type& rhs) const {
      return Type::areEquivalent(this->get(), rhs.get());
    }
    template<typename T>
    static bool areEquivalent(const T* lhs, const T* rhs) {
      if (lhs == rhs) {
        return true;
      }
      if ((lhs == nullptr) || (rhs == nullptr)) {
        return false;
      }
      return Type::areEquivalent(*lhs, *rhs);
    }
    static bool areEquivalent(const Type& lhs, const Type& rhs) {
      return Type::areEquivalent(lhs.get(), rhs.get());
    }
    static bool areEquivalent(const IType& lhs, const IType& rhs);
    static bool areEquivalent(const IFunctionSignatureParameter& lhs, const IFunctionSignatureParameter& rhs);
    static bool areEquivalent(const IFunctionSignature& lhs, const IFunctionSignature& rhs);
    static bool areEquivalent(const IIndexSignature& lhs, const IIndexSignature& rhs);
    static bool areEquivalent(const IIteratorSignature& lhs, const IIteratorSignature& rhs);
    static bool areEquivalent(const IPropertySignature& lhs, const IPropertySignature& rhs);
    static bool areEquivalent(const IPointerSignature& lhs, const IPointerSignature& rhs);
    static bool areEquivalent(const TypeShape& lhs, const TypeShape& rhs);

    // Helpers
    bool isComplex() const {
      return (*this)->getObjectShapeCount() > 0;
    }
    bool hasPrimitiveFlag(ValueFlags flag) const {
      return Bits::hasAnySet((*this)->getPrimitiveFlags(), flag);
    }
    enum class Assignability { Never, Sometimes, Always };
    Assignability queryAssignable(const IType& from) const;
    enum class Assignment {
      Success, Uninitialized, Incompatible, BadIntToFloat
    };
    Assignment promote(IAllocator& allocator, const Value& rhs, Value& out) const;
    Assignment mutate(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) const;

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

  class TypeFactory : public ITypeFactory {
    TypeFactory(const TypeFactory&) = delete;
    TypeFactory& operator=(const TypeFactory&) = delete;
  private:
    std::unique_ptr<Forge> forge;
    std::map<const IType*, Type> pointers;
    StringProperties* stringProperties; // WIBBLE remove
  public:
    explicit TypeFactory(IAllocator& allocator);
    ~TypeFactory();
    virtual IAllocator& getAllocator() const override;

    virtual Type createSimple(ValueFlags flags) override;
    virtual Type createPointer(const Type& pointee, Modifiability modifiability) override;
    virtual Type createArray(const Type& result, Modifiability modifiability) override;
    virtual Type createMap(const Type& result, const Type& index, Modifiability modifiability) override;
    virtual Type createUnion(const std::vector<Type>& types) override;
    virtual Type createIterator(const Type& element) override;

    virtual Type addVoid(const Type& type) override;
    virtual Type addNull(const Type& type) override;
    virtual Type removeVoid(const Type& type) override;
    virtual Type removeNull(const Type& type) override;

    virtual TypeBuilder createTypeBuilder(const String& name, const String& description = {}) override;
    virtual TypeBuilder createFunctionBuilder(const Type& rettype, const String& name, const String& description = {}) override;
    virtual TypeBuilder createGeneratorBuilder(const Type& gentype, const String& name, const String& description = {}) override;

    virtual const TypeShape& getObjectShape() override;
    virtual const TypeShape& getStringShape() override;

    virtual Type getVanillaArray() override;
    virtual Type getVanillaDictionary() override;
    virtual Type getVanillaError() override;
    virtual Type getVanillaKeyValue() override;
    virtual Type getVanillaPredicate() override;
  };
}
