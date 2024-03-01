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

  struct TypeForgeCacheHelper {
    struct Hash {
      size_t operator()(const IType* key) const {
        return Hasher<size_t>::hash(key);
      }
      template<typename K>
      size_t operator()(const K* key) const {
        return key->cacheHash();
      }
    };
    struct Equals {
      bool operator()(const IType* lhs, const IType* rhs) const {
        return lhs == rhs;
      }
      template<typename K>
      bool operator()(const K* lhs, const K* rhs) const {
        return K::cacheEquals(*lhs, *rhs);
      }
    };
  };

  template<typename K, typename V>
  class TypeForgeCacheMap {
    TypeForgeCacheMap(const TypeForgeCacheMap&) = delete;
    TypeForgeCacheMap& operator=(const TypeForgeCacheMap&) = delete;
  private:
    std::unordered_map<const K*, V*, TypeForgeCacheHelper::Hash, TypeForgeCacheHelper::Equals> cache;
    std::mutex mutex;
  public:
    TypeForgeCacheMap() {}
    V& add(const K& key, V& value) {
      return *this->cache.emplace(&key, &value).first->second;
    }
    V* find(const K& key) {
      std::lock_guard<std::mutex> lock{ this->mutex };
      auto found = this->cache.find(&key);
      if (found != this->cache.end()) {
        return found->second;
      }
      return nullptr;
    }
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
    virtual HardPtr<IVMTypeSpecification> getSpecification() const override {
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
      HardPtr<IVMTypeSpecification> specification;
      Detail(ValueFlags flags, const TypeShapeSet& shapeset, const HardPtr<IVMTypeSpecification>& specification)
        : flags(flags),
          shapes(),
          specification(specification) {
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
    TypeForgeComplex(IAllocator& allocator, ValueFlags flags, const TypeShapeSet& shapeset, const HardPtr<IVMTypeSpecification>& specification)
      : SoftReferenceCountedAllocator(allocator),
        detail(flags, shapeset, specification) {
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
    virtual HardPtr<IVMTypeSpecification> getSpecification() const override {
      return this->detail.specification;
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
    String name;
    std::vector<const IFunctionSignatureParameter*> parameters;
    TypeForgeFunctionSignature(const Type& rtype, const String& name)
      : rtype(rtype),
        name(name),
        parameters() {
    }
    TypeForgeFunctionSignature(TypeForgeFunctionSignature&& rhs) noexcept
      : rtype(std::move(rhs.rtype)),
        name(std::move(rhs.name)),
        parameters(std::move(rhs.parameters)) {
    }
    virtual String getName() const override {
      return this->name;
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
      hash.add(this->rtype, this->name).addFrom(this->parameters.begin(), this->parameters.end());
      return hash;
    }
    static bool cacheEquals(const TypeForgeFunctionSignature& lhs, const TypeForgeFunctionSignature& rhs) {
      return (lhs.rtype == rhs.rtype) && (lhs.name == rhs.name) && (lhs.parameters == rhs.parameters);
    }
  };

  class TypeForgePropertySignature : public IPropertySignature {
    TypeForgePropertySignature(const TypeForgePropertySignature&) = delete;
    TypeForgePropertySignature& operator=(const TypeForgePropertySignature&) = delete;
  public:
    struct Entry {
      Type type = nullptr;
      Accessability accessability = Accessability::None;
      size_t hash() const {
        // Used by 'TypeForgePropertySignature::cacheHash()'
        return Hash::combine(this->type.get(), this->accessability);
      }
      bool operator==(const Entry& rhs) const {
        // Used by 'TypeForgePropertySignature::cacheEquals()'
        return (this->type == rhs.type) && (this->accessability == rhs.accessability);
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
    virtual Accessability getAccessability(const String& property) const override {
      return this->findByName(property).accessability;
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
    void setUnknown(const Type& type, Accessability accessability) {
      this->unknown.type = type;
      this->unknown.accessability = accessability;
    }
    bool addProperty(const String& name, const Type& type, Accessability accessability) {
      return this->entries.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple(type, accessability)).second;
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
    Accessability accessability = Accessability::Get | Accessability::Set;
    TypeForgeIndexSignature() {
    }
    TypeForgeIndexSignature(TypeForgeIndexSignature&& rhs) noexcept
      : resultType(std::move(rhs.resultType)),
        indexType(std::move(rhs.indexType)),
        accessability(std::move(rhs.accessability)) {
    }
    virtual Type getResultType() const override {
      return this->resultType;
    }
    virtual Type getIndexType() const override {
      return this->indexType;
    }
    virtual Accessability getAccessability() const override {
      return this->accessability;
    }
    size_t cacheHash() const {
      return Hash::combine(this->resultType, this->indexType, this->accessability);
    }
    static bool cacheEquals(const TypeForgeIndexSignature& lhs, const TypeForgeIndexSignature& rhs) {
      return (lhs.resultType == rhs.resultType) && (lhs.indexType == rhs.indexType) && (lhs.accessability == rhs.accessability);
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
    Modifiability modifiability = Modifiability::All;
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
    String fname;
    std::vector<TypeForgeFunctionSignatureParameter> parameters;
  public:
    explicit TypeForgeFunctionBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge) {
    }
    virtual void setFunctionName(const String& name) override {
      this->fname = name;
    }
    virtual void setReturnType(const Type& type) override {
      this->rtype = type;
    }
    virtual void addRequiredParameter(const String& name, const Type& type) override {
      this->parameters.emplace_back(this->parameters.size(), type, name, IFunctionSignatureParameter::Flags::Required);
    }
    virtual void addOptionalParameter(const String& name, const Type& type) override {
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
    virtual void setUnknownProperty(const Type& type, Accessability accessability) override {
      assert(!this->built);
      this->signature.setUnknown(type, accessability);
    }
    virtual void addProperty(const String& name, const Type& type, Accessability accessability) override {
      assert(!this->built);
      auto added = this->signature.addProperty(name, type, accessability);
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
    virtual void setAccessability(Accessability accessability) override {
      assert(!this->built);
      this->signature.accessability = accessability;
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
    HardPtr<IVMTypeSpecification> specification;
  public:
    explicit TypeForgeComplexBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge),
        flags(ValueFlags::None) {
    }
    virtual void setSpecification(IVMTypeSpecification* spec) override {
      this->specification.set(spec);
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

  class TypeForgeMetashapeBuilder : public TypeForgeBaseBuilder<ITypeForgeMetashapeBuilder> {
    TypeForgeMetashapeBuilder(const TypeForgeMetashapeBuilder&) = delete;
    TypeForgeMetashapeBuilder& operator=(const TypeForgeMetashapeBuilder&) = delete;
  private:
    const IFunctionSignature* callableSignature;
    HardPtr<ITypeForgeIndexBuilder> indexBuilder;
    HardPtr<ITypeForgePointerBuilder> pointerBuilder;
    HardPtr<ITypeForgePropertyBuilder> propertyBuilder;
    HardPtr<ITypeForgeTaggableBuilder> taggableBuilder;
  public:
    explicit TypeForgeMetashapeBuilder(TypeForgeDefault& forge)
      : TypeForgeBaseBuilder(forge),
        callableSignature(nullptr) {
    }
    virtual void setDescription(const String& description, int precedence) override {
      this->getTaggableBuilder().setDescription(description, precedence);
    }
    virtual void setCallable(const IFunctionSignature& signature) override {
      this->callableSignature = &signature;
    }
    virtual void setIndexable(const Type& rtype, const Type& itype, Accessability accessability) override {
      auto& ib = this->getIndexBuilder();
      ib.setResultType(rtype);
      if (itype != nullptr) {
        ib.setIndexType(itype);
      }
      ib.setAccessability(accessability);
    }
    virtual void setPointable(const Type& ptype, Modifiability modifiability) override {
      auto& pb = this->getPointerBuilder();
      pb.setPointeeType(ptype);
      pb.setModifiability(modifiability);
    }
    virtual void setUnknownProperty(const Type& type, Accessability accessability) override {
      this->getPropertyBuilder().setUnknownProperty(type, accessability);
    }
    virtual void addProperty(const String& name, const Type& type, Accessability accessability) override {
      this->getPropertyBuilder().addProperty(name, type, accessability);
    }
    virtual TypeShape build(const Type& infratype) override;
    ITypeForgeIndexBuilder& getIndexBuilder();
    ITypeForgePointerBuilder& getPointerBuilder();
    ITypeForgePropertyBuilder& getPropertyBuilder();
    ITypeForgeTaggableBuilder& getTaggableBuilder();
  };

  class TypeForgeDefault : public HardReferenceCountedAllocator<ITypeForge> {
    TypeForgeDefault(const TypeForgeDefault&) = delete;
    TypeForgeDefault& operator=(const TypeForgeDefault&) = delete;
  private:
    HardPtr<IBasket> basket;
    std::set<const ICollectable*> owned;
    TypeForgeCacheSet<TypeForgeShape> cacheShape;
    TypeForgeCacheSet<TypeForgeFunctionSignatureParameter> cacheFunctionSignatureParameter;
    TypeForgeCacheSet<TypeForgeFunctionSignature> cacheFunctionSignature;
    TypeForgeCacheSet<TypeForgePropertySignature> cachePropertySignature;
    TypeForgeCacheSet<TypeForgeIndexSignature> cacheIndexSignature;
    TypeForgeCacheSet<TypeForgeIteratorSignature> cacheIteratorSignature;
    TypeForgeCacheSet<TypeForgePointerSignature> cachePointerSignature;
    TypeForgeCacheSet<TypeForgeTaggableSignature> cacheTaggableSignature;
    TypeForgeCacheMap<TypeForgeComplex::Detail, TypeForgeComplex> cacheComplex;
    TypeForgeCacheMap<IType, const IType::Shape> cacheMetashape;
    TypeShape infrashapeObject;
    TypeShape infrashapeString;
    TypeShape metashapeType;
    TypeShape metashapeObject;
    TypeShape metashapeString;
  public:
    TypeForgeDefault(IAllocator& allocator, IBasket& basket)
      : HardReferenceCountedAllocator(allocator),
        basket(&basket),
        infrashapeObject(makeInfrashapeObject()),
        infrashapeString(makeInfrashapeString()),
        metashapeType(makeMetashapeType()),
        metashapeObject(makeMetashapeObject()),
        metashapeString(makeMetashapeString()) {
    }
    IAllocator& getAllocator() const {
      return this->allocator;
    }
    virtual ~TypeForgeDefault() override {
      for (auto* instance : this->owned) {
        this->destroy(instance);
      }
    }
    virtual TypeShape forgeArrayShape(const Type& elementType, Accessability accessability) override {
      TypeForgeShape shape;
      {
        // Properties
        auto builder = this->createPropertyBuilder();
        auto lengthAccessability = Bits::hasAnySet(accessability, Accessability::Del) ? (Accessability::Get | Accessability::Set | Accessability::Mut) : Accessability::Get;
        builder->addProperty(StringBuilder::concat(this->allocator, "length"), Type::Int, lengthAccessability);
        shape.dotable = &builder->build();
      }
      {
        // Indexing
        auto builder = this->createIndexBuilder();
        builder->setResultType(elementType);
        builder->setAccessability(accessability);
        shape.indexable = &builder->build();
      }
      {
        // Iteration
        auto builder = this->createIteratorBuilder();
        builder->setIterationType(elementType);
        shape.iterable = &builder->build();
      }
      {
        // Taggable
        auto builder = this->createTaggableBuilder();
        builder->setDescription(this->typeSuffix(elementType, "[]"), 1);
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
        builder->addProperty(StringBuilder::concat(this->allocator, "length"), Type::Int, Accessability::Get);
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
    virtual Type forgeComplexType(ValueFlags flags, const TypeShapeSet& shapeset, const HardPtr<IVMTypeSpecification>& specification) override {
      if (shapeset.empty()) {
        if (flags == ValueFlags::None) {
          return nullptr;
        }
        return this->forgePrimitiveType(flags);
      }
      TypeForgeComplex::Detail detail{ flags, shapeset, specification };
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
    virtual Type forgeArrayType(const Type& elementType, Accessability accessability) override {
      return this->forgeShapeType(this->forgeArrayShape(elementType, accessability));
    }
    virtual Type forgeIterationType(const Type& container) override {
      // TODO optimize
      auto builder = this->createComplexBuilder();
      auto flags = container->getPrimitiveFlags();
      if (Bits::hasAnySet(flags, ValueFlags::Object)) {
        builder->addFlags(ValueFlags::AnyQ);
      }
      if (Bits::hasAnySet(flags, ValueFlags::String)) {
        builder->addFlags(ValueFlags::String);
      }
      size_t count = container->getShapeCount();
      for (size_t index = 0; index < count; ++index) {
        auto* shape = container->getShape(index);
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
    virtual Type forgeShapeType(const TypeShape& shape, IVMTypeSpecification* specification = nullptr) override {
      assert(shape.validate());
      assert(shape->taggable != nullptr);
      TypeShapeSet shapeset;
      shapeset.add(shape);
      TypeForgeComplex::Detail detail{ ValueFlags::None, shapeset, HardPtr(specification) };
      return this->forgeComplex(std::move(detail));
    }
    virtual Assignability isTypeAssignable(const Type& dst, const Type& src) override {
      return this->computeTypeAssignability(dst, src);
    }
    virtual Mutatability isTypeMutatable(const Type& dst, ValueMutationOp op, const Type& src) override {
      return this->computeTypeMutatability(dst, op, src);
    }
    virtual Assignability isFunctionSignatureAssignable(const IFunctionSignature& dst, const IFunctionSignature& src) override {
      return this->computeFunctionSignatureAssignability(&dst, &src);
    }
    virtual size_t foreachCallable(const Type& type, const std::function<bool(const IFunctionSignature&)>& callback) override {
      assert(type.validate());
      size_t visited = 0;
      bool completed = false;
      auto flags = type->getPrimitiveFlags();
      if (Bits::hasAnySet(flags, ValueFlags::Object)) {
        ++visited;
        completed = callback(*this->infrashapeObject->callable);
      }
      size_t index = 0;
      for (auto* shape = type->getShape(index); !completed && (shape != nullptr); shape = type->getShape(++index)) {
        if (shape->callable != nullptr) {
          ++visited;
          completed = callback(*shape->callable);
        }
      }
      return visited;
    }
    virtual size_t foreachDotable(const Type& type, const std::function<bool(const IPropertySignature&)>& callback) override {
      assert(type.validate());
      size_t visited = 0;
      bool completed = false;
      auto flags = type->getPrimitiveFlags();
      if (Bits::hasAnySet(flags, ValueFlags::Object)) {
        ++visited;
        completed = callback(*this->infrashapeObject->dotable);
      }
      if (!completed && Bits::hasAnySet(flags, ValueFlags::String)) {
        ++visited;
        completed = callback(*this->infrashapeString->dotable);
      }
      size_t index = 0;
      for (auto* shape = type->getShape(index); !completed && (shape != nullptr); shape = type->getShape(++index)) {
        if (shape->dotable != nullptr) {
          ++visited;
          completed = callback(*shape->dotable);
        }
      }
      return visited;
    }
    virtual size_t foreachIndexable(const Type& type, const std::function<bool(const IIndexSignature&)>& callback) override {
      assert(type.validate());
      size_t visited = 0;
      bool completed = false;
      auto flags = type->getPrimitiveFlags();
      if (Bits::hasAnySet(flags, ValueFlags::Object)) {
        ++visited;
        completed = callback(*this->infrashapeObject->indexable);
      }
      if (!completed && Bits::hasAnySet(flags, ValueFlags::String)) {
        ++visited;
        completed = callback(*this->infrashapeString->indexable);
      }
      size_t index = 0;
      for (auto* shape = type->getShape(index); !completed && (shape != nullptr); shape = type->getShape(++index)) {
        if (shape->indexable != nullptr) {
          ++visited;
          completed = callback(*shape->indexable);
        }
      }
      return visited;
    }
    virtual size_t foreachPointable(const Type& type, const std::function<bool(const IPointerSignature&)>& callback) override {
      assert(type.validate());
      size_t visited = 0;
      bool completed = false;
      auto flags = type->getPrimitiveFlags();
      if (Bits::hasAnySet(flags, ValueFlags::Object)) {
        ++visited;
        completed = callback(*this->infrashapeObject->pointable);
      }
      size_t index = 0;
      for (auto* shape = type->getShape(index); !completed && (shape != nullptr); shape = type->getShape(++index)) {
        if (shape->pointable != nullptr) {
          ++visited;
          completed = callback(*shape->pointable);
        }
      }
      return visited;
    }
    virtual const IType::Shape* getMetashape(const Type& infratype) override {
      return this->cacheMetashape.find(*infratype);
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
    virtual HardPtr<ITypeForgeMetashapeBuilder> createMetashapeBuilder() override {
      return this->createBuilder<TypeForgeMetashapeBuilder>();
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
    TypeShape forgeMetashape(const Type& infratype, TypeForgeShape&& metashape) {
      auto& forged = this->cacheShape.fetch(std::move(metashape));
      if (infratype == nullptr) {
        return TypeShape(forged);
      }
      return TypeShape(this->cacheMetashape.add(*infratype, forged));
    }
    template<typename T, typename... ARGS>
    HardPtr<T> createBuilder(ARGS&&... args) {
      return HardPtr(this->allocator.makeRaw<T>(*this, std::forward<ARGS>(args)...));
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
      // TODO more compatability checks?
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
        auto dsti = dst->getIndexType();
        auto srci = src->getIndexType();
        if (dsti == nullptr) {
          // Destination is an array; source must also be an array
          if (srci != nullptr) {
            return Assignability::Never;
          }
        } else {
          // Destination is a map; source must also be a map
          if (srci == nullptr) {
            return Assignability::Never;
          }
          retval = assignabilityIntersection(retval, this->computeTypeAssignability(dsti, srci));
        }
        retval = assignabilityIntersection(retval, this->computeTypeAssignability(dst->getResultType(), src->getResultType()));
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
        retval = assignabilityIntersection(retval, this->computeTypeAssignability(dst->getIterationType(), src->getIterationType()));
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
    Mutatability computeTypeMutatability(const Type& dst, ValueMutationOp op, const Type& src) {
      auto fdst = dst->getPrimitiveFlags();
      auto fsrc = src->getPrimitiveFlags();
      switch (op) {
      case ValueMutationOp::Assign: // dst = src
        switch (this->computeTypeAssignability(dst, src)) {
        case Assignability::Always:
          return Mutatability::Always;
        case Assignability::Sometimes:
          return Mutatability::Sometimes;
        case Assignability::Never:
        default:
          break;
        }
        return Mutatability::NeverLeft;
      case ValueMutationOp::Decrement: // --dst
      case ValueMutationOp::Increment: // ++dst
        if (Bits::hasNoneSet(fdst, ValueFlags::Int)) {
          return Mutatability::NeverLeft;
        }
        if (fsrc != ValueFlags::Void) {
          return Mutatability::NeverRight;
        }
        if (fdst != ValueFlags::Int) {
          return Mutatability::Sometimes;
        }
        return Mutatability::Always;
      case ValueMutationOp::Add: // dst += src
      case ValueMutationOp::Subtract: // dst -= src
      case ValueMutationOp::Multiply: // dst *= src
      case ValueMutationOp::Divide: // dst /= src
      case ValueMutationOp::Remainder: // dst %= src
      case ValueMutationOp::Minimum: // dst <|= src
      case ValueMutationOp::Maximum: // dst >|= src
        if (Bits::hasAnySet(fdst, ValueFlags::Float)) {
          // Support int-to-float promotion
          if (Bits::hasNoneSet(fsrc, ValueFlags::Arithmetic)) {
            return Mutatability::NeverRight;
          }
          if (Bits::hasAnySet(Bits::clear(fsrc, ValueFlags::Arithmetic)) || Bits::hasAnySet(Bits::clear(fdst, ValueFlags::Arithmetic))) {
            return Mutatability::Sometimes;
          }
          return Mutatability::Always;
        }
        if (Bits::hasAnySet(fdst, ValueFlags::Int)) {
          if (Bits::hasNoneSet(fsrc, ValueFlags::Int)) {
            return Mutatability::NeverRight;
          }
          if ((fsrc != ValueFlags::Int) || (fdst != ValueFlags::Int)) {
            return Mutatability::Sometimes;
          }
          return Mutatability::Always;
        }
        return Mutatability::NeverLeft;
      case ValueMutationOp::BitwiseAnd: // dst &= src
      case ValueMutationOp::BitwiseOr: // dst |= src
      case ValueMutationOp::BitwiseXor: // dst ^= src
        if (Bits::hasAnySet(fdst, ValueFlags::Bool) && Bits::hasAnySet(fsrc, ValueFlags::Bool)) {
          if ((fsrc != ValueFlags::Bool) || (fdst != ValueFlags::Bool)) {
            return Mutatability::Sometimes;
          }
          return Mutatability::Always;
        }
        if (Bits::hasAnySet(fdst, ValueFlags::Int) && Bits::hasAnySet(fsrc, ValueFlags::Int)) {
          if ((fsrc != ValueFlags::Int) || (fdst != ValueFlags::Int)) {
            return Mutatability::Sometimes;
          }
          return Mutatability::Always;
        }
        if (!Bits::hasAnySet(fdst, ValueFlags::Bool | ValueFlags::Int)) {
          return Mutatability::NeverLeft;
        }
        return Mutatability::NeverRight;
      case ValueMutationOp::ShiftLeft: // dst <<= src
      case ValueMutationOp::ShiftRight: // dst >>= src
      case ValueMutationOp::ShiftRightUnsigned: // dst >>>= src
        if (Bits::hasAnySet(fdst, ValueFlags::Int)) {
          if (Bits::hasNoneSet(fsrc, ValueFlags::Int)) {
            return Mutatability::NeverRight;
          }
          if ((fsrc != ValueFlags::Int) || (fdst != ValueFlags::Int)) {
            return Mutatability::Sometimes;
          }
          return Mutatability::Always;
        }
        return Mutatability::NeverLeft;
      case ValueMutationOp::IfVoid: // dst !!= src
        // We can always be uninitialized
        return Mutatability::Always;
      case ValueMutationOp::IfNull: // dst ??= src
        if (!Bits::hasAnySet(fdst, ValueFlags::Null)) {
          return Mutatability::Unnecessary;
        }
        return Mutatability::Always;
      case ValueMutationOp::IfFalse: // dst ||= src
      case ValueMutationOp::IfTrue: // dst &&= src
        if (!Bits::hasAnySet(fdst, ValueFlags::Bool)) {
          return Mutatability::NeverLeft;
        }
        if (!Bits::hasAnySet(fsrc, ValueFlags::Bool)) {
          return Mutatability::NeverRight;
        }
        if (fsrc != ValueFlags::Bool) {
          return Mutatability::Sometimes;
        }
        return Mutatability::Always;
      case ValueMutationOp::Noop:
        break;
      }
      return Mutatability::Unnecessary;
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
    struct FunctionBuilder {
      TypeForgeDefault* forge;
      HardPtr<ITypeForgeFunctionBuilder> builder;
      FunctionBuilder(TypeForgeDefault* forge, const char* fname, const Type& rtype)
        : forge(forge),
          builder(forge->createFunctionBuilder()) {
        this->builder->setFunctionName(this->forge->makeASCII(fname));
        this->builder->setReturnType(rtype);
      }
      FunctionBuilder& addOptionalParameter(const char* pname, const Type& ptype) {
        this->builder->addOptionalParameter(this->forge->makeASCII(pname), ptype);
        return *this;
      }
      FunctionBuilder& addRequiredParameter(const char* pname, const Type& ptype) {
        this->builder->addRequiredParameter(this->forge->makeASCII(pname), ptype);
        return *this;
      }
    };
    struct BaseBuilder {
      TypeForgeDefault* forge;
      HardPtr<ITypeForgeMetashapeBuilder> builder;
      explicit BaseBuilder(TypeForgeDefault* forge)
        : forge(forge),
          builder(forge->createMetashapeBuilder()) {
        assert(this->forge != nullptr);
        assert(this->builder != nullptr);
      }
      void addPropertyData(const char* pname, const Type& ptype, Accessability paccessability = Accessability::Get) {
        this->builder->addProperty(this->forge->makeASCII(pname), ptype, paccessability);
      }
      void addPropertyFunction(const FunctionBuilder& fbuilder, Accessability paccessability = Accessability::Get) {
        auto& psignature = fbuilder.builder->build();
        auto ptype = this->forge->forgeFunctionType(psignature);
        this->builder->addProperty(psignature.getName(), ptype, paccessability);
      }
    };
    struct InfrashapeBuilder : public BaseBuilder {
      explicit InfrashapeBuilder(TypeForgeDefault* forge)
        : BaseBuilder(forge) {
      }
      void setUnknownProperty(const Type& ptype, Accessability paccessability) {
        this->builder->setUnknownProperty(ptype, paccessability);
      }
      void setCallable(const FunctionBuilder& fbuilder) {
        this->builder->setCallable(fbuilder.builder->build());
      }
      void setIndexable(const Type& rtype, const Type& itype, Accessability paccessability) {
        this->builder->setIndexable(rtype, itype, paccessability);
      }
      void setPointable(const Type& ptype, Modifiability pmodifiability) {
        this->builder->setPointable(ptype, pmodifiability);
      }
      TypeShape build() {
        return this->builder->build(nullptr);
      }
    };
    struct MetashapeBuilder : public BaseBuilder {
      explicit MetashapeBuilder(TypeForgeDefault* forge)
        : BaseBuilder(forge) {
      }
      TypeShape build(const Type& infratype) {
        assert(infratype.validate());
        return this->builder->build(infratype);
      }
    };
    struct TypeBuilder : public BaseBuilder {
      explicit TypeBuilder(TypeForgeDefault* forge)
        : BaseBuilder(forge) {
      }
      Type build(const char* description, int precedence = 0) {
        assert(description != nullptr);
        this->builder->setDescription(this->forge->makeASCII(description), precedence);
        auto shape = this->builder->build(nullptr);
        return this->forge->forgeShapeType(shape);
      }
    };
    FunctionBuilder function(const char* fname, const Type& rtype) {
      return FunctionBuilder{ this, fname, rtype };
    }
    TypeShape makeInfrashapeObject() {
      InfrashapeBuilder ib{ this };
      // TODO add variadic parameters
      ib.setCallable(this->function("WIBBLE", Type::AnyQV));
      ib.setIndexable(Type::AnyQ, Type::AnyQ, Accessability::All);
      ib.setPointable(Type::AnyQ, Modifiability::All);
      ib.setUnknownProperty(Type::AnyQ, Accessability::All);
      return ib.build();
    }
    TypeShape makeInfrashapeString() {
      InfrashapeBuilder ib{ this };
      ib.setIndexable(Type::String, Type::Int, Accessability::Get);
      ib.addPropertyData("length", Type::Int);
      return ib.build();
    }
    TypeShape makeMetashapeType() {
      MetashapeBuilder mb{ this };
      mb.addPropertyFunction(this->function("of", Type::String)
        .addRequiredParameter("value", Type::AnyQV));
      return mb.build(Type::Type_);
    }
    TypeShape makeMetashapeObject() {
      MetashapeBuilder mb{ this };
      mb.addPropertyData("index", this->makeTypeObjectIndex()); // WIBBLE
      mb.addPropertyData("property", this->makeTypeObjectProperty()); // WIBBLE
      return mb.build(Type::Object);
    }
    Type makeTypeObjectIndex() {
      TypeBuilder tb{ this };
      tb.addPropertyFunction(this->function("get", Type::AnyQ)
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("index", Type::AnyQ));
      tb.addPropertyFunction(this->function("set", Type::Void)
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("index", Type::AnyQ)
        .addRequiredParameter("value", Type::AnyQ));
      tb.addPropertyFunction(this->function("mut", Type::AnyQV)
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("index", Type::AnyQ)
        .addRequiredParameter("value", Type::AnyQV)
        .addRequiredParameter("mutation", Type::String));
      tb.addPropertyFunction(this->function("ref", this->forgePointerType(Type::AnyQ, Modifiability::All))
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("index", Type::AnyQ));
      tb.addPropertyFunction(this->function("del", Type::AnyQV)
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("index", Type::AnyQ));
      return tb.build("object.Index");
    }
    Type makeTypeObjectProperty() {
      TypeBuilder tb{ this };
      tb.addPropertyFunction(this->function("get", Type::AnyQ)
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("property", Type::String));
      tb.addPropertyFunction(this->function("set", Type::Void)
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("property", Type::String)
        .addRequiredParameter("value", Type::AnyQ));
      tb.addPropertyFunction(this->function("mut", Type::AnyQV)
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("property", Type::String)
        .addRequiredParameter("value", Type::AnyQV)
        .addRequiredParameter("mutation", Type::String));
      tb.addPropertyFunction(this->function("ref", this->forgePointerType(Type::AnyQ, Modifiability::All))
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("property", Type::String));
      tb.addPropertyFunction(this->function("del", Type::AnyQV)
        .addRequiredParameter("instance", Type::Object)
        .addRequiredParameter("property", Type::String));
      return tb.build("object.Property");
    }
    TypeShape makeMetashapeString() {
      // TODO
      MetashapeBuilder mb{ this };
      mb.addPropertyFunction(this->function("fromCodePoints", Type::String)
        .addOptionalParameter("codepoint", Type::Int));
      return mb.build(Type::String);
    }
    String makeASCII(const char* ascii) const {
      assert(ascii != nullptr);
      return String::fromUTF8(this->allocator, ascii);
    }
  };

  template<typename T>
  void TypeForgeBaseBuilder<T>::hardDestroy() const {
    assert(this->atomic.get() == 0);
    this->forge.destroy(this);
  }

  const IFunctionSignature& TypeForgeFunctionBuilder::build() {
    assert(this->rtype != nullptr);
    TypeForgeFunctionSignature signature(this->rtype, this->fname);
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
    return this->forge.forgeComplexType(this->flags, this->shapeset, this->specification);
  }

  ITypeForgeIndexBuilder& TypeForgeMetashapeBuilder::getIndexBuilder() {
    if (this->indexBuilder == nullptr) {
      this->indexBuilder = this->forge.createIndexBuilder();
    }
    return *this->indexBuilder;
  }

  ITypeForgePointerBuilder& TypeForgeMetashapeBuilder::getPointerBuilder() {
    if (this->pointerBuilder == nullptr) {
      this->pointerBuilder = this->forge.createPointerBuilder();
    }
    return *this->pointerBuilder;
  }

  ITypeForgePropertyBuilder& TypeForgeMetashapeBuilder::getPropertyBuilder() {
    if (this->propertyBuilder == nullptr) {
      this->propertyBuilder = this->forge.createPropertyBuilder();
    }
    return *this->propertyBuilder;
  }

  ITypeForgeTaggableBuilder& TypeForgeMetashapeBuilder::getTaggableBuilder() {
    if (this->taggableBuilder == nullptr) {
      this->taggableBuilder = this->forge.createTaggableBuilder();
    }
    return *this->taggableBuilder;
  }

  TypeShape TypeForgeMetashapeBuilder::build(const Type& infratype) {
    if (infratype != nullptr) {
      StringBuilder sb;
      infratype->print(sb);
      sb << "::manifestation"; // WIBBLE
      this->getTaggableBuilder().setDescription(sb.build(this->forge.getAllocator()), 2);
    }
    TypeForgeShape metashape;
    metashape.callable = this->callableSignature;
    if (this->indexBuilder != nullptr) {
      metashape.indexable = &this->indexBuilder->build();
    }
    if (this->pointerBuilder != nullptr) {
      metashape.pointable = &this->pointerBuilder->build();
    }
    if (this->propertyBuilder != nullptr) {
      metashape.dotable = &this->propertyBuilder->build();
    }
    if (this->taggableBuilder != nullptr) {
      metashape.taggable = &this->taggableBuilder->build();
    }
    return this->forge.forgeMetashape(infratype, std::move(metashape));
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
const Type Type::AnyQV{ TypeForgePrimitive::forge(ValueFlags::AnyQV) };
const Type Type::Type_{ TypeForgePrimitive::forge(ValueFlags::Type) };

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
