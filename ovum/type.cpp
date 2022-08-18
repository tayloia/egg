#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/node.h"
#include "ovum/builtin.h"
#include "ovum/function.h"

#include <array>
#include <map>
#include <unordered_set>

namespace {
  using namespace egg::ovum;

  const char* primitiveComponent(ValueFlags flags) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (flags) {
      case ValueFlags::None:
        return "var";
#define EGG_OVUM_VALUE_FLAGS_COMPONENT(name, text) case ValueFlags::name: return text;
        EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_COMPONENT)
#undef EGG_OVUM_VALUE_FLAGS_COMPONENT
      case ValueFlags::Any:
        return "any";
      }
    EGG_WARNING_SUPPRESS_SWITCH_END
      return nullptr;
  }

  std::pair<std::string, int> primitiveToStringPrecedence(ValueFlags flags) {
    auto* component = primitiveComponent(flags);
    if (component != nullptr) {
      return { component, 0 };
    }
    if (Bits::hasAnySet(flags, ValueFlags::Null)) {
      return { primitiveToStringPrecedence(Bits::clear(flags, ValueFlags::Null)).first + "?", 1 };
    }
    auto head = Bits::topmost(flags);
    assert(head != ValueFlags::None);
    component = primitiveComponent(head);
    assert(component != nullptr);
    return { primitiveToStringPrecedence(Bits::clear(flags, head)).first + '|' + component, 2 };
  }

  void complexComponentAdd(std::pair<std::string, int>& out, const std::pair<std::string, int>& in) {
    assert(!in.first.empty());
    assert((in.second >= 0) && (in.second <= 2));
    if (out.first.empty()) {
      out = in;
    } else {
      out.first += '|' + in.first;
      out.second = 2;
    }
  }

  std::pair<std::string, int> complexComponentObject(const ObjectShape* shape) {
    assert(shape != nullptr);
    if (shape->callable != nullptr) {
      return FunctionSignature::toStringPrecedence(*shape->callable);
    }
    return { "<complex>", 0 }; // TODO
  }

  std::pair<std::string, int> complexComponentPointer(const PointerShape* shape) {
    assert(shape != nullptr);
    assert(shape->pointee != nullptr);
    auto pointee = shape->pointee->toStringPrecedence();
    if (pointee.second > 1) {
      return { '(' + pointee.first + ')' + '*', 1 };
    }
    return { pointee.first + '*', 1 };
  }

  std::pair<std::string, int> complexToStringPrecedence(ValueFlags primitive, const IType& complex) {
    auto on = complex.getObjectShapeCount();
    auto pn = complex.getPointerShapeCount();
    if ((on == 0) && (pn == 0)) {
      // Primitive types only
      return primitiveToStringPrecedence(primitive);
    }
    std::pair<std::string, int> result;
    if (primitive != ValueFlags::None) {
      result = primitiveToStringPrecedence(primitive);
    }
    for (size_t oi = 0; oi < on; ++oi) {
      complexComponentAdd(result, complexComponentObject(complex.getObjectShape(oi)));
    }
    for (size_t pi = 0; pi < pn; ++pi) {
      complexComponentAdd(result, complexComponentPointer(complex.getPointerShape(pi)));
    }
    assert(!result.first.empty());
    return result;
  }

  class TypeBase : public NotHardReferenceCounted<IType> {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  public:
    TypeBase() {}
    virtual const ObjectShape* getObjectShape(size_t) const override {
      // By default, we are not shaped like objects
      return nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      // By default, we are not shaped like objects
      return 0;
    }
    virtual const PointerShape* getPointerShape(size_t) const override {
      // By default, we are not a pointer
      return nullptr;
    }
    virtual size_t getPointerShapeCount() const override {
      // By default, we are not a pointer
      return 0;
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  template<enum ValueFlags FLAGS>
  class TypePrimitive final : public TypeBase {
    TypePrimitive(const TypePrimitive&) = delete;
    TypePrimitive& operator=(const TypePrimitive&) = delete;
  public:
    TypePrimitive() {}
    virtual ValueFlags getPrimitiveFlags() const override {
      return FLAGS;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return primitiveToStringPrecedence(FLAGS);
    }
  };

  TypePrimitive<ValueFlags::None> typeNone{};
  TypePrimitive<ValueFlags::Void> typeVoid{};
  TypePrimitive<ValueFlags::Null> typeNull{};
  TypePrimitive<ValueFlags::Bool> typeBool{};
  TypePrimitive<ValueFlags::Bool | ValueFlags::Int> typeBoolInt{};
  TypePrimitive<ValueFlags::Int> typeInt{};
  TypePrimitive<ValueFlags::Null | ValueFlags::Int> typeIntQ{};
  TypePrimitive<ValueFlags::Float> typeFloat{};
  TypePrimitive<ValueFlags::Arithmetic> typeArithmetic{};
  TypePrimitive<ValueFlags::String> typeString{};
  TypePrimitive<ValueFlags::Object> typeObject{};
  TypePrimitive<ValueFlags::Any> typeAny{};
  TypePrimitive<ValueFlags::AnyQ> typeAnyQ{};

  class TypeSimple final : public HardReferenceCounted<IType> {
    TypeSimple(const TypeSimple&) = delete;
    TypeSimple& operator=(const TypeSimple&) = delete;
  private:
    ValueFlags flags;
  public:
    TypeSimple(IAllocator& allocator, ValueFlags flags)
      : HardReferenceCounted(allocator, 0),
        flags(flags) {
      assert(flags != ValueFlags::None);
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return this->flags;
    }
    virtual const ObjectShape* getObjectShape(size_t) const override {
      return nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 0;
    }
    virtual const PointerShape* getPointerShape(size_t) const override {
      return nullptr;
    }
    virtual size_t getPointerShapeCount() const override {
      return 0;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return primitiveToStringPrecedence(this->flags);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  class TypePointer final : public HardReferenceCounted<IType> {
    TypePointer(const TypePointer&) = delete;
    TypePointer& operator=(const TypePointer&) = delete;
  private:
    Type pointee;
    PointerShape shape;
  public:
    TypePointer(IAllocator& allocator, const Type& pointee, Modifiability modifiability)
      : HardReferenceCounted(allocator, 0),
        pointee(pointee),
        shape({ pointee.get(), modifiability }) {
      assert(this->pointee != nullptr);
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return ValueFlags::None;
    }
    virtual const ObjectShape* getObjectShape(size_t) const override {
      return nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 0;
    }
    virtual const PointerShape* getPointerShape(size_t index) const override {
      return (index == 0) ? &this->shape : nullptr;
    }
    virtual size_t getPointerShapeCount() const override {
      return 1;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return complexToStringPrecedence(ValueFlags::None, *this);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Pointer of type '", this->toStringPrecedence().first, "'");
    }
  };

  class TypeStrip final : public HardReferenceCounted<IType> {
    TypeStrip(const TypeStrip&) = delete;
    TypeStrip& operator=(const TypeStrip&) = delete;
  private:
    Type underlying;
    ValueFlags strip;
  public:
    TypeStrip(IAllocator& allocator, const Type& underlying, ValueFlags strip)
      : HardReferenceCounted(allocator, 0),
        underlying(underlying),
        strip(strip) {
      assert(this->underlying != nullptr);
      assert(!Bits::hasAnySet(this->strip, ValueFlags::FlowControl | ValueFlags::Object));
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return Bits::clear(this->underlying->getPrimitiveFlags(), this->strip);
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return this->underlying->getObjectShape(index);
    }
    virtual size_t getObjectShapeCount() const override {
      return this->underlying->getObjectShapeCount();
    }
    virtual const PointerShape* getPointerShape(size_t index) const override {
      return this->underlying->getPointerShape(index);
    }
    virtual size_t getPointerShapeCount() const override {
      return this->underlying->getPointerShapeCount();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return complexToStringPrecedence(this->getPrimitiveFlags(), *this->underlying);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  class TypeUnion final : public HardReferenceCounted<IType> {
    TypeUnion(const TypeUnion&) = delete;
    TypeUnion& operator=(const TypeUnion&) = delete;
  private:
    Type a;
    Type b;
    ValueFlags flags;
    std::vector<const ObjectShape*> objectShapes;
    std::vector<const PointerShape*> pointerShapes;
  public:
    TypeUnion(IAllocator& allocator, const Type& a, const Type& b)
      : HardReferenceCounted(allocator, 0),
      a(a),
      b(b),
      flags(a->getPrimitiveFlags() | b->getPrimitiveFlags()),
      objectShapes(unionObjects({{ a.get(), b.get() }})),
      pointerShapes(unionPointers({{ a.get(), b.get() }})) {
      assert(this->a != nullptr);
      assert(this->b != nullptr);
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return this->flags;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return this->objectShapes.at(index);
    }
    virtual size_t getObjectShapeCount() const override {
      return this->objectShapes.size();
    }
    virtual const PointerShape* getPointerShape(size_t index) const override {
      return this->pointerShapes.at(index);
    }
    virtual size_t getPointerShapeCount() const override {
      return this->pointerShapes.size();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return complexToStringPrecedence(this->flags, *this);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  private:
    static std::vector<const ObjectShape*> unionObjects(const std::array<const IType*, 2>& types) {
      std::unordered_set<const ObjectShape*> unique;
      std::vector<const ObjectShape*> list;
      for (auto type : types) {
        auto count = type->getObjectShapeCount();
        for (size_t index = 0; index < count; ++index) {
          auto* shape = type->getObjectShape(index);
          if ((shape != nullptr) && unique.insert(shape).second) {
            list.push_back(shape);
          }
        }
      }
      return list;
    }
    static std::vector<const PointerShape*> unionPointers(const std::array<const IType*, 2>& types) {
      std::unordered_set<const PointerShape*> unique;
      std::vector<const PointerShape*> list;
      for (auto type : types) {
        auto count = type->getPointerShapeCount();
        for (size_t index = 0; index < count; ++index) {
          auto* shape = type->getPointerShape(index);
          if ((shape != nullptr) && unique.insert(shape).second) {
            list.push_back(shape);
          }
        }
      }
      return list;
    }
  };

  class Builder : public HardReferenceCounted<ITypeBuilder> {
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
  protected:
    class Built;
    friend class Built;
    std::string name;
    String description;
    std::unique_ptr<TypeBuilderIndexable> indexable;
    std::unique_ptr<TypeBuilderIterable> iterable;
    std::unique_ptr<TypeBuilderProperties> properties;
    bool built;
  public:
    Builder(IAllocator& allocator, const String& name, const String& description)
      : HardReferenceCounted(allocator, 0),
        name(name.toUTF8()),
        description(description),
        built(false) {
    }
    virtual void addPositionalParameter(const Type&, const String&, IFunctionSignatureParameter::Flags) override {
      throw std::logic_error("TypeBuilder::addPositionalParameter() called for non-function type");
    }
    virtual void addNamedParameter(const Type&, const String&, IFunctionSignatureParameter::Flags) override {
      throw std::logic_error("TypeBuilder::addNamedParameter() called for non-function type");
    }
    virtual void addProperty(const Type& ptype, const String& pname, Modifiability modifiability) override;
    virtual void defineDotable(const Type& unknownType, Modifiability unknownModifiability) override;
    virtual void defineIndexable(const Type& resultType, const Type& indexType, Modifiability modifiability) override;
    virtual void defineIterable(const Type& resultType) override;
    virtual Type build() override {
      if (this->built) {
        throw std::logic_error("TypeBuilder::build() called more than once");
      }
      this->built = true;
      return this->allocator.makeHard<Built, Type>(*this, nullptr);
    }
  };

  class Builder::Built : public HardReferenceCounted<IType> {
    Built(const Built&) = delete;
    Built& operator=(const Built&) = delete;
  private:
    HardPtr<Builder> builder;
    ObjectShape shape;
  public:
    Built(IAllocator& allocator, Builder& builder, const IFunctionSignature* callable = nullptr)
      : HardReferenceCounted(allocator, 0),
        builder(&builder) {
      this->shape.callable = callable;
      this->shape.dotable = builder.properties.get();
      this->shape.indexable = builder.indexable.get();
      this->shape.iterable = builder.iterable.get();
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return ValueFlags::None;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &this->shape : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual const PointerShape* getPointerShape(size_t) const override {
      return nullptr;
    }
    virtual size_t getPointerShapeCount() const override {
      return 0;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      if (this->shape.callable != nullptr) {
        // Use the type of the function signature
        return FunctionSignature::toStringPrecedence(*this->shape.callable);
      }
      return { this->builder->name, 0 };
    }
    virtual String describeValue() const override {
      return this->builder->description;
    }
  };

  void Builder::addProperty(const Type& ptype, const String& pname, Modifiability modifiability) {
    if (this->properties == nullptr) {
      throw std::logic_error("TypeBuilder::addProperty() called before TypeBuilder::defineDotable()");
    }
    if (!this->properties->add(ptype, pname, modifiability)) {
      throw std::logic_error("TypeBuilder::addProperty() found duplicate property name: " + pname.toUTF8());
    }
  }

  void Builder::defineDotable(const Type& unknownType, Modifiability unknownModifiability) {
    if (this->properties != nullptr) {
      throw std::logic_error("TypeBuilder::defineDotable() called more than once");
    }
    this->properties = std::make_unique<TypeBuilderProperties>(unknownType, unknownModifiability);
  }

  void Builder::defineIndexable(const Type& resultType, const Type& indexType, Modifiability modifiability) {
    if (this->indexable != nullptr) {
      throw std::logic_error("TypeBuilder::defineIndexable() called more than once");
    }
    this->indexable = std::make_unique<TypeBuilderIndexable>(resultType, indexType, modifiability);
  }

  void Builder::defineIterable(const Type& resultType) {
    if (this->iterable != nullptr) {
      throw std::logic_error("TypeBuilder::defineIterable() called more than once");
    }
    this->iterable = std::make_unique<TypeBuilderIterable>(resultType);
  }

  class FunctionBuilder : public Builder {
    FunctionBuilder(const FunctionBuilder&) = delete;
    FunctionBuilder& operator=(const FunctionBuilder&) = delete;
  private:
    TypeBuilderCallable callable;
  public:
    FunctionBuilder(IAllocator& allocator, const Type& rettype, const String& name, const String& description)
      : Builder(allocator, name, description),
        callable(rettype, name) {
      assert(rettype != nullptr);
    }
    virtual void addPositionalParameter(const Type& ptype, const String& pname, IFunctionSignatureParameter::Flags pflags) {
      if (this->built) {
        throw std::logic_error("FunctionBuilder::addPositionalParameter() called after type is built");
      }
      this->callable.addPositionalParameter(ptype, pname, pflags);
    }
    virtual void addNamedParameter(const Type& ptype, const String& pname, IFunctionSignatureParameter::Flags pflags) {
      if (this->built) {
        throw std::logic_error("FunctionBuilder::addNamedParameter() called after type is built");
      }
      this->callable.addNamedParameter(ptype, pname, pflags);
    }
    virtual Type build() override {
      if (this->built) {
        throw std::logic_error("FunctionBuilder::build() called more than once");
      }
      this->built = true;
      return this->allocator.makeHard<Built, Type>(*this, &this->callable);
    }
  };

  template<typename T, typename F>
  const T* queryObjectShape(const IType* type, F field) {
    assert(type != nullptr);
    auto count = type->getObjectShapeCount();
    assert(count <= 1);
    if (count > 0) {
      auto* shape = type->getObjectShape(0);
      if (shape != nullptr) {
        auto* result = shape->*field;
        if (result != nullptr) {
          return result;
        }
      }
    }
    auto flags = type->getPrimitiveFlags();
    if (Bits::hasAnySet(flags, ValueFlags::Object)) {
      auto* result = BuiltinFactory::getObjectShape().*field;
      if (result != nullptr) {
        return result;
      }
    }
    if (Bits::hasAnySet(flags, ValueFlags::String)) {
      return BuiltinFactory::getStringShape().*field;
    }
    return nullptr;
  }

  Type::Assignability queryAssignableObject(const IType& ltype, const IType& rtype) {
    // WOBBLE shapes
    (void)ltype;
    (void)rtype;
    return Type::Assignability::Always;
  }

  Type::Assignability queryAssignablePointer(const IType& ltype, const IType& rtype) {
    // WOBBLE shapes
    (void)ltype;
    (void)rtype;
    return Type::Assignability::Always;
  }

  Type::Assignability queryAssignablePrimitive(const IType& ltype, const IType& rtype, ValueFlags lflags, ValueFlags rflag) {
    assert(Bits::hasOneSet(rflag, ValueFlags::Void | ValueFlags::AnyQ));
    auto result = Type::Assignability::Never;
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (rflag) {
    case ValueFlags::Void:
      break;
    case ValueFlags::Null:
    case ValueFlags::Bool:
    case ValueFlags::Float:
    case ValueFlags::String:
      if (Bits::hasAnySet(lflags, rflag)) {
        result = Type::Assignability::Always;
      }
      break;
    case ValueFlags::Int:
      if (Bits::hasAnySet(lflags, ValueFlags::Int)) {
        result = Type::Assignability::Always;
      } else if (Bits::hasAnySet(lflags, ValueFlags::Float)) {
        // Permit type promotion int->float
        result = Type::Assignability::Sometimes;
      }
      break;
    case ValueFlags::Object:
      if (Bits::hasAnySet(lflags, ValueFlags::Object)) {
        result = queryAssignableObject(ltype, rtype);
      }
      break;
    default:
      assert(0); // not a type within 'any?'
      break;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    return result;
  }

  Type::Assignability queryAssignablePrimitives(const IType& ltype, const IType& rtype, ValueFlags lflags, ValueFlags rflags) {
    assert(Bits::hasAnySet(rflags, ValueFlags::Void | ValueFlags::AnyQ));
    if (Bits::hasOneSet(rflags, ValueFlags::Void | ValueFlags::AnyQ)) {
      return queryAssignablePrimitive(ltype, rtype, lflags, rflags);
    }
    auto rflag = Bits::topmost(rflags);
    assert(rflag != ValueFlags::None);
    auto assignability = queryAssignablePrimitive(ltype, rtype, lflags, rflag);
    if (assignability == Type::Assignability::Sometimes) {
      return Type::Assignability::Sometimes;
    }
    rflags = Bits::clear(rflags, rflag);
    rflag = Bits::topmost(rflags);
    while (rflag != ValueFlags::None) {
      if (queryAssignablePrimitive(ltype, rtype, lflags, rflag) != assignability) {
        return Type::Assignability::Sometimes;
      }
      rflags = Bits::clear(rflags, rflag);
      rflag = Bits::topmost(rflags);
    }
    return assignability;
  }

  Type::Assignability queryAssignableType(const IType& ltype, const IType& rtype) {
    // TODO complex
    // Composability is like this:
    //  ALWAYS + ALWAYS = ALWAYS
    //  ALWAYS + SOMETIMES = SOMETIMES
    //  ALWAYS + NEVER = SOMETIMES
    //  SOMETIMES + ALWAYS = SOMETIMES
    //  SOMETIMES + SOMETIMES = SOMETIMES
    //  SOMETIMES + NEVER = SOMETIMES
    //  NEVER + ALWAYS = SOMETIMES
    //  NEVER + SOMETIMES = SOMETIMES
    //  NEVER + NEVER = NEVER
    // which is:
    //  (a == b) ? a : SOMETIMES
    auto lflags = ltype.getPrimitiveFlags();
    auto rflags = rtype.getPrimitiveFlags();
    if (Bits::hasAnySet(rflags, ValueFlags::Void | ValueFlags::AnyQ)) {
      // We have at least one primitive type and possibly object/pointer shapes
      auto assignability = queryAssignablePrimitives(ltype, rtype, lflags, rflags);
      if (assignability == Type::Assignability::Sometimes) {
        return Type::Assignability::Sometimes;
      }
      if ((rtype.getObjectShapeCount() > 0) && (queryAssignableObject(ltype, rtype) != assignability)) {
        return Type::Assignability::Sometimes;
      }
      if ((rtype.getPointerShapeCount() > 0) && (queryAssignablePointer(ltype, rtype) != assignability)) {
        return Type::Assignability::Sometimes;
      }
      return assignability;
    }
    if (rtype.getObjectShapeCount() > 0) {
      // We have no primitive types, but at least one object shape and possibly pointer shapes
      auto assignability = queryAssignableObject(ltype, rtype);
      if (assignability == Type::Assignability::Sometimes) {
        return Type::Assignability::Sometimes;
      }
      if ((rtype.getPointerShapeCount() > 0) && (queryAssignablePointer(ltype, rtype) != assignability)) {
        return Type::Assignability::Sometimes;
      }
      return assignability;
    }
    if (rtype.getPointerShapeCount() > 0) {
      // We have no primitive types nor object shapes, but at least one pointer shape
      return queryAssignablePointer(ltype, rtype);
    }
    throw std::logic_error("Type has no known shape");
  }

  bool isAssignableInstanceObjectShapeCallable(const IFunctionSignature* lcallable, const IFunctionSignature* rcallable) {
    if (lcallable == rcallable) {
      return true;
    }
    if ((lcallable == nullptr) || (rcallable == nullptr)) {
      return false;
    }
    return true; // TODO
  }

  bool isAssignableInstanceObjectShapeDotable(const IPropertySignature* ldotable, const IPropertySignature* rdotable) {
    if (ldotable == rdotable) {
      return true;
    }
    if ((ldotable == nullptr) || (rdotable == nullptr)) {
      return false;
    }
    return true; // TODO
  }

  bool isAssignableInstanceObjectShapeIndexable(const IIndexSignature* lindexable, const IIndexSignature* rindexable) {
    if (lindexable == rindexable) {
      return true;
    }
    if ((lindexable == nullptr) || (rindexable == nullptr)) {
      return false;
    }
    return true; // TODO
  }

  bool isAssignableInstanceObjectShapeIterable(const IIteratorSignature* literable, const IIteratorSignature* riterable) {
    if (literable == riterable) {
      return true;
    }
    if ((literable == nullptr) || (riterable == nullptr)) {
      return false;
    }
    return true; // TODO
  }

  bool isAssignableInstanceObjectShape(const ObjectShape* lshape, const ObjectShape* rshape) {
    assert(lshape != nullptr);
    assert(rshape != nullptr);
    return
      isAssignableInstanceObjectShapeCallable(lshape->callable, rshape->callable) &&
      isAssignableInstanceObjectShapeDotable(lshape->dotable, rshape->dotable) &&
      isAssignableInstanceObjectShapeIndexable(lshape->indexable, rshape->indexable) &&
      isAssignableInstanceObjectShapeIterable(lshape->iterable, rshape->iterable);
  }

  bool isAssignableInstanceObject(const IType& ltype, const IType& rtype) {
    // Complex (non-pointer) objects can be assigned if *any* of their object shapes are compatible
    auto lcount = ltype.getObjectShapeCount();
    auto rcount = rtype.getObjectShapeCount();
    for (size_t lindex = 0; lindex < lcount; ++lindex) {
      for (size_t rindex = 0; rindex < rcount; ++rindex) {
        if (isAssignableInstanceObjectShape(ltype.getObjectShape(lindex), rtype.getObjectShape(rindex))) {
          return true;
        }
      }
    }
    return false;
  }

  bool isAssignableInstancePointerShapePointee(const IType* lpointee, const IType* rpointee) {
    assert(lpointee != nullptr);
    assert(rpointee != nullptr);
    if (lpointee == rpointee) {
      return true;
    }
    return false; // TODO
  }

  bool isAssignableInstancePointerShape(const PointerShape* lshape, const PointerShape* rshape) {
    assert(lshape != nullptr);
    assert(rshape != nullptr);
    return isAssignableInstancePointerShapePointee(lshape->pointee, rshape->pointee);
  }

  bool isAssignableInstancePointer(const IType& ltype, const IType& rtype) {
    // Pointers can be assigned if *any* of their pointees have identical types
    auto lcount = ltype.getPointerShapeCount();
    auto rcount = rtype.getPointerShapeCount();
    for (size_t lindex = 0; lindex < lcount; ++lindex) {
      for (size_t rindex = 0; rindex < rcount; ++rindex) {
        if (isAssignableInstancePointerShape(ltype.getPointerShape(lindex), rtype.getPointerShape(rindex))) {
          return true;
        }
      }
    }
    return false;
  }

  bool isAssignableInstance(const IType& ltype, const IObject& rhs) {
    // Determine if we can assign an object to a complex target
    assert(ltype.getPrimitiveFlags() == ValueFlags::None);
    auto* rtype = rhs.getRuntimeType().get();
    assert(rtype != nullptr);
    if (&ltype == rtype) {
      return true;
    }
    if (isAssignableInstanceObject(ltype, *rtype)) {
      return true;
    }
    return isAssignableInstancePointer(ltype, *rtype);
  }
}

egg::ovum::Type::Assignability egg::ovum::Type::queryAssignable(const IType& from) const {
  auto* to = this->get();
  assert(to != nullptr);
  return queryAssignableType(*to, from);
}

const egg::ovum::IFunctionSignature* egg::ovum::Type::queryCallable() const {
  return queryObjectShape<IFunctionSignature>(this->get(), &ObjectShape::callable);
}

const egg::ovum::IPropertySignature* egg::ovum::Type::queryDotable() const {
  return queryObjectShape<IPropertySignature>(this->get(), &ObjectShape::dotable);
}

const egg::ovum::IIndexSignature* egg::ovum::Type::queryIndexable() const {
  return queryObjectShape<IIndexSignature>(this->get(), &ObjectShape::indexable);
}

const egg::ovum::IIteratorSignature* egg::ovum::Type::queryIterable() const {
  return queryObjectShape<IIteratorSignature>(this->get(), &ObjectShape::iterable);
}

const egg::ovum::PointerShape* egg::ovum::Type::queryPointable() const {
  if (this->hasPrimitiveFlag(ValueFlags::Object)) {
    // Allow a pointer to anything
    static const PointerShape anything{ &typeAnyQ, Modifiability::Read | Modifiability::Write | Modifiability::Mutate };
    return &anything;
  }
  auto count = (*this)->getPointerShapeCount();
  assert(count <= 1);
  if (count > 0) {
    auto* shape = (*this)->getPointerShape(0);
    if (shape != nullptr) {
      return shape;
    }
  }
  return nullptr;
}

egg::ovum::String egg::ovum::Type::describeValue() const {
  auto* type = this->get();
  if (type == nullptr) {
    return "Value of unknown type";
  }
  return type->describeValue();
}

egg::ovum::String egg::ovum::Type::toString() const {
  auto* type = this->get();
  if (type == nullptr) {
    return "<unknown>";
  }
  return type->toStringPrecedence().first;
}

egg::ovum::TypeFactory::TypeFactory(IAllocator& allocator)
  : allocator(allocator) {
  registerSimpleBasic(typeNone);
  registerSimpleBasic(typeVoid);
  registerSimpleBasic(typeNull);
  registerSimpleBasic(typeBool);
  registerSimpleBasic(typeBoolInt);
  registerSimpleBasic(typeInt);
  registerSimpleBasic(typeIntQ);
  registerSimpleBasic(typeFloat);
  registerSimpleBasic(typeString);
  registerSimpleBasic(typeArithmetic);
  registerSimpleBasic(typeObject);
  registerSimpleBasic(typeAny);
  registerSimpleBasic(typeAnyQ);
}

egg::ovum::Type egg::ovum::TypeFactory::createSimple(ValueFlags flags) {
  {
    // First, try to fetch an existing entry
    ReadLock lockR{ this->mutex };
    auto found = this->simple.find(flags);
    if (found != this->simple.end()) {
      return found->second;
    }
  }

  // We need to modify the map after releasing the read lock
  WriteLock lockW{ this->mutex };
  auto created = this->allocator.makeHard<TypeSimple, Type>(flags);
  return this->simple.try_emplace(flags, created).first->second;
}

egg::ovum::Type egg::ovum::TypeFactory::createPointer(const Type& pointee, Modifiability modifiability) {
  assert(pointee != nullptr);
  return this->allocator.makeHard<TypePointer, Type>(pointee, modifiability);
}

egg::ovum::Type egg::ovum::TypeFactory::createUnion(const Type& a, const Type& b) {
  assert(a != nullptr);
  assert(b != nullptr);
  if (a.get() == b.get()) {
    return a;
  }
  return this->allocator.makeHard<TypeUnion, Type>(a, b);
}

egg::ovum::Type egg::ovum::TypeFactory::addVoid(const Type& type) {
  return this->addPrimitiveFlag(type, ValueFlags::Void);
}

egg::ovum::Type egg::ovum::TypeFactory::addNull(const Type& type) {
  return this->addPrimitiveFlag(type, ValueFlags::Null);
}

egg::ovum::Type egg::ovum::TypeFactory::addPrimitiveFlag(const Type& type, ValueFlags flag) {
  // TODO optimize
  assert(type != nullptr);
  auto existingFlags = type->getPrimitiveFlags();
  auto requiredFlags = Bits::set(type->getPrimitiveFlags(), flag);
  if (existingFlags != requiredFlags) {
    if (!type.isComplex()) {
      return this->createSimple(requiredFlags);
    }
    return this->createUnion(type, this->createSimple(flag)); // WIBBLE
  }
  return type;
}

egg::ovum::Type egg::ovum::TypeFactory::removeVoid(const Type& type) {
  return removePrimitiveFlag(type, ValueFlags::Void);
}

egg::ovum::Type egg::ovum::TypeFactory::removeNull(const Type& type) {
  return removePrimitiveFlag(type, ValueFlags::Null);
}

egg::ovum::Type egg::ovum::TypeFactory::removePrimitiveFlag(const Type& type, ValueFlags flag) {
  // TODO optimize
  if (type != nullptr) {
    auto existingFlags = type->getPrimitiveFlags();
    auto requiredFlags = Bits::clear(type->getPrimitiveFlags(), flag);
    if (existingFlags != requiredFlags) {
      if (!type.isComplex()) {
        return this->createSimple(requiredFlags);
      }
      if (requiredFlags == ValueFlags::None) {
        return nullptr;
      }
      return this->allocator.makeHard<TypeStrip, Type>(type, flag); // WIBBLE
    }
  }
  return type;
}

egg::ovum::TypeBuilder egg::ovum::TypeFactory::createTypeBuilder(const String& name, const String& description) {
  return this->allocator.makeHard<Builder>(name, description);
}

egg::ovum::TypeBuilder egg::ovum::TypeFactory::createFunctionBuilder(const Type& rettype, const String& name, const String& description) {
  return this->allocator.makeHard<FunctionBuilder>(rettype, name, description);
}

egg::ovum::TypeBuilder egg::ovum::TypeFactory::createGeneratorBuilder(const Type& gentype, const String& name, const String& description) {
  // Convert the return type (e.g. 'int') into a generator function 'int...' aka '(void|int)()'
  assert(!gentype.hasPrimitiveFlag(ValueFlags::Void));
  auto rettype = this->addVoid(gentype);
  auto generator = this->createFunctionBuilder(rettype, name, description);
  generator->defineIterable(gentype);
  return this->allocator.makeHard<FunctionBuilder>(generator->build(), name, description);
}

void egg::ovum::TypeFactory::registerSimpleBasic(const IType& type) {
  this->simple.emplace(type.getPrimitiveFlags(), Type(&type));
}

// Common types
const egg::ovum::Type egg::ovum::Type::Void{ &typeVoid };
const egg::ovum::Type egg::ovum::Type::Null{ &typeNull };
const egg::ovum::Type egg::ovum::Type::Bool{ &typeBool };
const egg::ovum::Type egg::ovum::Type::BoolInt{ &typeBoolInt };
const egg::ovum::Type egg::ovum::Type::Int{ &typeInt };
const egg::ovum::Type egg::ovum::Type::IntQ{ &typeIntQ };
const egg::ovum::Type egg::ovum::Type::Float{ &typeFloat };
const egg::ovum::Type egg::ovum::Type::String{ &typeString };
const egg::ovum::Type egg::ovum::Type::Arithmetic{ &typeArithmetic };
const egg::ovum::Type egg::ovum::Type::Object{ &typeObject };
const egg::ovum::Type egg::ovum::Type::Any{ &typeAny };
const egg::ovum::Type egg::ovum::Type::AnyQ{ &typeAnyQ };

egg::ovum::Type::Assignment egg::ovum::Type::promote(IAllocator& allocator, const Value& rhs, Value& out) const {
  // WOBBLE
  assert(rhs.validate());
  switch (rhs->getFlags()) {
  case ValueFlags::Void:
    return Assignment::Incompatible;
  case ValueFlags::Null:
    if (!this->hasPrimitiveFlag(ValueFlags::Null)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Bool:
    if (!this->hasPrimitiveFlag(ValueFlags::Bool)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Int:
    if (!this->hasPrimitiveFlag(ValueFlags::Int)) {
      if (this->hasPrimitiveFlag(ValueFlags::Float)) {
        egg::ovum::Int i;
        if (rhs->getInt(i)) {
          // Attempt int-to-float promotion
          auto f = egg::ovum::Float(i);
          if (egg::ovum::Int(f) == i) {
            // Can promote the integer to a float
            out = ValueFactory::createFloat(allocator, f);
            return Assignment::Success;
          }
          return Assignment::BadIntToFloat;
        }
      }
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Float:
    if (!this->hasPrimitiveFlag(ValueFlags::Float)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::String:
    if (!this->hasPrimitiveFlag(ValueFlags::String)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Object:
    if (this->get()->getPrimitiveFlags() == ValueFlags::None) {
      // Complex
      egg::ovum::Object o;
      if (!rhs->getObject(o) || !isAssignableInstance(**this, *o)) {
        return Assignment::Incompatible;
      }
    } else if (!this->hasPrimitiveFlag(ValueFlags::Object)) {
      // We cannot assign any object-like value to this type
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::None:
  case ValueFlags::Any:
  case ValueFlags::AnyQ:
  case ValueFlags::Arithmetic:
  case ValueFlags::FlowControl:
  case ValueFlags::Break:
  case ValueFlags::Continue:
  case ValueFlags::Return:
  case ValueFlags::Yield:
  case ValueFlags::Throw:
  default:
    return Assignment::Incompatible;
  }
  out = rhs;
  return Assignment::Success;
}

namespace {
  using namespace egg::ovum;

  Type::Assignment mutateDelta(IAllocator& allocator, const Value& lhs, Int rhs, Value& out) {
    Int i;
    if (!lhs->getInt(i)) {
      return Type::Assignment::Incompatible;
    }
    out = ValueFactory::createInt(allocator, i + rhs);
    return Type::Assignment::Success;
  }

  Type::Assignment mutateBool(IAllocator&, Bool lhs, Bool rhs, Mutation mutation, Value& out) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (mutation) {
    case Mutation::BitwiseAnd:
    case Mutation::LogicalAnd:
      // No harm in using the short-circuit form (MSVC complains less too!)
      out = ValueFactory::createBool(lhs && rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseOr:
    case Mutation::LogicalOr:
      // No harm in using the short-circuit form (MSVC complains less too!)
      out = ValueFactory::createBool(lhs || rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseXor:
      out = ValueFactory::createBool(lhs ^ rhs);
      return Type::Assignment::Success;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    throw std::logic_error("Unsupported bool mutation");
  }

  Type::Assignment mutateInt(IAllocator& allocator, Int lhs, Int rhs, Mutation mutation, Value& out) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (mutation) {
    case Mutation::Add:
      out = ValueFactory::createInt(allocator, lhs + rhs);
      return Type::Assignment::Success;
    case Mutation::Subtract:
      out = ValueFactory::createInt(allocator, lhs - rhs);
      return Type::Assignment::Success;
    case Mutation::Multiply:
      out = ValueFactory::createInt(allocator, lhs * rhs);
      return Type::Assignment::Success;
    case Mutation::Divide:
      out = ValueFactory::createInt(allocator, lhs / rhs);
      return Type::Assignment::Success;
    case Mutation::Remainder:
      out = ValueFactory::createInt(allocator, lhs % rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseAnd:
      out = ValueFactory::createInt(allocator, lhs & rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseOr:
      out = ValueFactory::createInt(allocator, lhs | rhs);
      return Type::Assignment::Success;
    case Mutation::BitwiseXor:
      out = ValueFactory::createInt(allocator, lhs ^ rhs);
      return Type::Assignment::Success;
    case Mutation::ShiftLeft:
      if (rhs < 0) {
        out = ValueFactory::createInt(allocator, lhs >> -rhs);
      } else {
        out = ValueFactory::createInt(allocator, lhs << rhs);
      }
      return Type::Assignment::Success;
    case Mutation::ShiftRight:
      if (rhs < 0) {
        out = ValueFactory::createInt(allocator, lhs << -rhs);
      } else {
        out = ValueFactory::createInt(allocator, lhs >> rhs);
      }
      return Type::Assignment::Success;
    case Mutation::ShiftRightUnsigned:
      if (rhs < 0) {
        out = ValueFactory::createInt(allocator, lhs << -rhs);
      } else {
        out = ValueFactory::createInt(allocator, Int(uint64_t(lhs) >> rhs));
      }
      return Type::Assignment::Success;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    throw std::logic_error("Unsupported int mutation");
  }

  Type::Assignment mutateFloat(IAllocator& allocator, Float lhs, Float rhs, Mutation mutation, Value& out) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (mutation) {
    case Mutation::Add:
      out = ValueFactory::createFloat(allocator, lhs + rhs);
      return Type::Assignment::Success;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END
    throw std::logic_error("Unsupported float mutation");
  }

  Type::Assignment mutateArithmetic(IAllocator& allocator, const Type& type, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) {
    Float flhs;
    if (lhs->getFloat(flhs)) {
      Float frhs;
      if (rhs->getFloat(frhs)) {
        assert(type.hasPrimitiveFlag(ValueFlags::Float));
        return mutateFloat(allocator, flhs, frhs, mutation, out);
      }
      Int irhs;
      if (rhs->getInt(irhs)) {
        assert(type.hasPrimitiveFlag(ValueFlags::Float));
        // Allow inaccurate promotion
        return mutateFloat(allocator, flhs, Float(irhs), mutation, out);
      }
      return Type::Assignment::Incompatible;
    }
    Int ilhs;
    if (lhs->getInt(ilhs)) {
      Int irhs;
      if (rhs->getInt(irhs)) {
        assert(type.hasPrimitiveFlag(ValueFlags::Int));
        return mutateInt(allocator, ilhs, irhs, mutation, out);
      }
      Float frhs;
      if (rhs->getFloat(frhs)) {
        if (!type.hasPrimitiveFlag(ValueFlags::Float)) {
          return Type::Assignment::BadIntToFloat;
        }
        // Allow inaccurate promotion
        return mutateFloat(allocator, Float(ilhs), frhs, mutation, out);
      }
    }
    return Type::Assignment::Incompatible;
  }

  Type::Assignment mutateBitwise(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) {
    Int ilhs;
    if (lhs->getInt(ilhs)) {
      Int irhs;
      if (rhs->getInt(irhs)) {
        return mutateInt(allocator, ilhs, irhs, mutation, out);
      }
    }
    Bool blhs;
    if (lhs->getBool(blhs)) {
      Bool brhs;
      if (rhs->getBool(brhs)) {
        return mutateBool(allocator, blhs, brhs, mutation, out);
      }
    }
    return Type::Assignment::Incompatible;
  }

  Type::Assignment mutateLogical(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) {
    Bool blhs;
    if (lhs->getBool(blhs)) {
      Bool brhs;
      if (rhs->getBool(brhs)) {
        return mutateBool(allocator, blhs, brhs, mutation, out);
      }
    }
    return Type::Assignment::Incompatible;
  }

  Type::Assignment mutateShift(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) {
    Int ilhs;
    if (lhs->getInt(ilhs)) {
      Int irhs;
      if (rhs->getInt(irhs)) {
        return mutateInt(allocator, ilhs, irhs, mutation, out);
      }
    }
    return Type::Assignment::Incompatible;
  }

  Type::Assignment mutateIfNull(IAllocator&, const Value& lhs, const Value& rhs, Value& out) {
    out = lhs->getNull() ? rhs : lhs;
    return Type::Assignment::Success;
  }
}

egg::ovum::Type::Assignment egg::ovum::Type::mutate(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) const {
  assert(lhs.validate());
  assert(rhs.validate());
  switch (mutation) {
  case Mutation::Assign:
    return this->promote(allocator, rhs, out);
  case Mutation::Noop:
    out = lhs;
    return Assignment::Success;
  case Mutation::Decrement:
    assert(rhs->getVoid());
    return mutateDelta(allocator, lhs, -1, out);
  case Mutation::Increment:
    assert(rhs->getVoid());
    return mutateDelta(allocator, lhs, +1, out);
  case Mutation::Add:
  case Mutation::Subtract:
  case Mutation::Multiply:
  case Mutation::Divide:
  case Mutation::Remainder:
    return mutateArithmetic(allocator, *this, lhs, rhs, mutation, out);
  case Mutation::BitwiseAnd:
  case Mutation::BitwiseOr:
  case Mutation::BitwiseXor:
    return mutateBitwise(allocator, lhs, rhs, mutation, out);
  case Mutation::LogicalAnd:
  case Mutation::LogicalOr:
    return mutateLogical(allocator, lhs, rhs, mutation, out);
  case Mutation::ShiftLeft:
  case Mutation::ShiftRight:
  case Mutation::ShiftRightUnsigned:
    return mutateShift(allocator, lhs, rhs, mutation, out);
  case Mutation::IfNull:
    return mutateIfNull(allocator, lhs, rhs, out);
  }
  throw std::logic_error("Unsupported value mutation");
}
