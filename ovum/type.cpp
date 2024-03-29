#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/node.h"
#include "ovum/builtin.h"
#include "ovum/function.h"
#include "ovum/forge.h"

#include <array>
#include <unordered_set>

namespace {
  using namespace egg::ovum;

  const auto OptionalRead = Modifiability::READ | Modifiability::DELETE;
  const auto ReadOnly = Modifiability::READ;
  const auto ReadWriteMutate = Modifiability::READ_WRITE_MUTATE;
  const auto ReadWriteMutateDelete = Modifiability::ALL;

  template<enum ValueFlags FLAGS>
  class TypePrimitive final : public NotHardReferenceCounted<IType> {
    TypePrimitive(const TypePrimitive&) = delete;
    TypePrimitive& operator=(const TypePrimitive&) = delete;
  public:
    TypePrimitive() {}
    virtual ValueFlags getPrimitiveFlags() const override {
      return FLAGS;
    }
    virtual const TypeShape* getObjectShape(size_t) const override {
      // By default, we are not shaped like objects
      return nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      // By default, we are not shaped like objects
      return 0;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return Forge::simpleToStringPrecedence(FLAGS);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
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

  class Builder : public HardReferenceCounted<ITypeBuilder> {
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
  protected:
    struct Properties {
      std::vector<Forge::Property> properties;
      const IType* unknownType;
      Modifiability unknownModifiability;
    };
    struct Function {
      const IType* returnType;
      const IType* generatorType;
      std::vector<Forge::Parameter> parameters;
    };
    Forge& forge;
    String name;
    String description;
    std::unique_ptr<Function> callable;
    std::unique_ptr<Properties> dotable;
    const IIndexSignature* indexable;
    const IIteratorSignature* iterable;
    bool built;
  public:
    Builder(Forge& forge, const String& name, const String& description)
      : HardReferenceCounted(forge.getAllocator(), 0),
        forge(forge),
        name(name.toUTF8()),
        description(description.toUTF8()),
        callable(nullptr),
        dotable(nullptr),
        indexable(nullptr),
        iterable(nullptr),
        built(false) {
    }
    virtual void addPositionalParameter(const Type& type, const String& name, bool optional) override;
    virtual void addNamedParameter(const Type& type, const String& name, bool optional) override;
    virtual void addVariadicParameter(const Type& type, const String& name, bool optional) override;
    virtual void addPredicateParameter(const Type& type, const String& name, bool optional) override;
    virtual void addProperty(const Type& ptype, const String& pname, Modifiability modifiability) override;
    virtual void defineCallable(const Type& rettype, const Type& gentype) override;
    virtual void defineDotable(const Type& unknownType, Modifiability unknownModifiability) override;
    virtual void defineIndexable(const Type& resultType, const Type& indexType, Modifiability modifiability) override;
    virtual void defineIterable(const Type& resultType) override;
    virtual Type build() override;
  };

  void Builder::addPositionalParameter(const Type& ptype, const String& pname, bool optional) {
    if (this->callable == nullptr) {
      throw std::logic_error("TypeBuilder::addPositionalParameter() called before TypeBuilder::defineCallable()");
    }
    this->callable->parameters.emplace_back(pname, ptype, optional, Forge::Parameter::Kind::Positional);
  }

  void Builder::addNamedParameter(const Type& ptype, const String& pname, bool optional) {
    if (this->callable == nullptr) {
      throw std::logic_error("TypeBuilder::addNamedParameter() called before TypeBuilder::defineCallable()");
    }
    this->callable->parameters.emplace_back(pname, ptype, optional, Forge::Parameter::Kind::Named);
  }

  void Builder::addVariadicParameter(const Type& ptype, const String& pname, bool optional) {
    if (this->callable == nullptr) {
      throw std::logic_error("TypeBuilder::addVariadicParameter() called before TypeBuilder::defineCallable()");
    }
    this->callable->parameters.emplace_back(pname, ptype, optional, Forge::Parameter::Kind::Variadic);
  }

  void Builder::addPredicateParameter(const Type& ptype, const String& pname, bool optional) {
    if (this->callable == nullptr) {
      throw std::logic_error("TypeBuilder::addPredicateParameter() called before TypeBuilder::defineCallable()");
    }
    this->callable->parameters.emplace_back(pname, ptype, optional, Forge::Parameter::Kind::Predicate);
  }

  void Builder::addProperty(const Type& ptype, const String& pname, Modifiability modifiability) {
    if (this->dotable == nullptr) {
      throw std::logic_error("TypeBuilder::addProperty() called before TypeBuilder::defineDotable()");
    }
    this->dotable->properties.emplace_back(pname, ptype, modifiability);
  }

  void Builder::defineCallable(const Type& rettype, const Type& gentype) {
    assert(rettype != nullptr);
    if (this->callable != nullptr) {
      throw std::logic_error("TypeBuilder::defineCallable() called more than once");
    }
    this->callable = std::make_unique<Function>();
    this->callable->returnType = rettype.get();
    this->callable->generatorType = gentype.get();
  }

  void Builder::defineDotable(const Type& unknownType, Modifiability unknownModifiability) {
    assert((unknownType == nullptr) || (unknownModifiability != Modifiability::NONE));
    assert((unknownModifiability == Modifiability::NONE) || (unknownType != nullptr));
    if (this->dotable != nullptr) {
      throw std::logic_error("TypeBuilder::defineDotable() called more than once");
    }
    this->dotable = std::make_unique<Properties>();
    this->dotable->unknownType = unknownType.get();
    this->dotable->unknownModifiability = unknownModifiability;
  }

  void Builder::defineIndexable(const Type& resultType, const Type& indexType, Modifiability modifiability) {
    if (this->indexable != nullptr) {
      throw std::logic_error("TypeBuilder::defineIndexable() called more than once");
    }
    this->indexable = this->forge.forgeIndexSignature(*resultType, indexType.get(), modifiability);
  }

  void Builder::defineIterable(const Type& resultType) {
    if (this->iterable != nullptr) {
      throw std::logic_error("TypeBuilder::defineIterable() called more than once");
    }
    this->iterable = this->forge.forgeIteratorSignature(*resultType);
  }

  Type Builder::build() {
    if (this->built) {
      throw std::logic_error("TypeBuilder::build() called more than once");
    }
    this->built = true;
    if (this->callable == nullptr) {
      // Not a function or generator
      auto* shape = this->forge.forgeTypeShape(
        nullptr,
        this->dotable ? this->forge.forgePropertySignature(this->dotable->properties, this->dotable->unknownType, this->dotable->unknownModifiability) : nullptr,
        this->indexable,
        this->iterable,
        nullptr);
      assert(shape != nullptr);
      if (this->description.empty()) {
        if (this->name.empty()) {
          this->description = "Value";
        } else {
          this->description = StringBuilder::concat("Value of type '", this->name, "'");
        }
      }
      if (this->name.empty()) {
        return Type(this->forge.forgeComplex(ValueFlags::None, { shape }, nullptr, &this->description));
      }
      auto precedence = std::make_pair(this->name.toUTF8(), 0);
      return Type(this->forge.forgeComplex(ValueFlags::None, { shape }, &precedence, &this->description));
    }
    auto* shape = this->forge.forgeTypeShape(
      this->forge.forgeFunctionSignature(*this->callable->returnType, this->callable->generatorType, this->name, this->callable->parameters),
      this->dotable ? this->forge.forgePropertySignature(this->dotable->properties, this->dotable->unknownType, this->dotable->unknownModifiability) : nullptr,
      this->indexable,
      this->iterable,
      nullptr);
    assert(shape != nullptr);
    if (this->description.empty()) {
      StringBuilder sb;
      if (shape->callable->getGeneratorType() == nullptr) {
        sb.add("Function '");
      } else {
        sb.add("Generator '");
      }
      FunctionSignature::print(sb, *shape->callable);
      sb.add("'");
      this->description = sb.str();
    }
    auto precedence = FunctionSignature::toStringPrecedence(*shape->callable);
    return Type(this->forge.forgeComplex(ValueFlags::None, { shape }, &precedence, &this->description));
  }

  template<typename T, typename F>
  const T* queryObjectShape(ITypeFactory& factory, const Type& type, F field) {
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
      auto* result = factory.getObjectShape().*field;
      if (result != nullptr) {
        return result;
      }
    }
    if (Bits::hasAnySet(flags, ValueFlags::String)) {
      return factory.getStringShape().*field;
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

  bool isAssignableInstanceObjectShape(const TypeShape* lshape, const TypeShape* rshape) {
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
}

bool TypeShape::equals(const TypeShape& rhs) const {
  auto result = Type::areEquivalent(*this, rhs);
  assert(result == Type::areEquivalent(rhs, *this)); // sanity check
  return result;
}

Type::Assignability Type::queryAssignable(const IType& from) const {
  auto* to = this->get();
  assert(to != nullptr);
  return queryAssignableType(*to, from);
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

const IFunctionSignature* ITypeFactory::queryCallable(const Type& type) {
  return queryObjectShape<IFunctionSignature>(*this, type, &TypeShape::callable);
}

const IPropertySignature* ITypeFactory::queryDotable(const Type& type) {
  return queryObjectShape<IPropertySignature>(*this, type, &TypeShape::dotable);
}

const IIndexSignature* ITypeFactory::queryIndexable(const Type& type) {
  return queryObjectShape<IIndexSignature>(*this, type, &TypeShape::indexable);
}

const IIteratorSignature* ITypeFactory::queryIterable(const Type& type) {
  return queryObjectShape<IIteratorSignature>(*this, type, &TypeShape::iterable);
}

const IPointerSignature* ITypeFactory::queryPointable(const Type& type) {
  return queryObjectShape<IPointerSignature>(*this, type, &TypeShape::pointable);
}

TypeFactory::TypeFactory(IAllocator& allocator)
  : forge(std::make_unique<Forge>(allocator)),
    stringProperties(nullptr) {
}

Type TypeFactory::createSimple(ValueFlags flags) {
  return Type(this->forge->forgeSimple(flags));
}

IAllocator& TypeFactory::getAllocator() const {
  return this->forge->getAllocator();
}

Type TypeFactory::createPointer(const Type& pointee, Modifiability modifiability) {
  assert(pointee != nullptr);
  auto* pointable = this->forge->forgePointerSignature(*pointee, modifiability);
  std::set<const TypeShape*> shapes{ this->forge->forgeTypeShape(nullptr, nullptr, nullptr, nullptr, pointable) };
  auto precedence = Forge::complexToStringPrecedence(ValueFlags::None, shapes);
  auto description = StringBuilder::concat("Pointer of type '", precedence.first, "'");
  return Type(this->forge->forgeComplex(ValueFlags::None, std::move(shapes), &precedence, &description));
}

Type TypeFactory::createArray(const Type& result, Modifiability modifiability) {
  assert(result != nullptr);
  auto* indexable = this->forge->forgeIndexSignature(*result, nullptr, modifiability);
  auto* iterable = this->forge->forgeIteratorSignature(*result);
  std::set<const TypeShape*> shapes{ this->forge->forgeTypeShape(nullptr, nullptr, indexable, iterable, nullptr) };
  auto precedence = Forge::complexToStringPrecedence(ValueFlags::None, shapes);
  auto description = StringBuilder::concat("Array of type '", precedence.first, "'");
  return Type(this->forge->forgeComplex(ValueFlags::None, std::move(shapes), &precedence, &description));
}

Type TypeFactory::createMap(const Type& result, const Type& index, Modifiability modifiability) {
  assert(result != nullptr);
  assert(index != nullptr);
  auto* indexable = this->forge->forgeIndexSignature(*result, index.get(), modifiability);
  auto* iterable = this->forge->forgeIteratorSignature(*result);
  std::set<const TypeShape*> shapes{ this->forge->forgeTypeShape(nullptr, nullptr, indexable, iterable, nullptr) };
  auto precedence = Forge::complexToStringPrecedence(ValueFlags::None, shapes);
  auto description = StringBuilder::concat("Map of type '", precedence.first, "'");
  return Type(this->forge->forgeComplex(ValueFlags::None, std::move(shapes), &precedence, &description));
}

Type TypeFactory::createUnion(const std::vector<Type>& types) {
  switch (types.size()) {
  case 0:
    return nullptr;
  case 1:
    return types[0];
  }
  auto flags = ValueFlags::None;
  std::set<const TypeShape*> shapes;
  for (auto& type : types) {
    if (type != nullptr) {
      flags = flags | type->getPrimitiveFlags();
      this->forge->mergeTypeShapes(shapes, *type);
    }
  }
  return Type(this->forge->forgeComplex(flags, std::move(shapes)));
}

Type TypeFactory::createIterator(const Type& element) {
  assert(!element.hasPrimitiveFlag(ValueFlags::Void));
  auto rettype = this->addVoid(element);
  auto precedence = rettype->toStringPrecedence();
  assert(precedence.second == 2);
  auto builder = this->createFunctionBuilder(rettype, StringBuilder::concat("(", precedence.first, ")()"));
  return builder->build();
}

Type TypeFactory::addVoid(const Type& type) {
  if (type == nullptr) {
    return Type::Void;
  }
  auto flags = Bits::set(type->getPrimitiveFlags(), ValueFlags::Void);
  return Type(this->forge->forgeModified(*type, flags));
}

Type TypeFactory::addNull(const Type& type) {
  if (type == nullptr) {
    return Type::Null;
  }
  auto flags = Bits::set(type->getPrimitiveFlags(), ValueFlags::Null);
  return Type(this->forge->forgeModified(*type, flags));
}

Type TypeFactory::removeVoid(const Type& type) {
  if (type == nullptr) {
    return nullptr;
  }
  auto flags = Bits::clear(type->getPrimitiveFlags(), ValueFlags::Void);
  return Type(this->forge->forgeModified(*type, flags));
}

Type TypeFactory::removeNull(const Type& type) {
  if (type == nullptr) {
    return nullptr;
  }
  auto flags = Bits::clear(type->getPrimitiveFlags(), ValueFlags::Null);
  return Type(this->forge->forgeModified(*type, flags));
}

TypeBuilder TypeFactory::createTypeBuilder(const String& name, const String& description) {
  return TypeBuilder(this->getAllocator().makeRaw<Builder>(*this->forge, name, description));
}

TypeBuilder TypeFactory::createFunctionBuilder(const Type& rettype, const String& name, const String& description) {
  auto builder = this->createTypeBuilder(name, description);
  builder->defineCallable(rettype, nullptr);
  return builder;
}

TypeBuilder TypeFactory::createGeneratorBuilder(const Type& gentype, const String& name, const String& description) {
  // Convert the return type (e.g. 'int') into a generator function 'int...' aka '(void|int)()'
  assert(!gentype.hasPrimitiveFlag(ValueFlags::Void));
  auto rettype = this->addVoid(gentype);
  auto inner = this->createTypeBuilder({});
  inner->defineCallable(rettype, nullptr);
  inner->defineIterable(gentype);
  auto outer = this->createTypeBuilder(name, description);
  outer->defineCallable(inner->build(), gentype);
  return outer;
}

Type TypeFactory::getVanillaArray() {
  // TODO cache
  auto builder = this->createTypeBuilder("any?[]", "Array");
  builder->defineDotable(nullptr, Modifiability::NONE);
  builder->addProperty(Type::Int, "length", ReadWriteMutate);
  builder->defineIndexable(Type::AnyQ, nullptr, ReadWriteMutate);
  builder->defineIterable(Type::AnyQ);
  return builder->build();
}

Type TypeFactory::getVanillaDictionary() {
  // TODO cache
  auto builder = this->createTypeBuilder("any?[string]", "Dictionary");
  builder->defineDotable(Type::AnyQ, ReadWriteMutateDelete);
  builder->defineIndexable(Type::AnyQ, Type::String, ReadWriteMutateDelete);
  builder->defineIterable(this->getVanillaKeyValue());
  return builder->build();
}

Type TypeFactory::getVanillaError() {
  // TODO cache
  auto builder = this->createTypeBuilder("object", "Error");
  builder->defineDotable(Type::AnyQ, ReadOnly);
  builder->addProperty(Type::String, "message", ReadOnly);
  builder->addProperty(Type::String, "file", OptionalRead);
  builder->addProperty(Type::Int, "line", OptionalRead);
  builder->addProperty(Type::Int, "column", OptionalRead);
  builder->defineIndexable(Type::AnyQ, Type::String, ReadOnly);
  builder->defineIterable(this->getVanillaKeyValue());
  return builder->build();
}

Type TypeFactory::getVanillaKeyValue() {
  // TODO cache
  auto builder = this->createTypeBuilder("object", "Key-value pair");
  builder->defineDotable(nullptr, Modifiability::NONE);
  builder->addProperty(Type::String, "key", ReadOnly);
  builder->addProperty(Type::AnyQ, "value", ReadOnly);
  builder->defineIndexable(Type::AnyQ, Type::String, ReadOnly);
  builder->defineIterable(Type::Object); // WIBBLE recursive
  return builder->build();
}

Type TypeFactory::getVanillaPredicate() {
  // TODO cache
  auto builder = this->createFunctionBuilder(Type::Void, "void()", "Predicate");
  return builder->build();
}

bool Type::areEquivalent(const IType& lhs, const IType& rhs) {
  return &lhs == &rhs;
}

bool Type::areEquivalent(const TypeShape& lhs, const TypeShape& rhs) {
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
  return areEquivalent(lhs.getIndexType(), rhs.getIndexType()) && areEquivalent(lhs.getResultType(), rhs.getResultType());
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
const Type Type::None{ &typeNone };
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
