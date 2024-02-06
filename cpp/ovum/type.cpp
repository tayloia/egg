#include "ovum/ovum.h"

#include <unordered_set>

namespace egg::internal {
  using namespace egg::ovum;

  Assignability assignabilityIntersection(Assignability a, Assignability b) {
    // Never * Never = Never
    // Never * Sometimes = Never
    // Never * Always = Never
    // Sometimes * Never = Never
    // Sometimes * Sometimes = Sometimes
    // Sometimes * Always = Sometimes
    // Always * Never = Never
    // Always * Sometimes = Sometimes
    // Always * Always = Always
    if ((a == Assignability::Never) || (b == Assignability::Never)) {
      return Assignability::Never;
    }
    if ((a == Assignability::Always) && (b == Assignability::Always)) {
      return Assignability::Always;
    }
    return Assignability::Sometimes;
  }

  Assignability assignabilityUnion(Assignability a, Assignability b) {
    // Never + Never = Never
    // Never + Sometimes = Sometimes
    // Never + Always = Sometimes
    // Sometimes + Never = Sometimes
    // Sometimes + Sometimes = Sometimes
    // Sometimes + Always = Sometimes
    // Always + Never = Sometimes
    // Always + Sometimes = Sometimes
    // Always + Always = Always
    if (a == b) {
      return a;
    }
    return Assignability::Sometimes;
  }

  Assignability assignabilityFromModifiability(Modifiability dst, Modifiability src) {
    auto d = Bits::underlying(dst);
    auto s = Bits::underlying(src);
    if ((d & s) == s) {
      // All source bits are supported by destination
      return Assignability::Always;
    }
    if ((d & s) == 0) {
      // No source bits are supported by destination
      return Assignability::Never;
    }
    return Assignability::Sometimes;
  }

  class TypeForgeDefault;

  template<typename T>
  class TypeForgeCacheSet {
    TypeForgeCacheSet(const TypeForgeCacheSet&) = delete;
    TypeForgeCacheSet& operator=(const TypeForgeCacheSet&) = delete;
  private:
    struct Hash {
      size_t operator()(const T& instance) const {
        return instance.cacheHash();
      }
    };
    struct Equals {
      bool operator()(const T& lhs, const T& rhs) const {
        return T::cacheEquals(lhs, rhs);
      }
    };
    std::unordered_set<T, Hash, Equals> cache;
    std::mutex mutex;
  public:
    TypeForgeCacheSet() {}
    const T& fetch(T&& value) {
      std::lock_guard<std::mutex> lock{ this->mutex };
      return *this->cache.emplace(std::move(value)).first;
    }
  };

  template<typename K, typename V>
  class TypeForgeCacheMap {
    TypeForgeCacheMap(const TypeForgeCacheMap&) = delete;
    TypeForgeCacheMap& operator=(const TypeForgeCacheMap&) = delete;
  private:
    struct Hash {
      size_t operator()(const K* key) const {
        return key->cacheHash();
      }
    };
    struct Equals {
      bool operator()(const K* lhs, const K* rhs) const {
        return K::cacheEquals(*lhs, *rhs);
      }
    };
    std::unordered_map<const K*, V*, Hash, Equals> cache;
    std::mutex mutex;
  public:
    TypeForgeCacheMap() {}
    const V& fetch(K&& key, const std::function<V*(K&&)>& factory) {
      std::lock_guard<std::mutex> lock{ this->mutex };
      auto found = this->cache.find(&key);
      if (found != this->cache.end()) {
        return *found->second;
      }
      auto* value = factory(std::move(key));
      return *this->cache.emplace_hint(found, value->cacheKey(), value)->second;
    }
  };

  class TypeForgePrimitive final : public SoftReferenceCountedNone<IType> {
    TypeForgePrimitive(const TypeForgePrimitive&) = delete;
    TypeForgePrimitive& operator=(const TypeForgePrimitive&) = delete;
  private:
    static const size_t trivials = size_t(1 << int(ValueFlagsShift::UBound));
    static TypeForgePrimitive trivial[trivials];
    Atomic<ValueFlags> flags;
    TypeForgePrimitive()
      : flags(ValueFlags::None) {
      // Private construction used for TypeForgePrimitive::trivial
    }
  public:
    virtual bool isPrimitive() const override {
      return true;
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return this->flags.get();
    }
    virtual size_t getShapeCount() const override {
      return 0;
    }
    virtual const Shape* getShape(size_t) const override {
      return nullptr;
    }
    virtual int print(Printer& printer) const override {
      return Type::print(printer, this->flags.get());
    }
    static const IType* forge(ValueFlags flags) {
      auto index = size_t(flags);
      if (index < TypeForgePrimitive::trivials) {
        auto& entry = TypeForgePrimitive::trivial[index];
        (void)entry.flags.exchange(flags);
        return &entry;
      }
      return nullptr;
    }
  };
  TypeForgePrimitive TypeForgePrimitive::trivial[] = {};

  class TypeForgeComplex final : public SoftReferenceCountedAllocator<IType> {
    TypeForgeComplex(const TypeForgeComplex&) = delete;
    TypeForgeComplex& operator=(const TypeForgeComplex&) = delete;
  public:
    struct Detail {
      ValueFlags flags;
      std::vector<const IType::Shape*> shapes;
      Detail(ValueFlags flags, const TypeShapeSet& shapeset)
        : flags(flags) {
        assert(!shapeset.empty());
        this->shapes.reserve(shapeset.size());
        for (const auto& shape : shapeset) {
          this->shapes.emplace_back(&shape.get());
        }
      }
      bool validate() const {
        return !this->shapes.empty() && Bits::hasNoneSet(this->flags, ValueFlags::Object);
      }
      int print(Printer& printer) const {
        int complexPrecedence = -1;
        for (const auto& shape : this->shapes) {
          complexPrecedence = Type::print(printer, *shape);
        }
        if (this->shapes.size() > 1) {
          complexPrecedence = 2;
        }
        return Type::print(printer, this->flags, complexPrecedence);
      }
      size_t cacheHash() const {
        Hash hash;
        return hash.add(this->flags).addFrom(this->shapes.begin(), this->shapes.end());
      }
      static bool cacheEquals(const Detail& lhs, const Detail& rhs) {
        if (lhs.shapes.size() == rhs.shapes.size()) {
          if (std::equal(lhs.shapes.begin(), lhs.shapes.end(), rhs.shapes.begin())) {
            return lhs.flags == rhs.flags;
          }
        }
        return false;
      }
    };
  private:
    Detail detail;
  public:
    TypeForgeComplex(IAllocator& allocator, ValueFlags flags, const TypeShapeSet& shapeset)
      : SoftReferenceCountedAllocator(allocator),
        detail(flags, shapeset) {
    }
    TypeForgeComplex(IAllocator& allocator, TypeForgeComplex::Detail&& detail) noexcept
      : SoftReferenceCountedAllocator(allocator),
        detail(std::move(detail)) {
    }
    virtual bool validate() const override {
      return SoftReferenceCounted::validate() && this->detail.validate();
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // Nothing to do
    }
    virtual bool isPrimitive() const override {
      return false;
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return this->detail.flags;
    }
    virtual size_t getShapeCount() const override {
      return this->detail.shapes.size();
    }
    virtual const Shape* getShape(size_t index) const override {
      return (index < this->detail.shapes.size()) ? this->detail.shapes[index] : nullptr;
    }
    virtual int print(Printer& printer) const override {
      return this->detail.print(printer);
    }
    const Detail* cacheKey() const {
      return &this->detail;
    }
    size_t cacheHash() const {
      return this->detail.cacheHash();
    }
    static bool cacheEquals(const TypeForgeComplex& lhs, const TypeForgeComplex& rhs) {
      return Detail::cacheEquals(lhs.detail, rhs.detail);
    }
  };

