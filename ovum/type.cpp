#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/node.h"
#include "ovum/vanilla.h"

namespace {
  using namespace egg::ovum;

  // An omni function looks like this: 'any?(...any?[])'
  class OmniFunctionSignature : public IFunctionSignature {
    OmniFunctionSignature(const OmniFunctionSignature&) = delete;
    OmniFunctionSignature& operator=(const OmniFunctionSignature&) = delete;
  private:
    class Parameter : public IFunctionSignatureParameter {
    public:
      virtual String getName() const override {
        return "params";
      }
      virtual Type getType() const override {
        return Type::AnyQ;
      }
      virtual size_t getPosition() const override {
        return 0;
      }
      virtual Flags getFlags() const override {
        return Flags::Variadic;
      }
    };
    Parameter params;
  public:
    OmniFunctionSignature() {}
    virtual String getFunctionName() const override {
      return String();
    }
    virtual Type getReturnType() const override {
      return Type::AnyQ;
    }
    virtual size_t getParameterCount() const override {
      return 1;
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t) const override {
      return this->params;
    }
  };
  const OmniFunctionSignature omniFunctionSignature{};

  const char* flagsComponent(ValueFlags flags) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
      switch (flags) {
      case ValueFlags::None:
        return "var";
#define EGG_OVUM_VALUE_FLAGS_COMPONENT(name, text) case ValueFlags::name: return text;
        EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_COMPONENT)
#undef EGG_OVUM_VALUE_FLAGS_COMPONENT
      case ValueFlags::Any:
        return "any";
      }
    EGG_WARNING_SUPPRESS_SWITCH_END();
      return nullptr;
  }

  std::string flagsToString(ValueFlags flags) {
    auto* component = flagsComponent(flags);
    if (component != nullptr) {
      return component;
    }
    if (Bits::hasAnySet(flags, ValueFlags::Null)) {
      return flagsToString(Bits::clear(flags, ValueFlags::Null)) + "?";
    }
    auto head = Bits::topmost(flags);
    assert(head != ValueFlags::None);
    component = flagsComponent(head);
    assert(component != nullptr);
    return flagsToString(Bits::clear(flags, head)) + '|' + component;
  }

  std::pair<std::string, int> flagsToStringPrecedence(ValueFlags flags) {
    // Adjust the precedence of the result if the string has '|' (union) in it
    // This is so that parentheses can be added appropriately to handle "(void|int)*" versus "void|int*"
    auto result = std::make_pair(flagsToString(flags), 0);
    if (result.first.find('|') != std::string::npos) {
      result.second--;
    }
    return result;
  }

  Assignability assignableFlags(ValueFlags lhs, ValueFlags rhs) {
    if (Bits::hasAllSet(lhs, rhs)) {
      return Assignability::Always;
    }
    if (Bits::hasAnySet(lhs, rhs)) {
      return Assignability::Sometimes;
    }
    if (Bits::hasAnySet(lhs, ValueFlags::Float) && Bits::hasAnySet(rhs, ValueFlags::Int)) {
      // Float<-Int promotion
      return Assignability::Sometimes;
    }
    return Assignability::Never;
  }

  class TypeBase : public IType {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  public:
    TypeBase() = default;
    virtual const IFunctionSignature* queryCallable() const override {
      // By default, we are not callable
      return nullptr;
    }
    virtual const IPropertySignature* queryDotable() const override {
      // By default, we do not support properties
      return nullptr;
    }
    virtual const IIndexSignature* queryIndexable() const override {
      // By default, we are not indexable
      return nullptr;
    }
    virtual const IIteratorSignature* queryIterable() const override {
      // By default, we are not iterable
      return nullptr;
    }
    virtual const IPointerSignature* queryPointable() const override {
      // By default, we do not support dereferencing
      return nullptr;
    }
    virtual String describeValue() const override {
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
    std::string str() const {
      return this->toStringPrecedence().first;
    }
  };

  template<ValueFlags FLAGS>
  class TypeCommon : public NotSoftReferenceCounted<TypeBase> {
    TypeCommon(const TypeCommon&) = delete;
    TypeCommon& operator=(const TypeCommon&) = delete;
  public:
    TypeCommon() : NotSoftReferenceCounted() {}
    virtual ValueFlags getFlags() const override {
      return FLAGS;
    }
    virtual Assignability queryAssignable(const IType& rhs) const override {
      return assignableFlags(FLAGS, rhs.getFlags());
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return flagsToStringPrecedence(FLAGS);
    }
  };
  const TypeCommon<ValueFlags::Bool> typeBool{};
  const TypeCommon<ValueFlags::Int> typeInt{};
  const TypeCommon<ValueFlags::Float> typeFloat{};
  const TypeCommon<ValueFlags::Int | ValueFlags::Float> typeArithmetic{};

  class TypeVoid : public TypeCommon<ValueFlags::Void> {
    TypeVoid(const TypeVoid&) = delete;
    TypeVoid& operator=(const TypeVoid&) = delete;
  public:
    TypeVoid() = default;
    virtual Assignability queryAssignable(const IType&) const override {
      return Assignability::Never;
    }
  };
  const TypeVoid typeVoid{};

  class TypeNull : public TypeCommon<ValueFlags::Null> {
    TypeNull(const TypeNull&) = delete;
    TypeNull& operator=(const TypeNull&) = delete;
  public:
    TypeNull() = default;
  };
  const TypeNull typeNull{};

  class TypeString : public TypeCommon<ValueFlags::String> {
    TypeString(const TypeString&) = delete;
    TypeString& operator=(const TypeString&) = delete;
  private:
    class IndexSignature : public IIndexSignature {
    public:
      virtual Type getResultType() const override {
        return Type::String;
      }
      virtual Type getIndexType() const override {
        return Type::Int;
      }
      virtual Modifiability getModifiability() const override {
        return Modifiability::Read;
      }
    };
    class IteratorSignature : public IIteratorSignature {
    public:
      virtual Type getType() const override {
        return Type::String;
      }
    };
    class PropertySignature : public IPropertySignature {
    public:
      virtual Type getType(const String& property) const override {
        return ValueFactory::queryBuiltinStringPropertyType(property);
      }
      virtual Modifiability getModifiability(const String& property) const override {
        return ValueFactory::queryBuiltinStringPropertyModifiability(property);
      }
      virtual String getName(size_t index) const override {
        return ValueFactory::queryBuiltinStringPropertyName(index);
      }
    };
  public:
    TypeString() = default;
    virtual const IPropertySignature* queryDotable() const override {
      static const PropertySignature propertySignature{};
      return &propertySignature;
    }
    virtual const IIndexSignature* queryIndexable() const override {
      static const IndexSignature indexSignature{};
      return &indexSignature;
    }
    virtual const IIteratorSignature* queryIterable() const override {
      static const IteratorSignature iteratorSignature{};
      return &iteratorSignature;
    }
  };
  const TypeString typeString{};

  class TypeAny : public TypeCommon<ValueFlags::Any> {
    TypeAny(const TypeAny&) = delete;
    TypeAny& operator=(const TypeAny&) = delete;
  public:
    TypeAny() = default;
    virtual const IFunctionSignature* queryCallable() const override {
      return &omniFunctionSignature;
    }
  };
  const TypeAny typeAny{};

  class TypeAnyQ : public TypeCommon<ValueFlags::AnyQ> {
    TypeAnyQ(const TypeAnyQ&) = delete;
    TypeAnyQ& operator=(const TypeAnyQ&) = delete;
  public:
    TypeAnyQ() = default;
    virtual const IFunctionSignature* queryCallable() const override {
      return &omniFunctionSignature;
    }
  };
  const TypeAnyQ typeAnyQ{};

  class TypeUnion : public SoftReferenceCounted<TypeBase> {
    TypeUnion(const TypeUnion&) = delete;
    TypeUnion& operator=(const TypeUnion&) = delete;
  private:
    Type a;
    Type b;
  public:
    TypeUnion(IAllocator& allocator, const IType& a, const IType& b)
      : SoftReferenceCounted(allocator), a(&a), b(&b) {
    }
    virtual void softVisitLinks(const Visitor& visitor) const {
      this->a->softVisitLinks(visitor);
      this->b->softVisitLinks(visitor);
    }
    virtual ValueFlags getFlags() const override {
      return this->a->getFlags() | this->b->getFlags();
    }
    virtual Assignability queryAssignable(const IType& rhs) const override {
      // The logic is:
      //  Assignable to a Assignable to b Result
      //  =============== =============== ======
      //  Readonly        Readonly        Readonly
      //  Readonly        Never           Never
      //  Readonly        Sometimes       Sometimes
      //  Readonly        Always          Always
      //  Never           Readonly        Never
      //  Never           Never           Never
      //  Never           Sometimes       Sometimes
      //  Never           Always          Always
      //  Sometimes       Readonly        Sometimes
      //  Sometimes       Never           Sometimes
      //  Sometimes       Sometimes       Sometimes
      //  Sometimes       Always          Always
      //  Always          Readonly        Always
      //  Always          Never           Always
      //  Always          Sometimes       Always
      //  Always          Always          Always
      auto qa = this->a->queryAssignable(rhs);
      if (qa == Assignability::Always) {
        return Assignability::Always;
      }
      auto qb = this->b->queryAssignable(rhs);
      if (qb == Assignability::Always) {
        return Assignability::Always;
      }
      if ((qa == Assignability::Sometimes) || (qb == Assignability::Sometimes)) {
        return Assignability::Sometimes;
      }
      return (qa == Assignability::Readonly) ? qb : qa;
    }
    virtual const IFunctionSignature* queryCallable() const override {
      // TODO What if both have signatures?
      auto* signature = this->a->queryCallable();
      if (signature != nullptr) {
        return signature;
      }
      return this->b->queryCallable();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // TODO
      auto ab = StringBuilder().add(this->a.toString(0), '|', this->b.toString(0)).toUTF8();
      return std::make_pair(ab, 0);
    }
  };

  class TypeSimple : public SoftReferenceCounted<TypeBase> {
    TypeSimple(const TypeSimple&) = delete;
    TypeSimple& operator=(const TypeSimple&) = delete;
  private:
    ValueFlags flags;
  public:
    TypeSimple(IAllocator& allocator, ValueFlags flags)
      : SoftReferenceCounted(allocator), flags(flags) {
    }
    virtual void softVisitLinks(const Visitor&) const {
      // Nothing to visit
    }
    virtual ValueFlags getFlags() const override {
      return this->flags;
    }
    virtual Assignability queryAssignable(const IType& rhs) const override {
      return assignableFlags(this->flags, rhs.getFlags());
    }
    virtual const IFunctionSignature* queryCallable() const override {
      if (Bits::hasAnySet(this->flags, ValueFlags::Object)) {
        return &omniFunctionSignature;
      }
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return flagsToStringPrecedence(this->flags);
    }
  };

  class TypePointer : public SoftReferenceCounted<TypeBase> {
    TypePointer(const TypePointer&) = delete;
    TypePointer& operator=(const TypePointer&) = delete;
  private:
    Type referenced; // WOBBLE soft?
  public:
    TypePointer(IAllocator& allocator, const IType& referenced)
      : SoftReferenceCounted(allocator), referenced(&referenced) {
    }
    virtual void softVisitLinks(const Visitor& visitor) const {
      this->referenced->softVisitLinks(visitor);
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual Assignability queryAssignable(const IType&) const override {
      return Assignability::Readonly; // WIBBLE unimplemented
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto p = StringBuilder().add(this->referenced.toString(0), '*').toUTF8();
      return std::make_pair(p, 0);
    }
  };

  class FunctionSignatureParameter : public IFunctionSignatureParameter {
  private:
    String name; // may be empty
    Type type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    FunctionSignatureParameter(const String& name, const Type& type, size_t position, Flags flags)
      : name(name), type(type), position(position), flags(flags) {
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual Type getType() const override {
      return this->type;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
  };

  class FunctionSignature : public IFunctionSignature {
  private:
    String name;
    Type rettype;
    std::vector<FunctionSignatureParameter> parameters;
  public:
    FunctionSignature(const String& name, const Type& rettype)
      : name(name), rettype(rettype) {
    }
    void addSignatureParameter(const String& parameterName, const Type& parameterType, size_t position, IFunctionSignatureParameter::Flags flags) {
      this->parameters.emplace_back(parameterName, parameterType, position, flags);
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->rettype;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters.size());
      return this->parameters[index];
    }
    // Helpers
    enum class Parts {
      ReturnType = 0x01,
      FunctionName = 0x02,
      ParameterList = 0x04,
      ParameterNames = 0x08,
      NoNames = ReturnType | ParameterList,
      All = ReturnType | FunctionName | ParameterList | ParameterNames
    };
    static void build(StringBuilder& sb, const IFunctionSignature& signature, Parts parts = Parts::All) {
      // TODO better formatting of named/variadic etc.
      if (Bits::hasAnySet(parts, Parts::ReturnType)) {
        // Use precedence zero to get any necessary parentheses
        sb.add(signature.getReturnType().toString(0));
      }
      if (Bits::hasAnySet(parts, Parts::FunctionName)) {
        auto name = signature.getFunctionName();
        if (!name.empty()) {
          sb.add(' ', name);
        }
      }
      if (Bits::hasAnySet(parts, Parts::ParameterList)) {
        sb.add('(');
        auto n = signature.getParameterCount();
        for (size_t i = 0; i < n; ++i) {
          if (i > 0) {
            sb.add(", ");
          }
          auto& parameter = signature.getParameter(i);
          assert(parameter.getPosition() != SIZE_MAX);
          if (Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
            sb.add("...");
          } else {
            sb.add(parameter.getType().toString());
            if (Bits::hasAnySet(parts, Parts::ParameterNames)) {
              auto pname = parameter.getName();
              if (!pname.empty()) {
                sb.add(' ', pname);
              }
            }
            if (!Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Required)) {
              sb.add(" = null");
            }
          }
        }
        sb.add(')');
      }
    }
  };

  class FunctionBuilder : public HardReferenceCounted<ITypeBuilder>, public IFunctionSignature {
    FunctionBuilder(const FunctionBuilder&) = delete;
    FunctionBuilder& operator=(const FunctionBuilder&) = delete;
  private:
    class Parameter : public IFunctionSignatureParameter {
    private:
      Type type;
      String name;
      size_t position; // SIZE_MAX for named
      Flags flags;
    public:
      Parameter(const Type& type, const String& name, Flags flags, size_t position)
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
    class Built : public SoftReferenceCounted<IType> {
      Built(const Built&) = delete;
      Built& operator=(const Built&) = delete;
    private:
      HardPtr<FunctionBuilder> builder;
    public:
      Built(IAllocator& allocator, FunctionBuilder& builder)
        : SoftReferenceCounted(allocator),
          builder(&builder) {
      }
      virtual void softVisitLinks(const Visitor&) const {
        // WOBBLE
      }
      virtual ValueFlags getFlags() const override {
        return ValueFlags::Object;
      }
      virtual Assignability queryAssignable(const IType&) const override {
        return Assignability::Readonly; // WIBBLE unimplemented
      }
      virtual const IFunctionSignature* queryCallable() const override {
        return this->builder.get();
      }
      virtual const IPropertySignature* queryDotable() const override {
        return nullptr;
      }
      virtual const IIndexSignature* queryIndexable() const override {
        return nullptr;
      }
      virtual const IIteratorSignature* queryIterable() const override {
        return nullptr;
      }
      virtual const IPointerSignature* queryPointable() const override {
        return nullptr;
      }
      virtual std::pair<std::string, int> toStringPrecedence() const override {
        return std::pair<std::string, int>();
      }
      virtual String describeValue() const override {
        return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
      }
      std::string str() const {
        return this->toStringPrecedence().first;
      }
    };
    friend class Built;
    Type rettype;
    String name;
    std::vector<Parameter> positional;
    std::vector<Parameter> named;
    bool built;
  public:
    FunctionBuilder(IAllocator& allocator, const Type& rettype, const String& name)
      : HardReferenceCounted(allocator, 0),
        rettype(rettype),
        name(name),
        built(false) {
      assert(rettype != nullptr);
    }
    virtual void addPositionalParameter(const Type& ptype, const String& pname, IFunctionSignatureParameter::Flags pflags) {
      assert(!this->built);
      auto pindex = this->positional.size();
      this->positional.emplace_back(ptype, pname, pflags, pindex);
    }
    virtual void addNamedParameter(const Type& ptype, const String& pname, IFunctionSignatureParameter::Flags pflags) {
      assert(!this->built);
      this->named.emplace_back(ptype, pname, pflags, SIZE_MAX);
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->rettype;
    }
    virtual size_t getParameterCount() const override {
      assert(this->built);
      return this->positional.size() + this->named.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(this->built);
      if (index < this->positional.size()) {
        return this->positional[index];
      }
      return this->named.at(index - this->positional.size());
    }
    virtual Type build() override {
      assert(!this->built);
      this->built = true;
      return this->allocator.make<Built, Type>(*this);
    }
  };

  class GeneratorReturnType : public SoftReferenceCounted<TypeBase> {
    GeneratorReturnType(const GeneratorReturnType&) = delete;
    GeneratorReturnType& operator=(const GeneratorReturnType&) = delete;
  private:
    class FunctionSignature : public IFunctionSignature {
      FunctionSignature(const FunctionSignature&) = delete;
      FunctionSignature& operator=(const FunctionSignature&) = delete;
    private:
      Type rettype;
    public:
      FunctionSignature(Type rettype)
        : rettype(rettype) {
      }
      virtual String getFunctionName() const override {
        return String();
      }
      virtual Type getReturnType() const override {
        return this->rettype;
      }
      virtual size_t getParameterCount() const override {
        return 0;
      }
      virtual const IFunctionSignatureParameter& getParameter(size_t) const override {
        // Should never be called
        assert(false);
        return *static_cast<const IFunctionSignatureParameter*>(nullptr);
      }
    };
    Type gentype; // e.g. 'int' for 'int! generator()'
    FunctionSignature signature;
  public:
    GeneratorReturnType(IAllocator& allocator, const Type& gentype)
      : SoftReferenceCounted(allocator),
        gentype(gentype),
        signature(gentype) { // WOBBLE gentype->makeUnion(*Type::Void)
    }
    virtual void softVisitLinks(const Visitor&) const {
      // WOBBLE
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual const IFunctionSignature* queryCallable() const override {
      return &this->signature;
    }
    virtual const IIteratorSignature* queryIterable() const override {
      return nullptr;
    }
    virtual Assignability queryAssignable(const IType&) const override {
      return Assignability::Readonly; // WIBBLE unimplemented
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      // WIBBLE
      auto p = StringBuilder().add(this->gentype.toString(0), '!').toUTF8();
      return std::make_pair("", 0);
    }
  };
}

egg::ovum::String egg::ovum::Type::describeValue() const {
  auto* type = this->get();
  if (type == nullptr) {
    return "Value of unknown type";
  }
  return type->describeValue();
}

egg::ovum::String egg::ovum::Type::toString(int precedence) const {
  auto* type = this->get();
  if (type == nullptr) {
    return "<unknown>";
  }
  auto pair = type->toStringPrecedence();
  if (pair.second < precedence) {
    return "(" + pair.first + ")";
  }
  return pair.first;
}

egg::ovum::Type egg::ovum::TypeFactory::createSimple(IAllocator& allocator, ValueFlags flags) {
  // OPTIMIZE for known types
  return allocator.make<TypeSimple, Type>(flags);
}

egg::ovum::Type egg::ovum::TypeFactory::createPointer(IAllocator& allocator, const IType& pointee) {
  // OPTIMIZE with lazy pointer types in values instantiated when address-of is invoked
  return allocator.make<TypePointer, Type>(pointee);
}

egg::ovum::Type egg::ovum::TypeFactory::createUnion(IAllocator&, const IType& a, const IType& b) {
  if (&a == &b) {
    return Type(&a);
  }
  // WOBBLE
  return Type(&a);
}

egg::ovum::Type egg::ovum::TypeFactory::createUnionJoin(IAllocator& allocator, const IType& a, const IType& b) {
  if (&a == &b) {
    return Type(&a);
  }
  return allocator.make<TypeUnion, Type>(a, b);
}

egg::ovum::TypeBuilder egg::ovum::TypeFactory::createFunctionBuilder(IAllocator& allocator, const Type& rettype, const String& name) {
  return allocator.make<FunctionBuilder>(rettype, name);
}

egg::ovum::TypeBuilder egg::ovum::TypeFactory::createGeneratorBuilder(IAllocator& allocator, const Type& rettype, const String& name) {
  // Convert the return type (e.g. 'int') into a generator function 'int...' aka '(void|int)()'
  return allocator.make<FunctionBuilder>(allocator.make<GeneratorReturnType, Type>(rettype), name);
}

// Common types
const egg::ovum::Type egg::ovum::Type::Void{ &typeVoid };
const egg::ovum::Type egg::ovum::Type::Null{ &typeNull };
const egg::ovum::Type egg::ovum::Type::Bool{ &typeBool };
const egg::ovum::Type egg::ovum::Type::Int{ &typeInt };
const egg::ovum::Type egg::ovum::Type::Float{ &typeFloat };
const egg::ovum::Type egg::ovum::Type::String{ &typeString };
const egg::ovum::Type egg::ovum::Type::Arithmetic{ &typeArithmetic };
const egg::ovum::Type egg::ovum::Type::Any{ &typeAny };
const egg::ovum::Type egg::ovum::Type::AnyQ{ &typeAnyQ };

const egg::ovum::IFunctionSignature& egg::ovum::TypeFactory::OmniFunctionSignature{ omniFunctionSignature };
