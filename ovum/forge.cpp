#include "ovum/ovum.h"
#include "ovum/forge.h"

namespace {
  using namespace egg::ovum;

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

  struct FunctionSignature : public IFunctionSignature {
    FunctionSignature(const FunctionSignature&) = delete;
    FunctionSignature& operator=(const FunctionSignature&) = delete;
  private:
    String fname;
    const IType& returnType;
    const IType* generatorType;
    std::vector<Forge::Parameter> positional;
    std::map<String, Forge::Parameter> named;
    std::vector<ParameterSignature> signature;
  public:
    FunctionSignature(const IType& returnType, const IType* generatorType, String name, const std::span<Forge::Parameter>& parameters)
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
}

class egg::ovum::Forge::Implementation {
  Implementation(const Implementation&) = delete;
  Implementation& operator=(const Implementation&) = delete;
public:
  IAllocator& allocator;
  ForgeList<TypeShape, TypeShape> shapes;
  ForgeList<FunctionSignature, IFunctionSignature> functions;
  ForgeList<IndexSignature, IIndexSignature> indexes;
  ForgeList<IteratorSignature, IIteratorSignature> iterators;
  ForgeList<PointerSignature, IPointerSignature> pointers;
  ForgeList<PropertySignature, IPropertySignature> properties;

  explicit Implementation(IAllocator& allocator) : allocator(allocator) {
  }
  ~Implementation() {
    this->shapes.destroy(this->allocator);
    this->functions.destroy(this->allocator);
    this->indexes.destroy(this->allocator);
    this->iterators.destroy(this->allocator);
    this->pointers.destroy(this->allocator);
    this->properties.destroy(this->allocator);
  }
  template<typename T, typename... ARGS>
  ForgeablePtr<T> make(ARGS&&... args) {
    // Use perfect forwarding
    return ForgeablePtr(this->allocator, this->allocator.makeRaw<T>(std::forward<ARGS>(args)...));
  }
};

egg::ovum::Forge::Forge(IAllocator& allocator) : implementation(std::make_unique<Implementation>(allocator)) {
}

egg::ovum::Forge::~Forge() {
  // Cannot be in the header file: https://en.cppreference.com/w/cpp/language/pimpl
}

const egg::ovum::IFunctionSignature* egg::ovum::Forge::forgeFunctionSignature(const IType& returnType, const IType* generatorType, String name, const std::span<Parameter>& parameters) {
  auto equals = [&](const FunctionSignature& candidate) {
    return candidate.equals(returnType, generatorType, name, parameters);
  };
  auto create = [&]() {
    return this->implementation->make<FunctionSignature>(returnType, generatorType, name, parameters);
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