  class TypeForgeShape : public IType::Shape {
    TypeForgeShape(const TypeForgeShape&) = delete;
    TypeForgeShape& operator=(const TypeForgeShape&) = delete;
  public:
    TypeForgeShape()
      : IType::Shape() {
    }
    TypeForgeShape(TypeForgeShape&& rhs) noexcept
      : IType::Shape(std::move(rhs)) {
    }
    size_t cacheHash() const {
      return Hash::combine(this->callable, this->dotable, this->indexable, this->iterable, this->pointable, this->taggable);
    }
    static bool cacheEquals(const TypeForgeShape& lhs, const TypeForgeShape& rhs) {
      return (lhs.callable == rhs.callable)
          && (lhs.dotable == rhs.dotable)
          && (lhs.indexable == rhs.indexable)
          && (lhs.iterable == rhs.iterable)
          && (lhs.pointable == rhs.pointable)
          && (lhs.taggable == rhs.taggable);
    }
  };

  class TypeForgeFunctionSignatureParameter : public IFunctionSignatureParameter {
    TypeForgeFunctionSignatureParameter(const TypeForgeFunctionSignatureParameter&) = delete;
    TypeForgeFunctionSignatureParameter& operator=(const TypeForgeFunctionSignatureParameter&) = delete;
  public:
    size_t position;
    Type type;
    String name;
    Flags flags;
    TypeForgeFunctionSignatureParameter(size_t position, const Type& type, const String& name, Flags flags)
      : position(position),
        type(type),
        name(name),
        flags(flags) {
    }
    TypeForgeFunctionSignatureParameter(TypeForgeFunctionSignatureParameter&& rhs) noexcept
      : position(std::move(rhs.position)),
        type(std::move(rhs.type)),
        name(std::move(rhs.name)),
        flags(std::move(rhs.flags)) {
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Type getType() const override {
      return this->type;
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
    size_t cacheHash() const {
      return Hash::combine(this->position, this->type, this->name, this->flags);
    }
    static bool cacheEquals(const TypeForgeFunctionSignatureParameter& lhs, const TypeForgeFunctionSignatureParameter& rhs) {
      return (lhs.position == rhs.position) && (lhs.type == rhs.type) && (lhs.name == rhs.name) && (lhs.flags == rhs.flags);
    }
  };

  class TypeForgeFunctionSignature : public IFunctionSignature {
    TypeForgeFunctionSignature(const TypeForgeFunctionSignature&) = delete;
    TypeForgeFunctionSignature& operator=(const TypeForgeFunctionSignature&) = delete;
  public:
    Type rtype;
    Type gtype;
    String name;
    std::vector<const IFunctionSignatureParameter*> parameters;
    TypeForgeFunctionSignature(const Type& rtype, const Type& gtype, const String& name)
      : rtype(rtype),
        gtype(gtype),
        name(name),
        parameters() {
    }
    TypeForgeFunctionSignature(TypeForgeFunctionSignature&& rhs) noexcept
      : rtype(std::move(rhs.rtype)),
        gtype(std::move(rhs.gtype)),
        name(std::move(rhs.name)),
        parameters(std::move(rhs.parameters)) {
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual Type getGeneratedType() const override {
      return this->gtype;
    }
    virtual Type getReturnType() const override {
      return this->rtype;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      return *this->parameters[index];
    }
    size_t cacheHash() const {
      Hash hash;
      hash.add(this->rtype, this->gtype, this->name).addFrom(this->parameters.begin(), this->parameters.end());
      return hash;
    }
    static bool cacheEquals(const TypeForgeFunctionSignature& lhs, const TypeForgeFunctionSignature& rhs) {
      return (lhs.rtype == rhs.rtype) && (lhs.gtype == rhs.gtype) && (lhs.name == rhs.name) && (lhs.parameters == rhs.parameters);
    }
  };

  class TypeForgePropertySignature : public IPropertySignature {
    TypeForgePropertySignature(const TypeForgePropertySignature&) = delete;
    TypeForgePropertySignature& operator=(const TypeForgePropertySignature&) = delete;
  public:
    struct Entry {
      Type type = nullptr;
      Modifiability modifiability = Modifiability::None;
      size_t hash() const {
        return Hash::combine(this->type.get(), this->modifiability);
      }
      bool operator==(const Entry& rhs) const {
        return (this->type == rhs.type) && (this->modifiability == rhs.modifiability);
      }
    };
    std::map<String, Entry> entries;
    Entry unknown;
    TypeForgePropertySignature() {
    }
    TypeForgePropertySignature(TypeForgePropertySignature&& rhs) noexcept
      : entries(std::move(rhs.entries)),
        unknown(std::move(rhs.unknown)) {
    }
    virtual Type getType(const String& property) const override {
      return this->findByName(property).type;
    }
    virtual Modifiability getModifiability(const String& property) const override {
      return this->findByName(property).modifiability;
    }
    virtual String getName(size_t index) const override {
      return this->findByIndex(index);
    }
    virtual size_t getNameCount() const override {
      return this->entries.size();
    }
    virtual bool isClosed() const override {
      return this->unknown.type == nullptr;
    }
    void setUnknown(const Type& type, Modifiability modifiability) {
      this->unknown.type = type;
      this->unknown.modifiability = modifiability;
    }
    bool addProperty(const String& name, const Type& type, Modifiability modifiability) {
      return this->entries.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple(type, modifiability)).second;
    }
    const Entry& findByName(const String& name) const {
      auto found = this->entries.find(name);
      if (found == this->entries.end()) {
        return this->unknown;
      }
      return found->second;
    }
    String findByIndex(size_t index) const {
      // TODO optimize
      if (index < this->entries.size()) {
        auto entry = this->entries.begin();
        std::advance(entry, index);
        return entry->first;
      }
      return {};
    }
    size_t cacheHash() const {
      Hash hash;
      for (const auto& entry : this->entries) {
        hash.add(entry.first, entry.second);
      }
      return hash.add(this->unknown);
    }
    static bool cacheEquals(const TypeForgePropertySignature& lhs, const TypeForgePropertySignature& rhs) {
      if (lhs.entries.size() == rhs.entries.size()) {
        if (std::equal(lhs.entries.begin(), lhs.entries.end(), rhs.entries.begin())) {
          return lhs.unknown == rhs.unknown;
        }
      }
      return false;
    }
  };

  class TypeForgeIndexSignature : public IIndexSignature {
    TypeForgeIndexSignature(const TypeForgeIndexSignature&) = delete;
    TypeForgeIndexSignature& operator=(const TypeForgeIndexSignature&) = delete;
  public:
    Type resultType = nullptr;
    Type indexType = nullptr;
    Modifiability modifiability = Modifiability::ReadWriteMutate;
    TypeForgeIndexSignature() {
    }
    TypeForgeIndexSignature(TypeForgeIndexSignature&& rhs) noexcept
      : resultType(std::move(rhs.resultType)),
        indexType(std::move(rhs.indexType)),
        modifiability(std::move(rhs.modifiability)) {
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
    size_t cacheHash() const {
      return Hash::combine(this->resultType, this->indexType, this->modifiability);
    }
    static bool cacheEquals(const TypeForgeIndexSignature& lhs, const TypeForgeIndexSignature& rhs) {
      return (lhs.resultType == rhs.resultType) && (lhs.indexType == rhs.indexType) && (lhs.modifiability == rhs.modifiability);
    }
  };

  class TypeForgeIteratorSignature : public IIteratorSignature {
    TypeForgeIteratorSignature(const TypeForgeIteratorSignature&) = delete;
    TypeForgeIteratorSignature& operator=(const TypeForgeIteratorSignature&) = delete;
  public:
    Type interationType = nullptr;
    TypeForgeIteratorSignature() {
    }
    TypeForgeIteratorSignature(TypeForgeIteratorSignature&& rhs) noexcept
      : interationType(std::move(rhs.interationType)) {
    }
    virtual Type getIterationType() const override {
      return this->interationType;
    }
    size_t cacheHash() const {
      return Hash::combine(this->interationType);
    }
    static bool cacheEquals(const TypeForgeIteratorSignature& lhs, const TypeForgeIteratorSignature& rhs) {
      return lhs.interationType == rhs.interationType;
    }
  };

  class TypeForgePointerSignature : public IPointerSignature {
    TypeForgePointerSignature(const TypeForgePointerSignature&) = delete;
    TypeForgePointerSignature& operator=(const TypeForgePointerSignature&) = delete;
  public:
    Type pointeeType = nullptr;
    Modifiability modifiability = Modifiability::ReadWriteMutate;
    TypeForgePointerSignature() {
    }
    TypeForgePointerSignature(TypeForgePointerSignature&& rhs) noexcept
      : pointeeType(std::move(rhs.pointeeType)),
        modifiability(std::move(rhs.modifiability)) {
    }
    virtual Type getPointeeType() const override {
      return this->pointeeType;
    }
    virtual Modifiability getModifiability() const override {
      return this->modifiability;
    }
    size_t cacheHash() const {
      return Hash::combine(this->pointeeType, this->modifiability);
    }
    static bool cacheEquals(const TypeForgePointerSignature& lhs, const TypeForgePointerSignature& rhs) {
      return (lhs.pointeeType == rhs.pointeeType) && (lhs.modifiability == rhs.modifiability);
    }
  };

  class TypeForgeTaggableSignature : public ITaggableSignature {
    TypeForgeTaggableSignature(const TypeForgeTaggableSignature&) = delete;
    TypeForgeTaggableSignature& operator=(const TypeForgeTaggableSignature&) = delete;
  public:
    String description = {};
    int precedence = -1;
    TypeForgeTaggableSignature() {
    }
    TypeForgeTaggableSignature(TypeForgeTaggableSignature&& rhs) noexcept
      : description(std::move(rhs.description)),
        precedence(std::move(rhs.precedence)) {
    }
    virtual int print(Printer& printer) const override {
      printer.stream << this->description.toUTF8();
      return this->precedence;
    }
    size_t cacheHash() const {
      return Hash::combine(this->description.hash(), this->precedence);
    }
    static bool cacheEquals(const TypeForgeTaggableSignature& lhs, const TypeForgeTaggableSignature& rhs) {
      return (lhs.description == rhs.description) && (lhs.precedence == rhs.precedence);
    }
  };

  template<typename T>
  class TypeForgeBaseBuilder : public HardReferenceCounted<T> {
    TypeForgeBaseBuilder(const TypeForgeBaseBuilder&) = delete;
    TypeForgeBaseBuilder& operator=(const TypeForgeBaseBuilder&) = delete;
  protected:
    TypeForgeDefault& forge;
    bool built;
  public:
    explicit TypeForgeBaseBuilder(TypeForgeDefault& forge)
      : forge(forge),
        built(false) {
    }
  protected:
    virtual void hardDestroy() const override;
  };

  class TypeForgeFunctionBuilder : public TypeForgeBaseBuilder<ITypeForgeFunctionBuilder> {
    TypeForgeFunctionBuilder(const TypeForgeFunctionBuilder&) = delete;
    TypeForgeFunctionBuilder& operator=(const TypeForgeFunctionBuilder&) = delete;
  private:
    Type rtype; // reset to nullptr after building
    Type gtype;
    String fname;
    std::vector<TypeForgeFunctionSignatureParameter> parameters;
  public:
    explicit TypeForgeFunctionBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge) {
    }
    virtual void setFunctionName(const String& name) override {
      this->fname = name;
    }
    virtual void setGeneratedType(const Type& type) override;
    virtual void setReturnType(const Type& type) override {
      this->rtype = type;
    }
    virtual void addRequiredParameter(const Type& type, const String& name) override {
      this->parameters.emplace_back(this->parameters.size(), type, name, IFunctionSignatureParameter::Flags::Required);
    }
    virtual void addOptionalParameter(const Type& type, const String& name) override {
      this->parameters.emplace_back(this->parameters.size(), type, name, IFunctionSignatureParameter::Flags::None);
    }
    virtual const IFunctionSignature& build() override;
  };

  class TypeForgePropertyBuilder : public TypeForgeBaseBuilder<ITypeForgePropertyBuilder> {
    TypeForgePropertyBuilder(const TypeForgePropertyBuilder&) = delete;
    TypeForgePropertyBuilder& operator=(const TypeForgePropertyBuilder&) = delete;
  private:
    TypeForgePropertySignature signature;
  public:
    explicit TypeForgePropertyBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge) {
    }
    virtual void setUnknownProperty(const Type& type, Modifiability modifiability) override {
      assert(!this->built);
      this->signature.setUnknown(type, modifiability);
    }
    virtual void addProperty(const String& name, const Type& type, Modifiability modifiability) override {
      assert(!this->built);
      auto added = this->signature.addProperty(name, type, modifiability);
      assert(added);
      (void)added;
    }
    virtual const IPropertySignature& build() override;
  };

