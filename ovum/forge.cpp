#include "ovum/ovum.h"
#include "ovum/forge.h"
#include "ovum/function.h"

namespace {
  using namespace egg::ovum;

  const char* simpleComponent(ValueFlags flags) {
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

  std::pair<std::string, int> complexComponentObject(const TypeShape* shape) {
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
    if (shape->indexable != nullptr) {
      auto resultType = shape->indexable->getResultType();
      auto indexee = resultType->toStringPrecedence();
      auto result = indexee.first;
      if (indexee.second > 1) {
        result = '(' + result + ')';
      }
      auto indexType = shape->indexable->getIndexType();
      if (indexType == nullptr) {
        return { result + '[' + ']', 1 };
      }
      return { result + '[' + indexType->toStringPrecedence().first + ']', 1 };
    }
    return { "<complex>", 0 }; // TODO
  }

  template<typename T>
  class ForgeablePtr {
    ForgeablePtr(const ForgeablePtr&) = delete;
    ForgeablePtr& operator=(const ForgeablePtr&) = delete;
  private:
    IAllocator& allocator;
    T* ptr;
  public:
    ForgeablePtr(IAllocator& allocator, T* rhs) : allocator(allocator), ptr(rhs) {
    }
    ~ForgeablePtr() {
      if (this->ptr != nullptr) {
        this->allocator.destroy(this->ptr);
      }
    }
    T* get() {
      return this->ptr;
    }
    T* release() {
      T* released = this->ptr;
      this->ptr = nullptr;
      return released;
    }
  };

  template<typename S, typename T>
  class ForgeList final {
    ForgeList(const ForgeList&) = delete;
    ForgeList& operator=(const ForgeList&) = delete;
  private:
    std::vector<const S*> items;
  public:
    typedef std::function<bool(const S&)> Equals;
    typedef std::function<ForgeablePtr<S>()> Create;
    typedef std::function<void(const S*)> Destroy;
    ForgeList() {}
    size_t size() const {
      return this->items.size();
    }
    const T* get(size_t index) const {
      if (index < this->items.size()) {
        return this->items[index];
      }
      return nullptr;
    }
    const T* find(const T& item, Equals equals, size_t& index) const {
      index = 0;
      for (const auto& candidate : this->items) {
        if (equals(candidate)) {
          return &candidate;
        }
        ++index;
      }
      return nullptr;
    }
    const T* findOrAdd(Equals equals, Create create) {
      size_t index = 0;
      for (const auto* candidate : this->items) {
        if (equals(*candidate)) {
          return candidate;
        }
        ++index;
      }
      auto created = create();
      this->items.emplace_back(created.get());
      return created.release();
    }
    void destroy(IAllocator& allocator) {
      for (const auto* candidate : this->items) {
        allocator.destroy(candidate);
      }
      this->items.clear();
    }
  };

  struct ParameterSignature : public IFunctionSignatureParameter {
  private:
    const IType* type;
    String pname;
    size_t position;
    Flags flags;
  public:
    ParameterSignature(const IType& type, String name, size_t position, Flags flags)
      : type(&type), pname(name), position(position), flags(flags) {
    }
    virtual String getName() const override {
      return this->pname;
    }
    virtual Type getType() const override {
      return Type(this->type);
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
    bool equals(const IType& type2, const String& pname2, size_t position2, bool optional2, Forge::Parameter::Kind kind2) const {
      return Type::areEquivalent(*this->type, type2) &&
             this->pname == pname2 &&
             this->position == position2 &&
             this->flags == computeFlags(optional2, kind2);
    }
    static IFunctionSignatureParameter::Flags computeFlags(bool optional, Forge::Parameter::Kind kind) {
      auto flags = IFunctionSignatureParameter::Flags::None;
      switch (kind) {
      case Forge::Parameter::Kind::Positional:
      case Forge::Parameter::Kind::Named:
        break;
      case Forge::Parameter::Kind::Variadic:
        flags = IFunctionSignatureParameter::Flags::Variadic;
        break;
      case Forge::Parameter::Kind::Predicate:
        flags = IFunctionSignatureParameter::Flags::Predicate;
        break;
      }
      if (!optional) {
        flags = Bits::set(flags, IFunctionSignatureParameter::Flags::Required);
      }
      return flags;
    }
  };

  struct CallableSignature : public IFunctionSignature {
    CallableSignature(const CallableSignature&) = delete;
    CallableSignature& operator=(const CallableSignature&) = delete;
  private:
    String fname;
    const IType& returnType;
    const IType* generatorType;
    std::vector<Forge::Parameter> positional;
    std::map<String, Forge::Parameter> named;
    std::vector<ParameterSignature> signature;
  public:
    CallableSignature(const IType& returnType, const IType* generatorType, String name, const std::span<Forge::Parameter>& parameters)
      : fname(name), returnType(returnType), generatorType(generatorType) {
      for (const auto& parameter : parameters) {
        if (parameter.kind == Forge::Parameter::Kind::Named) {
          assert(!parameter.name.empty());
          this->named[parameter.name] = parameter;
        } else {
          this->positional.push_back(parameter);
        }
      }
      assert((this->positional.size() + this->named.size()) == parameters.size());
      size_t position = 0;
      for (const auto& p : this->positional) {
        this->signature.emplace_back(*p.type, p.name, position++, computeFlags(p));
      }
      for (const auto& kv : this->named) {
        this->signature.emplace_back(*kv.second.type, kv.first, SIZE_MAX, computeFlags(kv.second));
      }
      assert(this->signature.size() == parameters.size());
    }
    virtual String getName() const override {
      return this->fname;
    }
    virtual Type getReturnType() const override {
      return Type(&this->returnType);
    }
    virtual Type getGeneratorType() const override {
      return Type(this->generatorType);
    }
    virtual size_t getParameterCount() const override {
      return this->signature.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      return this->signature.at(index);
    }
    bool equals(const IType& returnType2, const IType* generatorType2, const String& fname2, const std::span<Forge::Parameter>& parameters2) const {
      if (!Type::areEquivalent(this->returnType, returnType2) ||
          !Type::areEquivalent(this->generatorType, generatorType2) ||
          this->fname != fname2 ||
          this->signature.size() != parameters2.size()) {
        return false;
      }
      size_t index = 0;
      for (const auto& p2 : parameters2) {
        if (!this->signature[index].equals(*p2.type, p2.name, index, p2.optional, p2.kind)) {
          return false;
        }
      }
      return true;
    }
    static IFunctionSignatureParameter::Flags computeFlags(const Forge::Parameter& parameter) {
      return ParameterSignature::computeFlags(parameter.optional, parameter.kind);
    }
  };

  struct IndexSignature : public IIndexSignature {
    IndexSignature(const IndexSignature&) = delete;
    IndexSignature& operator=(const IndexSignature&) = delete;
  private:
    const IType& resultType;
    const IType* indexType;
    Modifiability modifiability;
  public:
    IndexSignature(const IType& resultType, const IType* indexType, Modifiability modifiability)
      : resultType(resultType), indexType(indexType), modifiability(modifiability) {
    }
    virtual Type getResultType() const override {
      return Type(&this->resultType);
    }
    virtual Type getIndexType() const override {
      return Type(this->indexType);
    }
    virtual Modifiability getModifiability() const override {
      return this->modifiability;
    }
    bool equals(const IType& resultType2, const IType* indexType2, Modifiability modifiability2) const {
      return Type::areEquivalent(this->resultType, resultType2) &&
             Type::areEquivalent(this->indexType, indexType2) &&
             this->modifiability == modifiability2;
    }
  };

  struct IteratorSignature : public IIteratorSignature {
    IteratorSignature(const IteratorSignature&) = delete;
    IteratorSignature& operator=(const IteratorSignature&) = delete;
  private:
    const IType& resultType;
  public:
    explicit IteratorSignature(const IType& resultType)
      : resultType(resultType) {
    }
    virtual Type getType() const override {
      return Type(&this->resultType);
    }
    bool equals(const IType& resultType2) const {
      return Type::areEquivalent(this->resultType, resultType2);
    }
  };

  struct PointerSignature : public IPointerSignature {
    PointerSignature(const PointerSignature&) = delete;
    PointerSignature& operator=(const PointerSignature&) = delete;
  private:
    const IType& pointeeType;
    Modifiability modifiability;
  public:
    PointerSignature(const IType& pointeeType, Modifiability modifiability)
      : pointeeType(pointeeType), modifiability(modifiability) {
    }
    virtual Type getType() const override {
      return Type(&this->pointeeType);
    }
    virtual Modifiability getModifiability() const override {
      return this->modifiability;
    }
    bool equals(const IType& pointeeType2, Modifiability modifiability2) const {
      return Type::areEquivalent(this->pointeeType, pointeeType2) &&
             this->modifiability == modifiability2;
    }
  };

  struct PropertySignature : public IPropertySignature {
    PropertySignature(const PropertySignature&) = delete;
    PropertySignature& operator=(const PropertySignature&) = delete;
  private:
    struct Detail {
      const IType* type;
      Modifiability modifiability;
      Detail(const IType* type, Modifiability modifiability)
        : type(type), modifiability(modifiability) {
        assert((this->type != nullptr) || (this->modifiability == Modifiability::None));
        assert((this->modifiability != Modifiability::None) || (this->type == nullptr));
      }
      bool equals(const IType* type2, Modifiability modifiability2) const {
        return Type::areEquivalent(this->type, type2) &&
               this->modifiability == modifiability2;
      }
    };
    std::map<String, Detail> names;
    Detail unknown;
  public:
    PropertySignature(const std::span<Forge::Property>& properties, const IType* unknownType, Modifiability unknownModifiability)
      : unknown(unknownType, unknownModifiability) {
      for (const auto& property : properties) {
        Detail detail(property.type.get(), property.modifiability);
        this->names.emplace(property.name, std::move(detail));
      }
    }
    virtual Type getType(const String& property) const override {
      return Type(this->findDetail(property).type);
    }
    virtual Modifiability getModifiability(const String& property) const override {
      return this->findDetail(property).modifiability;
    }
    virtual String getName(size_t index) const override {
      auto iterator = std::begin(this->names);
      std::advance(iterator, index);
      return iterator->first;
    }
    virtual size_t getNameCount() const override {
      return this->names.size();
    }
    virtual bool isClosed() const override {
      return this->unknown.type == nullptr;
    }
    bool equals(const std::span<Forge::Property>& properties, const IType* unknownType, Modifiability unknownModifiability) const {
      if (this->names.size() != properties.size() ||
          !Type::areEquivalent(this->unknown.type, unknownType) ||
          this->unknown.modifiability != unknownModifiability) {
        return false;
      }
      for (const auto& property : properties) {
        auto found = this->names.find(property.name);
        if (found == std::end(this->names) || !found->second.equals(property.type.get(), property.modifiability)) {
          return false;
        }
      }
      return true;
    }
  private:
    const Detail& findDetail(const String& property) const {
      auto found = this->names.find(property);
      if (found == std::end(this->names)) {
        return this->unknown;
      }
      return found->second;
    }
  };

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
    virtual const TypeShape* getObjectShape(size_t) const override {
      return nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 0;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return Forge::simpleToStringPrecedence(this->flags);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  class TypeComplex final : public HardReferenceCounted<IType> {
    TypeComplex(const TypeComplex&) = delete;
    TypeComplex& operator=(const TypeComplex&) = delete;
  private:
    ValueFlags flags;
    std::set<const TypeShape*> shapes;
    const char* description;
  public:
    TypeComplex(IAllocator& allocator, ValueFlags flags, std::set<const TypeShape*>&& shapes, const char* description)
      : HardReferenceCounted(allocator, 0),
      flags(flags),
      shapes(std::move(shapes)),
      description(description) {
      assert(!this->shapes.empty());
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return this->flags;
    }
    virtual const TypeShape* getObjectShape(size_t index) const override {
      auto iterator = std::begin(this->shapes);
      std::advance(iterator, index);
      return *iterator;
    }
    virtual size_t getObjectShapeCount() const override {
      return this->shapes.size();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return Forge::complexToStringPrecedence(this->flags, this->shapes);
    }
    virtual String describeValue() const override {
      // TODO i18n
      StringBuilder sb;
      auto p = (this->description == nullptr) ? "Value of type '$'" : this->description;
      auto q = std::strchr(p, '$');
      if (q == nullptr) {
        return String(p);
      }
      std::string prefix{ p, q };
      std::string suffix{ q + 1 };
      return StringBuilder::concat(prefix, this->toStringPrecedence().first, suffix);
    }
    bool equals(ValueFlags flags2, const std::set<const TypeShape*>& shapes2) const {
      return this->flags == flags2 && this->shapes == shapes2;
    }
  };
}

std::pair<std::string, int> egg::ovum::Forge::simpleToStringPrecedence(ValueFlags flags) {
  auto* component = simpleComponent(flags);
  if (component != nullptr) {
    return { component, 0 };
  }
  if (Bits::hasAnySet(flags, ValueFlags::Null)) {
    return { simpleToStringPrecedence(Bits::clear(flags, ValueFlags::Null)).first + "?", 1 };
  }
  auto head = Bits::topmost(flags);
  assert(head != ValueFlags::None);
  component = simpleComponent(head);
  assert(component != nullptr);
  return { simpleToStringPrecedence(Bits::clear(flags, head)).first + '|' + component, 2 };
}

std::pair<std::string, int> egg::ovum::Forge::complexToStringPrecedence(ValueFlags flags, const TypeShape& shape) {
  std::pair<std::string, int> result;
  if (flags != ValueFlags::None) {
    result = Forge::simpleToStringPrecedence(flags);
  }
  auto last = complexComponentObject(&shape);
  assert(!last.first.empty());
  assert((last.second >= 0) && (last.second <= 2));
  if (result.first.empty()) {
    result = last;
  } else {
    result.first += '|' + last.first;
    result.second = 2;
  }
  return result;
}

std::pair<std::string, int> egg::ovum::Forge::complexToStringPrecedence(ValueFlags flags, const std::set<const TypeShape*>& shapes) {
  if (shapes.empty()) {
    // Primitive types only
    return Forge::simpleToStringPrecedence(flags);
  }
  std::pair<std::string, int> result;
  if (flags!= ValueFlags::None) {
    result = Forge::simpleToStringPrecedence(flags);
  }
  std::set<std::string> parts;
  for (auto* shape : shapes) {
    auto next = complexComponentObject(shape);
    assert(!next.first.empty());
    assert((next.second >= 0) && (next.second <= 2));
    if (parts.empty() && result.first.empty()) {
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

class egg::ovum::Forge::Implementation {
  Implementation(const Implementation&) = delete;
  Implementation& operator=(const Implementation&) = delete;
private:
  const size_t NSIMPLES = size_t(ValueFlags::Object) << 1;
public:
  IAllocator& allocator;
  ForgeList<TypeShape, TypeShape> shapes;
  ForgeList<CallableSignature, IFunctionSignature> functions;
  ForgeList<IndexSignature, IIndexSignature> indexes;
  ForgeList<IteratorSignature, IIteratorSignature> iterators;
  ForgeList<PointerSignature, IPointerSignature> pointers;
  ForgeList<PropertySignature, IPropertySignature> properties;
  std::vector<HardPtr<const IType>> simples;
  std::vector<HardPtr<const TypeComplex>> complexes;

  explicit Implementation(IAllocator& allocator) : allocator(allocator) {
    this->simples.resize(NSIMPLES);
  }
  ~Implementation() {
    this->shapes.destroy(this->allocator);
    this->functions.destroy(this->allocator);
    this->indexes.destroy(this->allocator);
    this->iterators.destroy(this->allocator);
    this->pointers.destroy(this->allocator);
    this->properties.destroy(this->allocator);
  }
  void addSimple(const IType& simple) {
    // Used only during construction, so no need for locks
    auto index = size_t(simple.getPrimitiveFlags());
    assert(index < NSIMPLES);
    assert(this->simples.at(index) == nullptr);
    this->simples[index].set(&simple);
  }
  template<typename T, typename... ARGS>
  ForgeablePtr<T> make(ARGS&&... args) {
    // Use perfect forwarding
    return ForgeablePtr(this->allocator, this->allocator.makeRaw<T>(std::forward<ARGS>(args)...));
  }
};

egg::ovum::Forge::Forge(IAllocator& allocator) : implementation(std::make_unique<Implementation>(allocator)) {
  implementation->addSimple(*Type::None);
  implementation->addSimple(*Type::Void);
  implementation->addSimple(*Type::Null);
  implementation->addSimple(*Type::Bool);
  implementation->addSimple(*Type::Int);
  implementation->addSimple(*Type::Float);
  implementation->addSimple(*Type::Arithmetic);
  implementation->addSimple(*Type::String);
  implementation->addSimple(*Type::Object);
  implementation->addSimple(*Type::Any);
  implementation->addSimple(*Type::AnyQ);
}

egg::ovum::Forge::~Forge() {
  // Cannot be in the header file: https://en.cppreference.com/w/cpp/language/pimpl
}

IAllocator& egg::ovum::Forge::getAllocator() const {
  return this->implementation->allocator;
}

const egg::ovum::IFunctionSignature* egg::ovum::Forge::forgeFunctionSignature(const IType& returnType, const IType* generatorType, String name, const std::span<Parameter>& parameters) {
  auto equals = [&](const CallableSignature& candidate) {
    return candidate.equals(returnType, generatorType, name, parameters);
  };
  auto create = [&]() {
    return this->implementation->make<CallableSignature>(returnType, generatorType, name, parameters);
  };
  return this->implementation->functions.findOrAdd(equals, create);
}

const egg::ovum::IIndexSignature* egg::ovum::Forge::forgeIndexSignature(const IType& resultType, const IType* indexType, Modifiability modifiability) {
  auto equals = [&](const IndexSignature& candidate) {
    return candidate.equals(resultType, indexType, modifiability);
  };
  auto create = [&]() {
    return this->implementation->make<IndexSignature>(resultType, indexType, modifiability);
  };
  return this->implementation->indexes.findOrAdd(equals, create);
}

const egg::ovum::IIteratorSignature* egg::ovum::Forge::forgeIteratorSignature(const IType& resultType) {
  auto equals = [&](const IteratorSignature& candidate) {
    return candidate.equals(resultType);
  };
  auto create = [&]() {
    return this->implementation->make<IteratorSignature>(resultType);
  };
  return this->implementation->iterators.findOrAdd(equals, create);
}

const egg::ovum::IPointerSignature* egg::ovum::Forge::forgePointerSignature(const IType& pointeeType, Modifiability modifiability) {
  auto equals = [&](const PointerSignature& candidate) {
    return candidate.equals(pointeeType, modifiability);
  };
  auto create = [&]() {
    return this->implementation->make<PointerSignature>(pointeeType, modifiability);
  };
  return this->implementation->pointers.findOrAdd(equals, create);
}

const egg::ovum::IPropertySignature* egg::ovum::Forge::forgePropertySignature(const std::span<Property>& properties, const IType* unknownType, Modifiability unknownModifiability) {
  auto equals = [&](const PropertySignature& candidate) {
    return candidate.equals(properties, unknownType, unknownModifiability);
  };
  auto create = [&]() {
    return this->implementation->make<PropertySignature>(properties, unknownType, unknownModifiability);
  };
  return this->implementation->properties.findOrAdd(equals, create);
}

const egg::ovum::TypeShape* egg::ovum::Forge::forgeTypeShape(const IFunctionSignature* callable, const IPropertySignature* dotable, const IIndexSignature* indexable, const IIteratorSignature* iterable, const IPointerSignature* pointable) {
  auto equals = [&](const TypeShape& candidate) {
    return candidate.callable == callable &&
           candidate.dotable == dotable &&
           candidate.indexable == indexable &&
           candidate.iterable == iterable &&
           candidate.pointable == pointable;
  };
  auto create = [&]() {
    return this->implementation->make<TypeShape>(callable, dotable, indexable, iterable, pointable);
  };
  return this->implementation->shapes.findOrAdd(equals, create);
}

const egg::ovum::IType* egg::ovum::Forge::forgeSimple(egg::ovum::ValueFlags simple) {
  auto& ref = this->implementation->simples.at(size_t(simple));
  if (ref == nullptr) {
    // TODO lock
    ref = this->implementation->allocator.makeHard<TypeSimple, Type>(simple);
  }
  return ref.get();
}

const egg::ovum::IType* egg::ovum::Forge::forgeComplex(ValueFlags simple, std::set<const TypeShape*>&& complex, const char* description) {
  if (complex.empty()) {
    return this->forgeSimple(simple);
  }
  // TODO lock
  for (const auto& candidate : this->implementation->complexes) {
    if (candidate->equals(simple, complex)) {
      return candidate.get();
    }
  }
  auto created = this->implementation->allocator.makeHard<TypeComplex>(simple, std::move(complex), description);
  this->implementation->complexes.emplace_back(created.get());
  return created.get();
}

void egg::ovum::Forge::mergeTypeShapes(std::set<const TypeShape*>& shapes, const IType& other) {
  auto count = other.getObjectShapeCount();
  for (size_t index = 0; index < count; ++index) {
    auto incoming = other.getObjectShape(index);
    assert(incoming != nullptr);
    auto* shape = this->forgeTypeShape(incoming->callable, incoming->dotable, incoming->indexable, incoming->iterable, incoming->pointable);
    shapes.insert(shape);
  }
}
