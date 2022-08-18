#include <map>

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
    template<typename T>
    explicit Type(const SoftPtr<T>& rhs) : HardPtr(rhs.get()) {
    }
    // We need to qualify the return type because 'String' is a member further down!
    egg::ovum::String describeValue() const;
    egg::ovum::String toString() const;
    static std::string toString(const IType& type) {
      return type.toStringPrecedence().first;
    }

    // Helpers
    bool isComplex() const {
      return (*this)->getObjectShapeCount() > 0;
    }
    bool hasPrimitiveFlag(ValueFlags flag) const {
      return Bits::hasAnySet((*this)->getPrimitiveFlags(), flag);
    }
    enum class Assignability { Never, Sometimes, Always };
    Assignability queryAssignable(const IType& from) const;
    const IFunctionSignature* queryCallable() const;
    const IPropertySignature* queryDotable() const;
    const IIndexSignature* queryIndexable() const;
    const IIteratorSignature* queryIterable() const;
    const IPointerSignature* queryPointable() const;

    enum class Assignment {
      Success, Uninitialized, Incompatible, BadIntToFloat
    };
    Assignment promote(IAllocator& allocator, const Value& rhs, Value& out) const;
    Assignment mutate(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) const;

    // Constants
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

  class TypeBuilderParameter : public IFunctionSignatureParameter {
  private:
    Type type;
    String name;
    size_t position; // SIZE_MAX for named
    Flags flags;
  public:
    TypeBuilderParameter(const Type& type, const String& name, Flags flags, size_t position)
      : type(type),
        name(name),
        position(position),
        flags(flags) {
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual Type getType() const override {
      return this->type;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
  };

  class TypeBuilderCallable : public IFunctionSignature {
    TypeBuilderCallable(const TypeBuilderCallable&) = delete;
    TypeBuilderCallable& operator=(const TypeBuilderCallable&) = delete;
  private:
    Type rettype;
    String name;
    std::vector<TypeBuilderParameter> positional;
    std::vector<TypeBuilderParameter> named;
  public:
    TypeBuilderCallable(const Type& rettype, const String& name)
      : rettype(rettype),
        name(name) {
      assert(rettype != nullptr);
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->rettype;
    }
    virtual size_t getParameterCount() const override {
      return this->positional.size() + this->named.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      if (index < this->positional.size()) {
        return this->positional[index];
      }
      return this->named.at(index - this->positional.size());
    }
    void addPositionalParameter(const Type& ptype, const String& pname, IFunctionSignatureParameter::Flags pflags) {
      auto pindex = this->positional.size();
      this->positional.emplace_back(ptype, pname, pflags, pindex);
    }
    void addNamedParameter(const Type& ptype, const String& pname, IFunctionSignatureParameter::Flags pflags) {
      this->named.emplace_back(ptype, pname, pflags, SIZE_MAX);
    }
  };

  class TypeBuilderProperties : public IPropertySignature {
    TypeBuilderProperties(const TypeBuilderProperties&) = delete;
    TypeBuilderProperties& operator=(const TypeBuilderProperties&) = delete;
  private:
    struct Property {
      Type type;
      String name;
      Modifiability modifiability;
    };
    // TODO use Dictionary?
    std::map<String, Property> map;
    std::vector<String> vec;
    Type unknownType; // the type of unknown properties or null
    Modifiability unknownModifiability; // the modifiability of unknown properties
  public:
    TypeBuilderProperties()
      : unknownType(nullptr),
        unknownModifiability(Modifiability::None) {
      // Closed properties (i.e. unknown properties are not allowed)
    }
    TypeBuilderProperties(const Type& unknownType, Modifiability unknownModifiability)
      : unknownType(unknownType),
        unknownModifiability(unknownModifiability) {
      // Open set of properties
    }
    virtual Type getType(const String& property) const override {
      auto found = this->map.find(property);
      if (found == this->map.end()) {
        return Type(this->unknownType);
      }
      return Type(found->second.type);
    }
    virtual Modifiability getModifiability(const String& property) const override {
      auto found = this->map.find(property);
      if (found == this->map.end()) {
        return this->unknownModifiability;
      }
      return found->second.modifiability;
    }
    virtual String getName(size_t index) const override {
      return this->vec.at(index);
    }
    virtual size_t getNameCount() const override {
      return this->vec.size();
    }
    virtual bool isClosed() const override {
      return this->unknownModifiability == Modifiability::None;
    }
    bool add(const Type& type, const String& name, Modifiability modifiability) {
      // Careful with reference counting under exception conditions!
      Property property{ type, name, modifiability };
      auto added = this->map.try_emplace(name, std::move(property));
      if (!added.second) {
        return false;
      }
      this->vec.emplace_back(name);
      return true;
    }
  };

  class TypeBuilderIndexable : public IIndexSignature {
    TypeBuilderIndexable(const TypeBuilderIndexable&) = delete;
    TypeBuilderIndexable& operator=(const TypeBuilderIndexable&) = delete;
  private:
    Type resultType;
    Type indexType;
    Modifiability modifiability;
  public:
    TypeBuilderIndexable(const Type& resultType, const Type& indexType, Modifiability modifiability)
      : resultType(resultType),
        indexType(indexType),
        modifiability(modifiability) {
      assert(resultType != nullptr);
      assert(indexType != nullptr);
      assert(modifiability != Modifiability::None);
    }
    virtual Type getResultType() const override {
      return this->resultType;
    }
    virtual Type getIndexType() const override {
      return this->indexType;
    }
    virtual Modifiability getModifiability() const override {
      return this->modifiability;
    }
  };

  class TypeBuilderIterable : public IIteratorSignature {
    TypeBuilderIterable(const TypeBuilderIterable&) = delete;
    TypeBuilderIterable& operator=(const TypeBuilderIterable&) = delete;
  private:
    Type resultType;
  public:
    explicit TypeBuilderIterable(const Type& resultType)
      : resultType(resultType) {
      assert(resultType != nullptr);
    }
    virtual Type getType() const override {
      return this->resultType;
    }
  };

  class TypeFactory : public ITypeFactory {
    TypeFactory(const TypeFactory&) = delete;
    TypeFactory& operator=(const TypeFactory&) = delete;
  private:
    class Shape {

    };
    class Complex {
      Type simple;
      std::vector<size_t> shapes;
    };
    IAllocator& allocator;
    ReadWriteMutex mutex;
    std::map<ValueFlags, Type> simple;
    std::vector<Complex> complex;
    std::vector<Shape> shape;
    std::map<const IType*, Type> pointer;
  public:
    explicit TypeFactory(IAllocator& allocator);
    virtual IAllocator& getAllocator() const override { return this->allocator; }

    virtual Type createSimple(ValueFlags flags) override;
    virtual Type createPointer(const Type& pointee) override;
    virtual Type createUnion(const Type& a, const Type& b) override;

    virtual Type addVoid(const Type& type) override;
    virtual Type addNull(const Type& type) override;
    virtual Type removeVoid(const Type& type) override;
    virtual Type removeNull(const Type& type) override;

    virtual TypeBuilder createTypeBuilder(const String& name, const String& description) override;
    virtual TypeBuilder createFunctionBuilder(const Type& rettype, const String& name, const String& description) override;
    virtual TypeBuilder createGeneratorBuilder(const Type& gentype, const String& name, const String& description) override;
  private:
    Type addPrimitiveFlag(const Type& type, ValueFlags flag);
    Type removePrimitiveFlag(const Type& type, ValueFlags flag);
    void registerSimpleBasic(const IType& type);
  };
}