  class TypeForgeIndexBuilder : public TypeForgeBaseBuilder<ITypeForgeIndexBuilder> {
    TypeForgeIndexBuilder(const TypeForgeIndexBuilder&) = delete;
    TypeForgeIndexBuilder& operator=(const TypeForgeIndexBuilder&) = delete;
  private:
    TypeForgeIndexSignature signature;
  public:
    explicit TypeForgeIndexBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge) {
    }
    virtual void setResultType(const Type& type) override {
      assert(!this->built);
      assert(type != nullptr);
      this->signature.resultType = type;
    }
    virtual void setIndexType(const Type& type) override {
      assert(!this->built);
      assert(type != nullptr);
      this->signature.indexType = type;
    }
    virtual void setModifiability(Modifiability modifiability) override {
      assert(!this->built);
      this->signature.modifiability = modifiability;
    }
    virtual const IIndexSignature& build() override;
  };

  class TypeForgeIteratorBuilder : public TypeForgeBaseBuilder<ITypeForgeIteratorBuilder> {
    TypeForgeIteratorBuilder(const TypeForgeIteratorBuilder&) = delete;
    TypeForgeIteratorBuilder& operator=(const TypeForgeIteratorBuilder&) = delete;
  private:
    TypeForgeIteratorSignature signature;
  public:
    explicit TypeForgeIteratorBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge) {
    }
    virtual void setIterationType(const Type& type) override {
      assert(!this->built);
      assert(type != nullptr);
      this->signature.interationType = type;
    }
    virtual const IIteratorSignature& build() override;
  };

  class TypeForgePointerBuilder : public TypeForgeBaseBuilder<ITypeForgePointerBuilder> {
    TypeForgePointerBuilder(const TypeForgePointerBuilder&) = delete;
    TypeForgePointerBuilder& operator=(const TypeForgePointerBuilder&) = delete;
  private:
    TypeForgePointerSignature signature;
  public:
    explicit TypeForgePointerBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge) {
    }
    virtual void setPointeeType(const Type& type) override {
      assert(!this->built);
      assert(type != nullptr);
      this->signature.pointeeType = type;
    }
    virtual void setModifiability(Modifiability modifiability) override {
      assert(!this->built);
      this->signature.modifiability = modifiability;
    }
    virtual const IPointerSignature& build() override;
  };

  class TypeForgeTaggableBuilder : public TypeForgeBaseBuilder<ITypeForgeTaggableBuilder> {
    TypeForgeTaggableBuilder(const TypeForgeTaggableBuilder&) = delete;
    TypeForgeTaggableBuilder& operator=(const TypeForgeTaggableBuilder&) = delete;
  private:
    TypeForgeTaggableSignature signature;
  public:
    explicit TypeForgeTaggableBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge) {
    }
    virtual void setDescription(const String& description, int precedence) override {
      assert(!this->built);
      assert(!description.empty());
      assert(precedence >= 0);
      this->signature.description = description;
      this->signature.precedence = precedence;
    }
    virtual const ITaggableSignature& build() override;
  };

  class TypeForgeComplexBuilder : public TypeForgeBaseBuilder<ITypeForgeComplexBuilder> {
    TypeForgeComplexBuilder(const TypeForgeComplexBuilder&) = delete;
    TypeForgeComplexBuilder& operator=(const TypeForgeComplexBuilder&) = delete;
  private:
    ValueFlags flags;
    TypeShapeSet shapeset;
  public:
    explicit TypeForgeComplexBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge),
        flags(ValueFlags::None),
        shapeset() {
    }
    virtual bool addFlags(ValueFlags bits) override {
      auto before = this->flags;
      this->flags = Bits::set(before, bits);
      return this->flags != before;
    }
    virtual bool removeFlags(ValueFlags bits) override {
      auto before = this->flags;
      this->flags = Bits::clear(before, bits);
      return this->flags != before;
    }
    virtual bool addShape(const TypeShape& shape) override {
      assert(shape.validate());
      assert(shape->taggable != nullptr);
      return this->shapeset.add(shape);
    }
    virtual bool removeShape(const TypeShape& shape) override {
      return this->shapeset.remove(shape);
    }
    virtual bool addType(const Type& type) override {
      assert(type.validate());
      auto changed = this->addFlags(type->getPrimitiveFlags());
      size_t count = type->getShapeCount();
      for (size_t index = 0; index < count; ++index) {
        TypeShape shape{ *type->getShape(index) };
        changed |= this->addShape(shape);
      }
      return changed;
    }
    virtual Type build() override;
  };

  class TypeForgeDefault : public HardReferenceCountedAllocator<ITypeForge> {
    TypeForgeDefault(const TypeForgeDefault&) = delete;
    TypeForgeDefault& operator=(const TypeForgeDefault&) = delete;
  private:
    HardPtr<IBasket> basket;
    TypeForgeCacheSet<TypeForgeShape> cacheShape;
    TypeForgeCacheSet<TypeForgeFunctionSignatureParameter> cacheFunctionSignatureParameter;
    TypeForgeCacheSet<TypeForgeFunctionSignature> cacheFunctionSignature;
    TypeForgeCacheSet<TypeForgePropertySignature> cachePropertySignature;
    TypeForgeCacheSet<TypeForgeIndexSignature> cacheIndexSignature;
    TypeForgeCacheSet<TypeForgeIteratorSignature> cacheIteratorSignature;
    TypeForgeCacheSet<TypeForgePointerSignature> cachePointerSignature;
    TypeForgeCacheSet<TypeForgeTaggableSignature> cacheTaggableSignature;
    TypeForgeCacheMap<TypeForgeComplex::Detail, TypeForgeComplex> cacheComplex;
    std::set<const ICollectable*> owned;
  public:
    TypeForgeDefault(IAllocator& allocator, IBasket& basket)
      : HardReferenceCountedAllocator(allocator),
        basket(&basket) {
    }
    virtual ~TypeForgeDefault() override {
      for (auto* instance : this->owned) {
        this->destroy(instance);
      }
    }
    virtual TypeShape forgeArrayShape(const Type& element, Modifiability modifiability) override {
      TypeForgeShape shape;
      {
        // Properties
        auto builder = this->createPropertyBuilder();
        auto lengthModifiability = Bits::hasAnySet(modifiability, Modifiability::Delete) ? Modifiability::ReadWriteMutate : Modifiability::Read;
        builder->addProperty(StringBuilder::concat(this->allocator, "length"), Type::Int, lengthModifiability);
        shape.dotable = &builder->build();
      }
      {
        // Indexing
        auto builder = this->createIndexBuilder();
        builder->setResultType(element);
        builder->setModifiability(modifiability);
        shape.indexable = &builder->build();
      }
      {
        // Iteration
        auto builder = this->createIteratorBuilder();
        builder->setIterationType(element);
        shape.iterable = &builder->build();
      }
      {
        // Taggable
        auto builder = this->createTaggableBuilder();
        builder->setDescription(this->typeSuffix(element, "[]"), 1);
        shape.taggable = &builder->build();
      }
      return this->forgeShape(std::move(shape));
    }
    virtual TypeShape forgeFunctionShape(const IFunctionSignature& signature) override {
      TypeForgeShape shape;
      shape.callable = &signature;
      {
        // Taggable
        auto builder = this->createTaggableBuilder();
        Print::Options options{};
        options.names = false;
        StringBuilder sb{ options };
        Type::print(sb, signature);
        builder->setDescription(sb.build(this->allocator), 1);
        shape.taggable = &builder->build();
      }
      return this->forgeShape(std::move(shape));
    }
    virtual TypeShape forgeIteratorShape(const Type& element) override {
      TypeForgeShape shape;
      {
        // Callable
        auto builder = this->createFunctionBuilder();
        builder->setReturnType(this->forgeVoidableType(element, true));
        shape.callable = &builder->build();
      }
      {
        // Iteration
        auto builder = this->createIteratorBuilder();
        builder->setIterationType(element);
        shape.iterable = &builder->build();
      }
      {
        // Taggable
        auto builder = this->createTaggableBuilder();
        builder->setDescription(this->typeSuffix(element, "!"), 1);
        shape.taggable = &builder->build();
      }
      return this->forgeShape(std::move(shape));
    }
    virtual TypeShape forgePointerShape(const Type& pointee, Modifiability modifiability) override {
      TypeForgeShape shape;
      {
        // Pointer
        auto builder = this->createPointerBuilder();
        builder->setPointeeType(pointee);
        builder->setModifiability(modifiability);
        shape.pointable = &builder->build();
      }
      {
        // Taggable
        auto builder = this->createTaggableBuilder();
        builder->setDescription(this->typeSuffix(pointee, "*"), 1);
        shape.taggable = &builder->build();
      }
      return this->forgeShape(std::move(shape));
    }
    virtual TypeShape forgeStringShape() override {
      TypeForgeShape shape;
      {
        // Properties
        auto builder = this->createPropertyBuilder();
        builder->addProperty(StringBuilder::concat(this->allocator, "length"), Type::Int, Modifiability::Read);
        shape.dotable = &builder->build();
      }
      {
        // Indexing
        auto builder = this->createIndexBuilder();
        builder->setResultType(Type::String);
        shape.indexable = &builder->build();
      }
      {
        // Iteration
        auto builder = this->createIteratorBuilder();
        builder->setIterationType(Type::String);
        shape.iterable = &builder->build();
      }
      {
        // Taggable
        auto builder = this->createTaggableBuilder();
        builder->setDescription(StringBuilder::concat(this->allocator, "string"), 0);
        shape.taggable = &builder->build();
      }
      return this->forgeShape(std::move(shape));
    }
    virtual Type forgePrimitiveType(ValueFlags flags) override {
      auto primitive  = TypeForgePrimitive::forge(flags);
      assert(primitive != nullptr);
      return primitive;
    }
    virtual Type forgeComplexType(ValueFlags flags, const TypeShapeSet& shapeset) override {
      if (shapeset.empty()) {
        if (flags == ValueFlags::None) {
          return nullptr;
        }
        return this->forgePrimitiveType(flags);
      }
      TypeForgeComplex::Detail detail{ flags, shapeset };
      return this->forgeComplex(std::move(detail));
    }
    virtual Type forgeUnionType(const Type& lhs, const Type& rhs) override {
      TypeForgeComplexBuilder builder{ *this };
      builder.addType(lhs);
      builder.addType(rhs);
      return builder.build();
    }
    virtual Type forgeNullableType(const Type& type, bool nullable) override {
      return this->forgeFlags(type, ValueFlags::Null, nullable);
    }
    virtual Type forgeVoidableType(const Type& type, bool voidable) override {
      return this->forgeFlags(type, ValueFlags::Void, voidable);
    }
    virtual Type forgeArrayType(const Type& element, Modifiability modifiability) override {
      return this->forgeShapeType(this->forgeArrayShape(element, modifiability));
    }
    virtual Type forgeIterationType(const Type& type) override {
      // TODO optimize
      auto builder = this->createComplexBuilder();
      auto flags = type->getPrimitiveFlags();
      if (Bits::hasAnySet(flags, ValueFlags::Object)) {
        builder->addFlags(ValueFlags::AnyQ);
      }
      if (Bits::hasAnySet(flags, ValueFlags::String)) {
        builder->addFlags(ValueFlags::String);
      }
      size_t count = type->getShapeCount();
      for (size_t index = 0; index < count; ++index) {
        auto* shape = type->getShape(index);
        if ((shape != nullptr) && (shape->iterable != nullptr)) {
          builder->addType(shape->iterable->getIterationType());
        }
      }
      return builder->build();
    }
    virtual Type forgeIteratorType(const Type& element) override {
      return this->forgeShapeType(this->forgeIteratorShape(element));
    }
    virtual Type forgeFunctionType(const IFunctionSignature& signature) override {
      return this->forgeShapeType(this->forgeFunctionShape(signature));
    }
    virtual Type forgePointerType(const Type& pointee, Modifiability modifiability) override {
      return this->forgeShapeType(this->forgePointerShape(pointee, modifiability));
    }
    virtual Type forgeShapeType(const TypeShape& shape) override {
      assert(shape.validate());
      assert(shape->taggable != nullptr);
      TypeShapeSet shapeset;
      shapeset.add(shape);
      TypeForgeComplex::Detail detail{ ValueFlags::None, shapeset };
      return this->forgeComplex(std::move(detail));
    }
    virtual Assignability isTypeAssignable(const Type& dst, const Type& src) override {
      return this->computeTypeAssignability(dst, src);
    }
    virtual Assignability isFunctionSignatureAssignable(const IFunctionSignature& dst, const IFunctionSignature& src) override {
      return this->computeFunctionSignatureAssignability(&dst, &src);
    }
    virtual HardPtr<ITypeForgeFunctionBuilder> createFunctionBuilder() override {
      return this->createBuilder<TypeForgeFunctionBuilder>();
    }
    virtual HardPtr<ITypeForgePropertyBuilder> createPropertyBuilder() override {
      return this->createBuilder<TypeForgePropertyBuilder>();
    }
    virtual HardPtr<ITypeForgeIndexBuilder> createIndexBuilder() override {
      return this->createBuilder<TypeForgeIndexBuilder>();
    }
    virtual HardPtr<ITypeForgeIteratorBuilder> createIteratorBuilder() override {
      return this->createBuilder<TypeForgeIteratorBuilder>();
    }
    virtual HardPtr<ITypeForgePointerBuilder> createPointerBuilder() override {
      return this->createBuilder<TypeForgePointerBuilder>();
    }
    virtual HardPtr<ITypeForgeTaggableBuilder> createTaggableBuilder() override {
      return this->createBuilder<TypeForgeTaggableBuilder>();
    }
    virtual HardPtr<ITypeForgeComplexBuilder> createComplexBuilder() override {
      return this->createBuilder<TypeForgeComplexBuilder>();
    }
    TypeShape forgeShape(TypeForgeShape&& shape) {
      return TypeShape(this->cacheShape.fetch(std::move(shape)));
    }
    Type forgeComplex(TypeForgeComplex::Detail&& detail) {
      auto factory = [this](TypeForgeComplex::Detail&& detail) {
        // TODO unroll this lambda and 'owned' update
        return this->createOwned<TypeForgeComplex>(this->allocator, std::move(detail));
      };
      return Type(&this->cacheComplex.fetch(std::move(detail), factory));
    }
    Type forgeFlags(const Type& type, ValueFlags flags, bool required) {
      auto before = type->getPrimitiveFlags();
      if (required) {
        if (!Bits::hasAllSet(before, flags)) {
          TypeForgeComplexBuilder builder{ *this };
          builder.addType(type);
          builder.addFlags(flags);
          return builder.build();
        }
      } else {
        if (Bits::hasAnySet(before, flags)) {
          TypeForgeComplexBuilder builder{ *this };
          builder.addType(type);
          builder.removeFlags(flags);
          return builder.build();
        }
      }
      return type;
    }
    const IFunctionSignatureParameter& forgeFunctionSignatureParameter(TypeForgeFunctionSignatureParameter&& parameter) {
      return this->cacheFunctionSignatureParameter.fetch(std::move(parameter));
    }
    const IFunctionSignature& forgeFunctionSignature(TypeForgeFunctionSignature&& signature) {
      return this->cacheFunctionSignature.fetch(std::move(signature));
    }
    const IPropertySignature& forgePropertySignature(TypeForgePropertySignature&& signature) {
      return this->cachePropertySignature.fetch(std::move(signature));
    }
    const IIndexSignature& forgeIndexSignature(TypeForgeIndexSignature&& signature) {
      return this->cacheIndexSignature.fetch(std::move(signature));
    }
    const IIteratorSignature& forgeIteratorSignature(TypeForgeIteratorSignature&& signature) {
      return this->cacheIteratorSignature.fetch(std::move(signature));
    }
    const IPointerSignature& forgePointerSignature(TypeForgePointerSignature&& signature) {
      return this->cachePointerSignature.fetch(std::move(signature));
    }
    const ITaggableSignature& forgeTaggableSignature(TypeForgeTaggableSignature&& signature) {
      return this->cacheTaggableSignature.fetch(std::move(signature));
    }
    template<typename T>
    HardPtr<T> createBuilder() {
      return HardPtr(this->allocator.makeRaw<T>(*this));
    }
    template<typename T, typename... ARGS>
    T* createOwned(ARGS&&... args) {
      auto* instance = this->allocator.makeRaw<T>(std::forward<ARGS>(args)...);
      this->owned.insert(instance);
      return instance;
    }
    template<typename T>
    void destroy(T* instance) {
      this->allocator.destroy(instance);
    }
  private:
    Assignability computeTypeAssignability(const Type& dst, const Type& src) {
      assert(dst.validate());
      assert(src.validate());
      if (dst == src) {
        return Assignability::Always;
      }
      auto fdst = dst->getPrimitiveFlags();
      auto fsrc = src->getPrimitiveFlags();
      if (!src->isPrimitive()) {
        fsrc = fsrc | ValueFlags::Object;
      }
      auto assignabilityPrimitive = this->computeTypeAssignabilityFlags(fdst, fsrc);
      if (dst->isPrimitive() || src->isPrimitive()) {
        return assignabilityPrimitive;
      }
      auto assignabilityComplex = this->computeTypeAssignabilityComplexComplex(*dst, *src);
      if (fdst == ValueFlags::None) {
        assert(assignabilityPrimitive == Assignability::Never);
        return this->computeTypeAssignabilityComplexComplex(*dst, *src);
      }
      return assignabilityUnion(assignabilityPrimitive, assignabilityComplex);
    }
    Assignability computeTypeAssignabilityComplexComplex(const IType& dst, const IType& src) {
      auto hasAlways = false;
      auto hasNever = false;
      auto count = dst.getShapeCount();
      assert(count > 0);
      for (size_t index = 0; index < count; ++index) {
        auto* shape = dst.getShape(index);
        assert(shape != nullptr);
        switch (this->computeTypeAssignabilityShapeComplex(*shape, src)) {
        case Assignability::Never:
          if (hasAlways) {
            return Assignability::Sometimes;
          }
          hasNever = true;
          break;
        case Assignability::Sometimes:
          return Assignability::Sometimes;
        case Assignability::Always:
          if (hasNever) {
            return Assignability::Sometimes;
          }
          hasAlways = true;
          break;
        }
      }
      assert(hasAlways ^ hasNever);
      return hasAlways ? Assignability::Always : Assignability::Never;
    }
    Assignability computeTypeAssignabilityShapeComplex(const IType::Shape& dst, const IType& src) {
      auto hasAlways = false;
      auto hasNever = false;
      auto count = src.getShapeCount();
      assert(count > 0);
      for (size_t index = 0; index < count; ++index) {
        auto* shape = src.getShape(index);
        assert(shape != nullptr);
        switch (this->computeTypeAssignabilityShapeShape(dst, *shape)) {
        case Assignability::Never:
          if (hasAlways) {
            return Assignability::Sometimes;
          }
          hasNever = true;
          break;
        case Assignability::Sometimes:
          return Assignability::Sometimes;
        case Assignability::Always:
          if (hasNever) {
            return Assignability::Sometimes;
          }
          hasAlways = true;
          break;
        }
      }
      assert(hasAlways ^ hasNever);
      return hasAlways ? Assignability::Always : Assignability::Never;
    }
    Assignability computeTypeAssignabilityShapeShape(const IType::Shape& dst, const IType::Shape& src) {
      auto always = true;
      switch (this->computeFunctionSignatureAssignability(dst.callable, src.callable)) {
      case Assignability::Never:
        return Assignability::Never;
      case Assignability::Sometimes:
        always = false;
        break;
      case Assignability::Always:
        break;
      }
      switch (this->computePropertySignatureAssignability(dst.dotable, src.dotable)) {
      case Assignability::Never:
        return Assignability::Never;
      case Assignability::Sometimes:
        always = false;
        break;
      case Assignability::Always:
        break;
      }
      switch (this->computeIndexSignatureAssignability(dst.indexable, src.indexable)) {
      case Assignability::Never:
        return Assignability::Never;
      case Assignability::Sometimes:
        always = false;
        break;
      case Assignability::Always:
        break;
      }
      switch (this->computeIteratorSignatureAssignability(dst.iterable, src.iterable)) {
      case Assignability::Never:
        return Assignability::Never;
      case Assignability::Sometimes:
        always = false;
        break;
      case Assignability::Always:
        break;
      }
      switch (this->computePointerSignatureAssignability(dst.pointable, src.pointable)) {
      case Assignability::Never:
        return Assignability::Never;
      case Assignability::Sometimes:
        always = false;
        break;
      case Assignability::Always:
        break;
      }
      switch (this->computeTaggableSignatureAssignability(dst.taggable, src.taggable)) {
      case Assignability::Never:
        return Assignability::Never;
      case Assignability::Sometimes:
        always = false;
        break;
      case Assignability::Always:
        break;
      }
      return always ? Assignability::Always : Assignability::Sometimes;
    }
    Assignability computeTypeAssignabilityFlags(ValueFlags fdst, ValueFlags fsrc) {
      if (Bits::hasAllSet(fdst, fsrc)) {
        return Assignability::Always;
      }
      if (Bits::hasAnySet(fsrc, ValueFlags::Int) && Bits::hasAnySet(fdst, ValueFlags::Float)) {
        // Promote integers to floats
        fsrc = Bits::set(Bits::clear(fsrc, ValueFlags::Int), ValueFlags::Float);
        if (Bits::hasAllSet(fdst, fsrc)) {
          return Assignability::Always;
        }
      }
      if (Bits::hasAnySet(fdst, fsrc)) {
        return Assignability::Sometimes;
      }
      return Assignability::Never;
    }
    Assignability computeFunctionSignatureAssignability(const IFunctionSignature* dst, const IFunctionSignature* src) {
      // TODO more compatability checks
      auto retval = Assignability::Always;
      if (dst != src) {
        if (dst == nullptr) {
          // Not interested in function signatures
          return Assignability::Always;
        }
        if (src == nullptr) {
          // Function signature required but not supplied
          return Assignability::Never;
        }
        // The return types must be compatible
        switch (this->computeTypeAssignability(dst->getReturnType(), src->getReturnType())) {
        case Assignability::Never:
          return Assignability::Never;
        case Assignability::Sometimes:
          retval = Assignability::Sometimes;
          break;
        case Assignability::Always:
          break;
        }
        // TODO optional parameters
        auto ndst = dst->getParameterCount();
        auto nsrc = src->getParameterCount();
        if (ndst != nsrc) {
          return Assignability::Never;
        }
        for (size_t i = 0; i < ndst; ++i) {
          auto* psrc = &src->getParameter(i);
          auto* pdst = &dst->getParameter(i);
          switch (this->computeTypeAssignability(pdst->getType(), psrc->getType())) {
          case Assignability::Never:
            return Assignability::Never;
          case Assignability::Sometimes:
            retval = Assignability::Sometimes;
            break;
          case Assignability::Always:
            break;
          }
        }
      }
      return retval;
    }
    Assignability computePropertySignatureAssignability(const IPropertySignature* dst, const IPropertySignature* src) {
      // TODO more compatability checks
      auto retval = Assignability::Always;
      if (dst != src) {
        if (dst == nullptr) {
          // Not interested in property signatures
          return Assignability::Always;
        }
        if (src == nullptr) {
          // Property signature required but not supplied
          return Assignability::Never;
        }
        // TODO property assignability
        assert(false);
      }
      return retval;
    }
    Assignability computeIndexSignatureAssignability(const IIndexSignature* dst, const IIndexSignature* src) {
      // TODO more compatability checks
      auto retval = Assignability::Always;
      if (dst != src) {
        if (dst == nullptr) {
          // Not interested in index signatures
          return Assignability::Always;
        }
        if (src == nullptr) {
          // Index signature required but not supplied
          return Assignability::Never;
        }
        // TODO index assignability
        assert(false);
      }
      return retval;
    }
    Assignability computeIteratorSignatureAssignability(const IIteratorSignature* dst, const IIteratorSignature* src) {
      // TODO more compatability checks
      auto retval = Assignability::Always;
      if (dst != src) {
        if (dst == nullptr) {
          // Not interested in iterator signatures
          return Assignability::Always;
        }
        if (src == nullptr) {
          // Iterator signature required but not supplied
          return Assignability::Never;
        }
        // TODO iterator assignability
        assert(false);
      }
      return retval;
    }
    Assignability computePointerSignatureAssignability(const IPointerSignature* dst, const IPointerSignature* src) {
      // TODO more compatability checks
      auto retval = Assignability::Always;
      if (dst != src) {
        if (dst == nullptr) {
          // Not interested in pointer signatures
          return Assignability::Always;
        }
        if (src == nullptr) {
          // Pointer signature required but not supplied
          return Assignability::Never;
        }
        retval = assignabilityFromModifiability(dst->getModifiability(), src->getModifiability());
        retval = assignabilityIntersection(retval, this->computeTypeAssignability(dst->getPointeeType(), src->getPointeeType()));
      }
      return retval;
    }
    Assignability computeTaggableSignatureAssignability(const ITaggableSignature*, const ITaggableSignature*) {
      // TODO more compatability checks
      return Assignability::Always;
    }
    String typeSuffix(const Type& type, const char* suffix) const {
      assert(type.validate());
      assert(suffix != nullptr);
      StringBuilder sb;
      auto precedence = type.print(sb);
      if (precedence == 2) {
        // Wrap 'a|b' in parentheses
        return StringBuilder::concat(this->allocator, '(', sb.toUTF8(), ')', suffix);
      }
      sb << suffix;
      return sb.build(this->allocator);
    }
  };

  template<typename T>
  void TypeForgeBaseBuilder<T>::hardDestroy() const {
    assert(this->atomic.get() == 0);
    this->forge.destroy(this);
  }

  void TypeForgeFunctionBuilder::setGeneratedType(const Type& type) {
    // Generated type is 'T' so the generator return type is '(void|T)()' aka 'T!'
    this->gtype = type;
    this->rtype = this->forge.forgeIteratorType(type);
  }

  const IFunctionSignature& TypeForgeFunctionBuilder::build() {
    assert(this->rtype != nullptr);
    TypeForgeFunctionSignature signature(this->rtype, this->gtype, this->fname);
    this->rtype = nullptr;
    for (auto& parameter : this->parameters) {
      auto& forged = this->forge.forgeFunctionSignatureParameter(std::move(parameter));
      signature.parameters.emplace_back(&forged);
    }
    return this->forge.forgeFunctionSignature(std::move(signature));
  }

  const IPropertySignature& TypeForgePropertyBuilder::build() {
    assert(!this->built);
    this->built = true;
    return this->forge.forgePropertySignature(std::move(this->signature));
  }

  const IIndexSignature& TypeForgeIndexBuilder::build() {
    assert(!this->built);
    this->built = true;
    return this->forge.forgeIndexSignature(std::move(this->signature));
  }

  const IIteratorSignature& TypeForgeIteratorBuilder::build() {
    assert(!this->built);
    this->built = true;
    return this->forge.forgeIteratorSignature(std::move(this->signature));
  }

  const IPointerSignature& TypeForgePointerBuilder::build() {
    assert(!this->built);
    this->built = true;
    return this->forge.forgePointerSignature(std::move(this->signature));
  }

  const ITaggableSignature& TypeForgeTaggableBuilder::build() {
    assert(!this->built);
    assert(!this->signature.description.empty());
    assert(this->signature.precedence >= 0);
    this->built = true;
    return this->forge.forgeTaggableSignature(std::move(this->signature));
  }

  Type TypeForgeComplexBuilder::build() {
    return this->forge.forgeComplexType(this->flags, this->shapeset);
  }

  const char* valueFlagsComponent(ValueFlags flags) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
      switch (flags) {
#define EGG_OVUM_VALUE_FLAGS_COMPONENT(name, text) case ValueFlags::name: return text;
        EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_COMPONENT)
