#include "ovum/ovum.h"

#include <unordered_set>

size_t egg::ovum::Hash::hash(const Type& value) {
  return value.hash();
}

namespace {
  using namespace egg::ovum;

  class TypeForgeDefault;

  template<typename T>
  class TypeForgeCache {
    TypeForgeCache(const TypeForgeCache&) = delete;
    TypeForgeCache& operator=(const TypeForgeCache&) = delete;
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
    TypeForgeCache() {};
    const T& fetch(T&& value) {
      return *this->cache.insert(std::move(value)).first;
    }
  };

  class TypePrimitive final : public SoftReferenceCountedNone<IType> {
    TypePrimitive(const TypePrimitive&) = delete;
    TypePrimitive& operator=(const TypePrimitive&) = delete;
  private:
    static const size_t trivials = size_t(1 << int(ValueFlagsShift::UBound));
    static TypePrimitive trivial[trivials];
    Atomic<ValueFlags> flags;
    TypePrimitive()
      : flags(ValueFlags::None) {
      // Private construction used for TypePrimitive::trivial
    }
  public:
    virtual bool isPrimitive() const override {
      return true;
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return this->flags.get();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      std::stringstream ss;
      Printer printer{ ss, Print::Options::DEFAULT };
      auto precedence = printer.describe(this->flags.get());
      return std::make_pair(ss.str(), precedence);
    }
    virtual void print(Printer& printer) const override {
      (void)printer.describe(this->flags.get());
    }
    static const IType* forge(ValueFlags flags) {
      auto index = size_t(flags);
      if (index < TypePrimitive::trivials) {
        auto& entry = TypePrimitive::trivial[index];
        (void)entry.flags.exchange(flags);
        return &entry;
      }
      return nullptr;
    }
  };
  TypePrimitive TypePrimitive::trivial[] = {};

  class TypeVanillaFunction final : public SoftReferenceCountedAllocator<IType> {
    TypeVanillaFunction(const TypeVanillaFunction&) = delete;
    TypeVanillaFunction& operator=(const TypeVanillaFunction&) = delete;
  private:
    const IFunctionSignature& signature;
  public:
    TypeVanillaFunction(IAllocator& allocator, IBasket&, const IFunctionSignature& signature)
      : SoftReferenceCountedAllocator(allocator),
        signature(signature) {
    }
    virtual void softVisit(ICollectable::IVisitor&) const override {
      // Nothing to do
    }
    virtual bool isPrimitive() const override {
      return false;
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return ValueFlags::None;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // TODO
      std::ostringstream stream;
      auto options = Print::Options::DEFAULT;
      options.names = false;
      Printer printer{ stream, options };
      Type::print(printer, this->signature);
      return std::make_pair(stream.str(), 0);
    }
    virtual void print(Printer& printer) const override {
      Type::print(printer, this->signature);
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
    TypeForgeFunctionSignatureParameter(size_t position, Type type, String name, Flags flags)
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
    virtual size_t getPosition() const {
      return this->position;
    }
    virtual Type getType() const {
      return this->type;
    }
    virtual String getName() const {
      return this->name;
    }
    virtual Flags getFlags() const {
      return this->flags;
    }
    size_t cacheHash() const {
      // TODO include names?
      Hash hash;
      hash.add(this->position).add(this->type).add(this->name).add(this->flags);
      return hash;
    }
    static bool cacheEquals(const TypeForgeFunctionSignatureParameter& lhs, const TypeForgeFunctionSignatureParameter& rhs) {
      // TODO check names?
      return (lhs.position == rhs.position) && (lhs.type == rhs.type) && (lhs.name == rhs.name) && (lhs.flags == rhs.flags);
    }
  };

