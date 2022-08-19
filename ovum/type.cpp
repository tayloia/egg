#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/node.h"
#include "ovum/builtin.h"
#include "ovum/function.h"

#include <array>
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

  std::pair<std::string, int> complexComponentObject(const ObjectShape* shape) {
    assert(shape != nullptr);
    if (shape->callable != nullptr) {
      return FunctionSignature::toStringPrecedence(*shape->callable);
    }
    if (shape->pointable != nullptr) {
      auto pointee = shape->pointable->getType()->toStringPrecedence();
      if (pointee.second > 1) {
        return { '(' + pointee.first + ')' + '*', 1 };
      }
      return { pointee.first + '*', 1 };
    }
    return { "<complex>", 0 }; // TODO
  }

  std::pair<std::string, int> complexToStringPrecedence(ValueFlags primitive, const IType& complex) {
    auto shapes = complex.getObjectShapeCount();
    if (shapes == 0) {
      // Primitive types only
      return primitiveToStringPrecedence(primitive);
    }
    std::pair<std::string, int> result;
    if (primitive != ValueFlags::None) {
      result = primitiveToStringPrecedence(primitive);
    }
    std::set<std::string> parts;
    for (size_t index = 0; index < shapes; ++index) {
      auto next = complexComponentObject(complex.getObjectShape(index));
      assert(!next.first.empty());
      assert((next.second >= 0) && (next.second <= 2));
      if ((index == 0) && result.first.empty()) {
        result.second = next.second;
      } else {
        result.second = 2;
      }
      parts.insert(next.first);
    }
    for (auto& part : parts) {
      // Lexigraphically ordered for stability
      if (result.first.empty()) {
        result.first = part;
      } else {
        result.first += '|' + part;
      }
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
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return primitiveToStringPrecedence(this->flags);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  class TypePointer final : public HardReferenceCounted<IType>, IPointerSignature {
    TypePointer(const TypePointer&) = delete;
    TypePointer& operator=(const TypePointer&) = delete;
  private:
    Type pointee;
    Modifiability modifiability;
    ObjectShape shape;
  public:
    TypePointer(IAllocator& allocator, const Type& pointee, Modifiability modifiability)
      : HardReferenceCounted(allocator, 0),
        pointee(pointee),
        modifiability(modifiability),
        shape{} {
      assert(this->pointee != nullptr);
      this->shape.pointable = this;
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
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return complexToStringPrecedence(ValueFlags::None, *this);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Pointer of type '", this->toStringPrecedence().first, "'");
    }
    virtual Type getType() const override {
      // IPointerSignature
      return this->pointee;
    }
    virtual Modifiability getModifiability() const override {
      // IPointerSignature
      return this->modifiability;
    }
  };

  class TypeComplex final : public HardReferenceCounted<IType> {
    TypeComplex(const TypeComplex&) = delete;
    TypeComplex& operator=(const TypeComplex&) = delete;
  private:
    ValueFlags flags;
    std::vector<const ObjectShape*> objectShapes;
  public:
    TypeComplex(IAllocator& allocator, ValueFlags flags, std::vector<const ObjectShape*>&& objectShapes)
      : HardReferenceCounted(allocator, 0),
        flags(flags),
        objectShapes(std::move(objectShapes)) {
      assert(!this->objectShapes.empty());
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
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return complexToStringPrecedence(this->flags, *this);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
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
        builder(&builder),
        shape{} {
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
      // We have at least one primitive type and possibly object shapes
      auto assignability = queryAssignablePrimitives(ltype, rtype, lflags, rflags);
      if (assignability == Type::Assignability::Sometimes) {
        return Type::Assignability::Sometimes;
      }
      if ((rtype.getObjectShapeCount() > 0) && (queryAssignableObject(ltype, rtype) != assignability)) {
        return Type::Assignability::Sometimes;
      }
      return assignability;
    }
    if (rtype.getObjectShapeCount() > 0) {
      // We have no primitive types, but at least one object shape
      return queryAssignableObject(ltype, rtype);
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

  bool isAssignableInstanceObjectShapePointable(const IPointerSignature* lpointable, const IPointerSignature* rpointable) {
    if (lpointable == rpointable) {
      return true;
    }
    if ((lpointable == nullptr) || (rpointable == nullptr)) {
      return false;
    }
    return lpointable->getType().get() == rpointable->getType().get();
  }

  bool isAssignableInstanceObjectShape(const ObjectShape* lshape, const ObjectShape* rshape) {
    assert(lshape != nullptr);
    assert(rshape != nullptr);
    return isAssignableInstanceObjectShapeCallable(lshape->callable, rshape->callable) &&
           isAssignableInstanceObjectShapeDotable(lshape->dotable, rshape->dotable) &&
           isAssignableInstanceObjectShapeIndexable(lshape->indexable, rshape->indexable) &&
           isAssignableInstanceObjectShapeIterable(lshape->iterable, rshape->iterable) &&
           isAssignableInstanceObjectShapePointable(lshape->pointable, rshape->pointable);
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

  bool isAssignableInstance(const IType& ltype, const IObject& rhs) {
    // Determine if we can assign an object to a complex target
    assert(ltype.getPrimitiveFlags() == ValueFlags::None);
    auto* rtype = rhs.getRuntimeType().get();
    assert(rtype != nullptr);
    if (&ltype == rtype) {
      return true;
    }
    return isAssignableInstanceObject(ltype, *rtype);
  }

  size_t findObjectShapeIndex(const std::vector<ObjectShape>& shapes, const ObjectShape& shape, size_t start, size_t count) {
    auto index = start;
    while (index < count) {
      if (Type::areEquivalent(shapes[index], shape)) {
        break;
      }
      ++index;
    }
    return index;
  }

  size_t findComplexIndex(const std::vector<TypeFactory::Complex>& complexes, const TypeFactory::Complex& complex, size_t start, size_t count) {
    auto type = complex.type.get();
    auto index = start;
    if (type == nullptr) {
      // Search by equivalence
      while (index < count) {
        if (TypeFactory::Complex::areEquivalent(complexes[index], complex)) {
          break;
        }
        ++index;
      }
    } else {
      // Search by existing type
      while (index < count) {
        if (complexes[index].type.get() == type) {
          break;
        }
        ++index;
      }
    }
    return index;
  }

  void registerSimpleBasic(std::map<ValueFlags, Type>& simples, const IType& type) {
    simples.emplace(type.getPrimitiveFlags(), Type(&type));
  }

  size_t registerObjectShapeWithLock(ReadWriteMutex& mutex, std::vector<ObjectShape>& shapes, const ObjectShape& shape) {
    // First, try to fetch an existing entry
    ReadLock lockR{ mutex };
    auto countR = shapes.size();
    auto index = findObjectShapeIndex(shapes, shape, 0, countR);
    if (index < countR) {
      return index;
    }
    lockR.unlock();

    // Obtain a write lock
    WriteLock lockW{ mutex };
    auto countW = shapes.size();
    if (countW > countR) {
      // Another thread has added entries we need to re-check
      index = findObjectShapeIndex(shapes, shape, countR, countW);
      if (index < countW) {
        return index;
      }
    }
    shapes.emplace_back(shape);
    return countW;
  }

  size_t registerObjectShapeWithoutLock(std::vector<ObjectShape>& shapes, const ObjectShape& shape) {
    auto count = shapes.size();
    auto index = findObjectShapeIndex(shapes, shape, 0, count);
    if (index < count) {
      return index;
    }
    shapes.emplace_back(shape);
    return count;
  }
}

Type::Assignability Type::queryAssignable(const IType& from) const {
  auto* to = this->get();
  assert(to != nullptr);
  return queryAssignableType(*to, from);
}

const IFunctionSignature* Type::queryCallable() const {
  return queryObjectShape<IFunctionSignature>(this->get(), &ObjectShape::callable);
}

const IPropertySignature* Type::queryDotable() const {
  return queryObjectShape<IPropertySignature>(this->get(), &ObjectShape::dotable);
}

const IIndexSignature* Type::queryIndexable() const {
  return queryObjectShape<IIndexSignature>(this->get(), &ObjectShape::indexable);
}

const IIteratorSignature* Type::queryIterable() const {
  return queryObjectShape<IIteratorSignature>(this->get(), &ObjectShape::iterable);
}

const IPointerSignature* Type::queryPointable() const {
  return queryObjectShape<IPointerSignature>(this->get(), &ObjectShape::pointable);
}

String Type::describeValue() const {
  auto* type = this->get();
  if (type == nullptr) {
    return "Value of unknown type";
  }
  return type->describeValue();
}

String Type::toString() const {
  auto* type = this->get();
  if (type == nullptr) {
    return "<unknown>";
  }
  return type->toStringPrecedence().first;
}

TypeFactory::TypeFactory(IAllocator& allocator)
  : allocator(allocator) {
  registerSimpleBasic(this->simples, typeNone);
  registerSimpleBasic(this->simples, typeVoid);
  registerSimpleBasic(this->simples, typeNull);
  registerSimpleBasic(this->simples, typeBool);
  registerSimpleBasic(this->simples, typeBoolInt);
  registerSimpleBasic(this->simples, typeInt);
  registerSimpleBasic(this->simples, typeIntQ);
  registerSimpleBasic(this->simples, typeFloat);
  registerSimpleBasic(this->simples, typeString);
  registerSimpleBasic(this->simples, typeArithmetic);
  registerSimpleBasic(this->simples, typeObject);
  registerSimpleBasic(this->simples, typeAny);
  registerSimpleBasic(this->simples, typeAnyQ);
}

Type TypeFactory::createSimple(ValueFlags flags) {
  // First, try to fetch an existing entry
  ReadLock lockR{ this->mutex };
  auto found = this->simples.find(flags);
  if (found != this->simples.end()) {
    return found->second;
  }
  lockR.unlock();

  // We need to modify the map after releasing the read lock
  auto created = this->allocator.makeHard<TypeSimple, Type>(flags);
  WriteLock lockW{ this->mutex };
  return this->simples.try_emplace(flags, created).first->second;
}

Type TypeFactory::createPointer(const Type& pointee, Modifiability modifiability) {
  auto* ptr = pointee.get();
  assert(ptr != nullptr);

  // First, try to fetch an existing entry
  ReadLock lockR{ this->mutex };
  auto found = this->pointers.find(ptr);
  if (found != this->pointers.end()) {
    return found->second;
  }
  lockR.unlock();

  // We need to modify the map after releasing the read lock
  auto created = this->allocator.makeHard<TypePointer, Type>(pointee, modifiability);
  WriteLock lockW{ this->mutex };
  auto emplaced = this->pointers.try_emplace(ptr, created);
  if (emplaced.second) {
    // We added 'created' so construct a complex entry for it too
    assert(emplaced.first->second.get() == created.get());
    auto shapePointer = created->getObjectShape(0);
    assert(shapePointer != nullptr);
    auto shapeIndex = registerObjectShapeWithoutLock(this->shapes, *shapePointer);
    Complex complex;
    complex.shapeIndices.insert(shapeIndex);
    complex.type = created;
    this->complexes.emplace_back(std::move(complex));
    return created;
  }
  return emplaced.first->second;
}

Type TypeFactory::createUnion(const std::vector<Type>& types) {
  switch (types.size()) {
  case 0:
    return nullptr;
  case 1:
    return types[0];
  }
  Complex merged;
  for (auto& type : types) {
    if (type != nullptr) {
      merged.merge(*this, *type);
    }
  }
  if (merged.shapeIndices.empty()) {
    return this->createSimple(merged.flags);
  }
  return createComplex(merged);
}

Type TypeFactory::createComplex(Complex& complex) {
  ReadLock lockR{ this->mutex };
  auto countR = this->complexes.size();
  auto index = findComplexIndex(this->complexes, complex, 0, countR);
  if (index < countR) {
    complex = this->complexes[index];
    return complex.type;
  }
  lockR.unlock();

  // Not found the first time through
  Type type = complex.type;
  if (type == nullptr) {
    // Probably need to insert a new complex type
    std::vector<const ObjectShape*> objectShapes;
    objectShapes.reserve(complex.shapeIndices.size());
    for (auto shapeIndex : complex.shapeIndices) {
      objectShapes.push_back(&this->shapes[shapeIndex]);
    }
    type = this->allocator.makeHard<TypeComplex, Type>(complex.flags, std::move(objectShapes));
  } else {
    // Fill the details in case we need to add this type
    complex.merge(*this, *type);
  }

  // Obtain a write lock before trying again
  WriteLock lockW{ this->mutex };
  auto countW = this->complexes.size();
  if (countW > countR) {
    // Another thread has added entries we need to re-check
    index = findComplexIndex(this->complexes, complex, countR, countW);
    if (index < countW) {
      complex = this->complexes[index];
      return complex.type;
    }
  }
  complex.type = type;
  this->complexes.emplace_back(complex);
  return complex.type;
}

Type TypeFactory::createModified(const Type& type, ValueFlags flags) {
  assert(type != nullptr);
  if (type->getPrimitiveFlags() == flags) {
    return type;
  }
  if (!type.isComplex()) {
    return createSimple(flags);
  }
  Complex complex{};
  complex.type = type;
  createComplex(complex);
  assert(complex.type.get() == type.get());
  assert(complex.flags == type->getPrimitiveFlags());
  complex.flags = flags;
  complex.type = nullptr;
  return createComplex(complex);
}

Type TypeFactory::addVoid(const Type& type) {
  if (type == nullptr) {
    return Type::Void;
  }
  auto flags = Bits::set(type->getPrimitiveFlags(), ValueFlags::Void);
  return createModified(type, flags);
}

Type TypeFactory::addNull(const Type& type) {
  if (type == nullptr) {
    return Type::Null;
  }
  auto flags = Bits::set(type->getPrimitiveFlags(), ValueFlags::Null);
  return createModified(type, flags);
}

Type TypeFactory::removeVoid(const Type& type) {
  if (type == nullptr) {
    return nullptr;
  }
  auto flags = Bits::clear(type->getPrimitiveFlags(), ValueFlags::Void);
  return createModified(type, flags);
}

Type TypeFactory::removeNull(const Type& type) {
  if (type == nullptr) {
    return nullptr;
  }
  auto flags = Bits::clear(type->getPrimitiveFlags(), ValueFlags::Null);
  return createModified(type, flags);
}

TypeBuilder TypeFactory::createTypeBuilder(const String& name, const String& description) {
  return this->allocator.makeHard<Builder>(name, description);
}

TypeBuilder TypeFactory::createFunctionBuilder(const Type& rettype, const String& name, const String& description) {
  return this->allocator.makeHard<FunctionBuilder>(rettype, name, description);
}

TypeBuilder TypeFactory::createGeneratorBuilder(const Type& gentype, const String& name, const String& description) {
  // Convert the return type (e.g. 'int') into a generator function 'int...' aka '(void|int)()'
  assert(!gentype.hasPrimitiveFlag(ValueFlags::Void));
  auto rettype = this->addVoid(gentype);
  auto generator = this->createFunctionBuilder(rettype, name, description);
  generator->defineIterable(gentype);
  return this->allocator.makeHard<FunctionBuilder>(generator->build(), name, description);
}

void TypeFactory::Complex::merge(TypeFactory& factory, const IType& other) {
  this->flags = this->flags | other.getPrimitiveFlags();
  auto count = other.getObjectShapeCount();
  for (size_t index = 0; index < count; ++index) {
    auto incoming = other.getObjectShape(index);
    assert(incoming != nullptr);
    this->shapeIndices.insert(registerObjectShapeWithLock(factory.mutex, factory.shapes, *incoming));
  }
}

bool TypeFactory::Complex::areEquivalent(const Complex& lhs, const Complex& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  return (lhs.flags == rhs.flags) && (lhs.shapeIndices == rhs.shapeIndices);
}

bool Type::areEquivalent(const IType& lhs, const IType& rhs) {
  return &lhs == &rhs;
}

bool Type::areEquivalent(const ObjectShape& lhs, const ObjectShape& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  return Type::areEquivalent(lhs.callable, rhs.callable) &&
         Type::areEquivalent(lhs.dotable, rhs.dotable) &&
         Type::areEquivalent(lhs.indexable, rhs.indexable) &&
         Type::areEquivalent(lhs.iterable, rhs.iterable) &&
         Type::areEquivalent(lhs.pointable, rhs.pointable);
}

bool Type::areEquivalent(const IFunctionSignature& lhs, const IFunctionSignature& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  auto parameters = lhs.getParameterCount();
  if ((parameters != rhs.getParameterCount()) || !lhs.getReturnType().isEquivalent(rhs.getReturnType())) {
    return false;
  }
  for (size_t parameter = 0; parameter < parameters; ++parameter) {
    // TODO optional parameter relaxation?
    if (!Type::areEquivalent(lhs.getParameter(parameter), rhs.getParameter(parameter))) {
      return false;
    }
  }
  return true;
}

bool Type::areEquivalent(const IFunctionSignatureParameter& lhs, const IFunctionSignatureParameter& rhs) {
  // TODO variadic/optional/etc
  if (&lhs == &rhs) {
    return true;
  }
  return (lhs.getPosition() == rhs.getPosition()) && (lhs.getFlags() == rhs.getFlags()) && (lhs.getType().isEquivalent(rhs.getType()));
}

bool Type::areEquivalent(const IPropertySignature& lhs, const IPropertySignature& rhs) {
  // TODO
  if (&lhs == &rhs) {
    return true;
  }
  return true;
}

bool Type::areEquivalent(const IIndexSignature& lhs, const IIndexSignature& rhs) {
  // TODO modifiability
  if (&lhs == &rhs) {
    return true;
  }
  return lhs.getIndexType().isEquivalent(rhs.getIndexType()) && lhs.getResultType().isEquivalent(rhs.getResultType());
}

bool Type::areEquivalent(const IPointerSignature& lhs, const IPointerSignature& rhs) {
  // TODO modifiability
  if (&lhs == &rhs) {
    return true;
  }
  return lhs.getType().isEquivalent(rhs.getType());
}

bool Type::areEquivalent(const IIteratorSignature& lhs, const IIteratorSignature& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  return lhs.getType().isEquivalent(rhs.getType());
}

// Common types
const Type Type::Void{ &typeVoid };
const Type Type::Null{ &typeNull };
const Type Type::Bool{ &typeBool };
const Type Type::BoolInt{ &typeBoolInt };
const Type Type::Int{ &typeInt };
const Type Type::IntQ{ &typeIntQ };
const Type Type::Float{ &typeFloat };
const Type Type::String{ &typeString };
const Type Type::Arithmetic{ &typeArithmetic };
const Type Type::Object{ &typeObject };
const Type Type::Any{ &typeAny };
const Type Type::AnyQ{ &typeAnyQ };

Type::Assignment Type::promote(IAllocator& allocator, const Value& rhs, Value& out) const {
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
    if (this->isComplex()) {
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

Type::Assignment Type::mutate(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) const {
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