#undef EGG_OVUM_VALUE_FLAGS_COMPONENT
      case ValueFlags::Any:
        return "any";
      }
    EGG_WARNING_SUPPRESS_SWITCH_END
      return nullptr;
  }

  int valueFlagsWrite(std::ostream& os, ValueFlags flags) {
    // Returns precedence:
    //  0: Simple keyword, e.g. 'int'
    //  1: Simple suffix, e.g. 'int?'
    //  2: Type union, e.g. 'float|int'
    //  3: Function signature, e.g. 'int(float)'
    assert(flags != ValueFlags::None);
    auto* component = valueFlagsComponent(flags);
    if (component != nullptr) {
      os << component;
      return 0;
    }
    if (Bits::hasAnySet(flags, ValueFlags::Null)) {
      auto nonnull = valueFlagsWrite(os, Bits::clear(flags, ValueFlags::Null));
      os << '?';
      return std::max(nonnull, 1);
    }
    if (Bits::hasAnySet(flags, ValueFlags::Void)) {
      os << "void|";
      (void)valueFlagsWrite(os, Bits::clear(flags, ValueFlags::Void));
      return 2;
    }
    auto head = Bits::topmost(flags);
    assert(head != ValueFlags::None);
    component = valueFlagsComponent(head);
    assert(component != nullptr);
    os << component << '|';
    (void)valueFlagsWrite(os, Bits::clear(flags, head));
    return 2;
  }
}