  class TypeForgeFunctionSignature : public IFunctionSignature {
    TypeForgeFunctionSignature(const TypeForgeFunctionSignature&) = delete;
    TypeForgeFunctionSignature& operator=(const TypeForgeFunctionSignature&) = delete;
  public:
    Type type;
    String name;
    std::vector<const IFunctionSignatureParameter*> parameters;
    TypeForgeFunctionSignature(Type type, String name)
      : type(type),
        name(name),
        parameters() {
    }
    TypeForgeFunctionSignature(TypeForgeFunctionSignature&& rhs) noexcept
      : type(std::move(rhs.type)),
        name(std::move(rhs.name)),
        parameters(std::move(rhs.parameters)) {
    }
    virtual String getName() const {
      return this->name;
    }
    virtual Type getReturnType() const {
      return this->type;
    }
    virtual size_t getParameterCount() const {
      return this->parameters.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const {
      return *this->parameters[index];
    }
    virtual Type getGeneratorType() const {
      // TODO
      return nullptr;
    }
    size_t cacheHash() const {
      // TODO include names?
      Hash hash;
      hash.add(this->type).add(this->name).add(this->parameters.begin(), this->parameters.end());
      return hash;
    }
    static bool cacheEquals(const TypeForgeFunctionSignature& lhs, const TypeForgeFunctionSignature& rhs) {
      // TODO check names?
      return (lhs.type == rhs.type) && (lhs.name == rhs.name) && (lhs.parameters == rhs.parameters);
    }
  };

  class TypeForgeFunctionBuilder : public HardReferenceCounted<ITypeForgeFunctionBuilder> {
    TypeForgeFunctionBuilder(const TypeForgeFunctionBuilder&) = delete;
    TypeForgeFunctionBuilder& operator=(const TypeForgeFunctionBuilder&) = delete;
  private:
    TypeForgeDefault& forge;
    Type rtype;
    String fname;
    std::vector<TypeForgeFunctionSignatureParameter> parameters;
  public:
    explicit TypeForgeFunctionBuilder(TypeForgeDefault& forge)
      : forge(forge) {
    }
    virtual void setFunctionName(const String& name) override {
      this->fname = name;
    }
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
  protected:
    virtual void hardDestroy() const override;
  };