using namespace egg::internal;

// Common types
const Type Type::None{ TypeForgePrimitive::forge(ValueFlags::None) };
const Type Type::Void{ TypeForgePrimitive::forge(ValueFlags::Void) };
const Type Type::Null{ TypeForgePrimitive::forge(ValueFlags::Null) };
const Type Type::Bool{ TypeForgePrimitive::forge(ValueFlags::Bool) };
const Type Type::Int{ TypeForgePrimitive::forge(ValueFlags::Int) };
const Type Type::Float{ TypeForgePrimitive::forge(ValueFlags::Float) };
const Type Type::String{ TypeForgePrimitive::forge(ValueFlags::String) };
const Type Type::Arithmetic{ TypeForgePrimitive::forge(ValueFlags::Arithmetic) };
const Type Type::Object{ TypeForgePrimitive::forge(ValueFlags::Object) };
const Type Type::Any{ TypeForgePrimitive::forge(ValueFlags::Any) };
const Type Type::AnyQ{ TypeForgePrimitive::forge(ValueFlags::AnyQ) };

int egg::ovum::Type::print(Printer& printer) const {
  auto* p = this->ptr;
  if (p == nullptr) {
    printer << "<unknown>";
    return -1;
  }
  return p->print(printer);
}

int egg::ovum::Type::print(Printer& printer, ValueFlags primitive, int complexPrecedence) {
  if (complexPrecedence < 0) {
    // No preceding complex components
    if (primitive == ValueFlags::None) {
      printer << "<none>";
      return -1;
    }
    return valueFlagsWrite(printer.stream, primitive);
  }
  if (primitive == ValueFlags::None) {
    return complexPrecedence;
  }
  if (primitive == ValueFlags::Null) {
    printer << '?';
    return std::max(complexPrecedence, 1);
  }
  printer << '|';
  valueFlagsWrite(printer.stream, primitive);
  return 2;
}

int egg::ovum::Type::print(Printer& printer, const IType::Shape& shape) {
  if (shape.taggable != nullptr) {
    return shape.taggable->print(printer);
  }
  // TODO
  printer.stream << "<SHAPE:" << &shape << ">";
  return -1;
}

void egg::ovum::Type::print(Printer& printer, const IFunctionSignature& signature) {
  // Write the return type to a separate buffer in case it has to be wrapped in parentheses
  StringBuilder sb{ printer.options };
  auto precedence = signature.getReturnType().print(sb);
  if (precedence == 2) {
    // Wrap 'a|b' in parentheses
    printer << '(' << sb.toUTF8() << ')';
  } else {
    // No need to wrap
    printer << sb.toUTF8();
  }
  if (printer.options.names) {
    auto name = signature.getName();
    if (!name.empty()) {
      printer << ' ' << name;
    }
  }
  printer << '(';
  auto count = signature.getParameterCount();
  for (size_t index = 0; index < count; ++index) {
    if (index > 0) {
      printer << ',';
      if (!printer.options.concise) {
        printer << ' ';
      }
    }
    auto& parameter = signature.getParameter(index);
    parameter.getType().print(printer);
    if (printer.options.names) {
      auto name = parameter.getName();
      if (!name.empty()) {
        printer << ' ' << name;
      }
    }
    if (Bits::hasNoneSet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Required)) {
      if (printer.options.concise) {
        printer << "=null";
      } else {
        printer << " = null";
      }
    }
  }
  printer << ')';
}

egg::ovum::HardPtr<egg::ovum::ITypeForge> egg::ovum::TypeForgeFactory::createTypeForge(IAllocator& allocator, IBasket& basket) {
  return allocator.makeHard<TypeForgeDefault>(basket);
}