  class TypeForgeDefault : public HardReferenceCountedAllocator<ITypeForge> {
    TypeForgeDefault(const TypeForgeDefault&) = delete;
    TypeForgeDefault& operator=(const TypeForgeDefault&) = delete;
  private:
    HardPtr<IBasket> basket;
    TypeForgeCache<TypeForgeFunctionSignatureParameter> cacheFunctionSignatureParameter;
    TypeForgeCache<TypeForgeFunctionSignature> cacheFunctionSignature;
  public:
    TypeForgeDefault(IAllocator& allocator, IBasket& basket)
      : HardReferenceCountedAllocator(allocator),
      basket(&basket) {
    }
    virtual Type forgePrimitiveType(ValueFlags flags) override {
      auto trivial = TypePrimitive::forge(flags);
      assert(trivial != nullptr);
      return trivial;
    }
    virtual Type forgeUnionType(const Type& lhs, const Type& rhs) override {
      assert(lhs->isPrimitive()); // TODO
      assert(rhs->isPrimitive()); // TODO
      return this->forgePrimitiveType(lhs->getPrimitiveFlags() | rhs->getPrimitiveFlags());
    }
    virtual Type forgeNullableType(const Type& type, bool nullable) override {
      assert(type->isPrimitive()); // TODO
      auto before = type->getPrimitiveFlags();
      if (nullable) {
        if (!Bits::hasAnySet(before, ValueFlags::Null)) {
          return this->forgePrimitiveType(Bits::set(before, ValueFlags::Null));
        }
      } else {
        if (Bits::hasAnySet(before, ValueFlags::Null)) {
          return this->forgePrimitiveType(Bits::clear(before, ValueFlags::Null));
        }
      }
      return type;
    }
    virtual Type forgeVoidableType(const Type& type, bool voidable) override {
      assert(type->isPrimitive()); // TODO
      auto before = type->getPrimitiveFlags();
      if (voidable) {
        if (!Bits::hasAnySet(before, ValueFlags::Void)) {
          return this->forgePrimitiveType(Bits::set(before, ValueFlags::Void));
        }
      } else {
        if (Bits::hasAnySet(before, ValueFlags::Void)) {
          return this->forgePrimitiveType(Bits::clear(before, ValueFlags::Void));
        }
      }
      return type;
    }
    virtual Type forgeFunctionType(const IFunctionSignature& signature) override {
      return this->create<TypeVanillaFunction>(this->allocator, *this->basket, signature);
    }
    virtual Assignability isTypeAssignable(const Type& dst, const Type& src) override {
      return this->computeTypeAssignability(dst, src);
    }
    virtual Assignability isFunctionSignatureAssignable(const IFunctionSignature& dst, const IFunctionSignature& src) override {
      return this->computeFunctionSignatureAssignability(&dst, &src);
    }
    virtual HardPtr<ITypeForgeFunctionBuilder> createFunctionBuilder() override {
      return HardPtr(this->create<TypeForgeFunctionBuilder>(*this));
    }
    const IFunctionSignatureParameter& forgeFunctionSignatureParameter(TypeForgeFunctionSignatureParameter&& parameter) {
      return this->cacheFunctionSignatureParameter.fetch(std::move(parameter));
    }
    const IFunctionSignature& forgeFunctionSignature(TypeForgeFunctionSignature&& signature) {
      return this->cacheFunctionSignature.fetch(std::move(signature));
    }
    template<typename T, typename... ARGS>
    T* create(ARGS&&... args) {
      return this->allocator.makeRaw<T>(std::forward<ARGS>(args)...);
    }
    template<typename T>
    void destroy(T* instance) {
      this->allocator.destroy(instance);
    }
  private:
    Assignability computeTypeAssignability(const IType* dst, const IType* src) {
      // TODO complex types
      assert(dst != nullptr);
      assert(src != nullptr);
      assert(dst->isPrimitive());
      assert(src->isPrimitive());
      auto fdst = dst->getPrimitiveFlags();
      auto fsrc = src->getPrimitiveFlags();
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
      assert(dst != nullptr);
      assert(src != nullptr);
      auto retval = Assignability::Always;
      if (dst != src) {
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
  };
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

  void TypeForgeFunctionBuilder::hardDestroy() const {
    assert(this->atomic.get() == 0);
    this->forge.destroy(this);
  }
}

// Common types
const Type Type::None{ TypePrimitive::forge(ValueFlags::None) };
const Type Type::Void{ TypePrimitive::forge(ValueFlags::Void) };
const Type Type::Null{ TypePrimitive::forge(ValueFlags::Null) };
const Type Type::Bool{ TypePrimitive::forge(ValueFlags::Bool) };
const Type Type::Int{ TypePrimitive::forge(ValueFlags::Int) };
const Type Type::Float{ TypePrimitive::forge(ValueFlags::Float) };
const Type Type::String{ TypePrimitive::forge(ValueFlags::String) };
const Type Type::Arithmetic{ TypePrimitive::forge(ValueFlags::Arithmetic) };
const Type Type::Object{ TypePrimitive::forge(ValueFlags::Object) };
const Type Type::Any{ TypePrimitive::forge(ValueFlags::Any) };
const Type Type::AnyQ{ TypePrimitive::forge(ValueFlags::AnyQ) };

void egg::ovum::Type::print(Printer& printer) const {
  auto* p = this->ptr;
  if (p == nullptr) {
    printer << "<unknown>";
  } else {
    Type::print(printer, *p);
  }
}

void egg::ovum::Type::print(Printer& printer, const IType& type) {
  if (type.isPrimitive()) {
    (void)printer.describe(type.getPrimitiveFlags());
  } else {
    printer << "<complex>";
  }
}

void egg::ovum::Type::print(Printer& printer, const IFunctionSignature& signature) {
  signature.getReturnType().print(printer);
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
