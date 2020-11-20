#include "ovum/ovum.h"
#include "ovum/slot.h"
#include "ovum/node.h"
#include "ovum/builtin.h"

#include <array>
#include <map>
#include <unordered_set>

namespace {
  using namespace egg::ovum;

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

  class Type_Shape : public NotSoftReferenceCounted<IType> {
    Type_Shape(const Type_Shape&) = delete;
    Type_Shape& operator=(const Type_Shape&) = delete;
  public:
    Type_Shape() = default;
    virtual const IntShape* getIntShape() const override {
      // By default, we are not shaped like an int
      return nullptr;
    }
    virtual const FloatShape* getFloatShape() const override {
      // By default, we are not shaped like a float
      return nullptr;
    }
    virtual const ObjectShape* getStringShape() const override {
      // By default, we are not shaped like a string
      return nullptr;
    }
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

  class Type_Void final : public Type_Shape {
    Type_Void(const Type_Void&) = delete;
    Type_Void& operator=(const Type_Void&) = delete;
  public:
    Type_Void() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Void;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "void", 0 };
    }
  };
  const Type_Void typeVoid{};

  class Type_Null final : public Type_Shape {
    Type_Null(const Type_Null&) = delete;
    Type_Null& operator=(const Type_Null&) = delete;
  public:
    Type_Null() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Null;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "null", 0 };
    }
  };
  const Type_Null typeNull{};

  class Type_Bool final : public Type_Shape {
    Type_Bool(const Type_Bool&) = delete;
    Type_Bool& operator=(const Type_Bool&) = delete;
  public:
    Type_Bool() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Bool;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "bool", 0 };
    }
  };
  const Type_Bool typeBool{};

  class Type_Int final : public Type_Shape {
    Type_Int(const Type_Int&) = delete;
    Type_Int& operator=(const Type_Int&) = delete;
  public:
    Type_Int() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Int;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::IntShape;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "int", 0 };
    }
  };
  const Type_Int typeInt{};

  class Type_Float final : public Type_Shape {
    Type_Float(const Type_Float&) = delete;
    Type_Float& operator=(const Type_Float&) = delete;
  public:
    Type_Float() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Float;
    }
    virtual const FloatShape* getFloatShape() const override {
      return &BuiltinFactory::FloatShape;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "float", 0 };
    }
  };
  const Type_Float typeFloat{};

  class Type_Arithmetic final : public Type_Shape {
    Type_Arithmetic(const Type_Arithmetic&) = delete;
    Type_Arithmetic& operator=(const Type_Arithmetic&) = delete;
  public:
    Type_Arithmetic() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Arithmetic;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::IntShape;
    }
    virtual const FloatShape* getFloatShape() const override {
      return &BuiltinFactory::FloatShape;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "int|float", 2 };
    }
  };
  const Type_Arithmetic typeArithmetic{};

  class Type_String final : public Type_Shape {
    Type_String(const Type_String&) = delete;
    Type_String& operator=(const Type_String&) = delete;
  public:
    Type_String() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::String;
    }
    virtual const ObjectShape* getStringShape() const override {
      return &BuiltinFactory::StringShape;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "string", 0 };
    }
  };
  const Type_String typeString{};

  class Type_Object final : public Type_Shape {
    Type_Object(const Type_Object&) = delete;
    Type_Object& operator=(const Type_Object&) = delete;
  public:
    Type_Object() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &BuiltinFactory::ObjectShape : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "object", 0 };
    }
  };
  const Type_Object typeObject{};

  class Type_Any final : public Type_Shape {
    Type_Any(const Type_Any&) = delete;
    Type_Any& operator=(const Type_Any&) = delete;
  public:
    Type_Any() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Any;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::IntShape;
    }
    virtual const FloatShape* getFloatShape() const override {
      return &BuiltinFactory::FloatShape;
    }
    virtual const ObjectShape* getStringShape() const override {
      return &BuiltinFactory::StringShape;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &BuiltinFactory::ObjectShape : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "any", 0 };
    }
  };
  const Type_Any typeAny{};

  class Type_AnyQ final : public Type_Shape {
    Type_AnyQ(const Type_AnyQ&) = delete;
    Type_AnyQ& operator=(const Type_AnyQ&) = delete;
  public:
    Type_AnyQ() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::AnyQ;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::IntShape;
    }
    virtual const FloatShape* getFloatShape() const override {
      return &BuiltinFactory::FloatShape;
    }
    virtual const ObjectShape* getStringShape() const override {
      return &BuiltinFactory::StringShape;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &BuiltinFactory::ObjectShape : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "any?", 1 };
    }
  };
  const Type_AnyQ typeAnyQ{};

  class TypeSimple : public SoftReferenceCounted<IType> {
    TypeSimple(const TypeSimple&) = delete;
    TypeSimple& operator=(const TypeSimple&) = delete;
  private:
    ValueFlags flags;
  public:
    TypeSimple(IAllocator& allocator, ValueFlags flags)
      : SoftReferenceCounted(allocator),
        flags(flags) {
      assert(flags != ValueFlags::None);
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // Nothing to visit
    }
    virtual ValueFlags getFlags() const override {
      return this->flags;
    }
    virtual const IntShape* getIntShape() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::Int) ? &BuiltinFactory::IntShape : nullptr;
    }
    virtual const FloatShape* getFloatShape() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::Float) ? &BuiltinFactory::FloatShape : nullptr;
    }
    virtual const ObjectShape* getStringShape() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::String) ? &BuiltinFactory::StringShape : nullptr;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return ((index == 0) && Bits::hasAnySet(this->flags, ValueFlags::Object)) ? &BuiltinFactory::ObjectShape : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::Object) ? 1u : 0u;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("WOBBLE", 0);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  class TypeUnion : public SoftReferenceCounted<IType> {
    TypeUnion(const TypeUnion&) = delete;
    TypeUnion& operator=(const TypeUnion&) = delete;
  private:
    Type a; // TODO make soft
    Type b; // TODO make soft
    ValueFlags flags;
    IntShape intShape;
    FloatShape floatShape;
    std::vector<const ObjectShape*> objectShape;
  public:
    TypeUnion(IAllocator& allocator, const IType& a, const IType& b)
      : SoftReferenceCounted(allocator),
        a(&a),
        b(&b),
        flags(a.getFlags() | b.getFlags()),
        intShape(unionInt(a.getIntShape(), b.getIntShape())),
        floatShape(unionFloat(a.getFloatShape(), b.getFloatShape())),
        objectShape(unionObject({ &a, &b })) {
      assert(this->a != nullptr);
      assert(this->b != nullptr);
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
    }
    virtual ValueFlags getFlags() const override {
      return this->flags;
    }
    virtual const IntShape* getIntShape() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::Int) ? &this->intShape : nullptr;
    }
    virtual const FloatShape* getFloatShape() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::Float) ? &this->floatShape : nullptr;
    }
    virtual const ObjectShape* getStringShape() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::String) ? &BuiltinFactory::StringShape : nullptr;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return this->objectShape.at(index);
    }
    virtual size_t getObjectShapeCount() const override {
      return this->objectShape.size();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("WOBBLE", 0);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  private:
    static IntShape unionInt(const IntShape* p, const IntShape* q) {
      if (p == nullptr) {
        if (q == nullptr) {
          return BuiltinFactory::IntShape;
        }
        return *q;
      }
      if (q == nullptr) {
        return *p;
      }
      return {
        std::min(p->minimum, q->minimum), // min
        std::max(p->maximum, q->maximum)  // max
      };
    }
    static FloatShape unionFloat(const FloatShape* p, const FloatShape* q) {
      if (p == nullptr) {
        if (q == nullptr) {
          return BuiltinFactory::FloatShape;
        }
        return *q;
      }
      if (q == nullptr) {
        return *p;
      }
      return {
        std::min(p->minimum, q->minimum), // min
        std::max(p->maximum, q->maximum), // max
        p->allowNaN || q->allowNaN,       // nan
        p->allowNInf || q->allowNInf,     // ninf
        p->allowPInf || q->allowPInf      // pinf
      };
    }
    static std::vector<const ObjectShape*> unionObject(const std::array<const IType*, 2>& types) {
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

  class Builder : public HardReferenceCounted<ITypeBuilder> {
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
  protected:
    class Built;
    friend class Built;
    std::string name;
    std::unique_ptr<TypeBuilderIndexable> indexable;
    std::unique_ptr<TypeBuilderIterable> iterable;
    std::unique_ptr<TypeBuilderProperties> properties;
    bool built;
  public:
    explicit Builder(IAllocator& allocator, const String& name)
      : HardReferenceCounted(allocator, 0),
        name(name.toUTF8()),
        built(false) {
    }
    virtual void addPositionalParameter(const Type&, const String&, IFunctionSignatureParameter::Flags) {
      throw std::logic_error("TypeBuilder::addPositionalParameter() called for non-function type");
    }
    virtual void addNamedParameter(const Type&, const String&, IFunctionSignatureParameter::Flags) {
      throw std::logic_error("TypeBuilder::addNamedParameter() called for non-function type");
    }
    virtual void addProperty(const Type& ptype, const String& pname, Modifiability modifiability) override;
    virtual void defineDotable(bool closed) override;
    virtual void defineIndexable(const Type& resultType, const Type& indexType, Modifiability modifiability) override;
    virtual void defineIterable(const Type& resultType) override;
    virtual Type build() override {
      if (this->built) {
        throw std::logic_error("TypeBuilder::build() called more than once");
      }
      this->built = true;
      return this->allocator.make<Built, Type>(*this);
    }
  };

  class Builder::Built : public SoftReferenceCounted<IType> {
    Built(const Built&) = delete;
    Built& operator=(const Built&) = delete;
  private:
    HardPtr<Builder> builder;
    ObjectShape shape;
  public:
    Built(IAllocator& allocator, Builder& builder, IFunctionSignature* callable = nullptr)
      : SoftReferenceCounted(allocator),
      builder(&builder) {
      this->shape.callable = callable;
      this->shape.dotable = builder.properties.get();
      this->shape.indexable = builder.indexable.get();
      this->shape.iterable = builder.iterable.get();
    }
    virtual void softVisitLinks(const Visitor&) const {
      // TODO visit soft links in Builder?
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual const IntShape* getIntShape() const override {
      return nullptr;
    }
    virtual const FloatShape* getFloatShape() const override {
      return nullptr;
    }
    virtual const ObjectShape* getStringShape() const override {
      return nullptr;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &this->shape : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair(this->builder->name, 0);
    }
    virtual String describeValue() const override {
      return StringBuilder::concat("Value of type '", this->builder->name, "'");
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

  void Builder::defineDotable(bool closed) {
    if (this->properties != nullptr) {
      throw std::logic_error("TypeBuilder::defineDotable() called more than once");
    }
    this->properties = std::make_unique<TypeBuilderProperties>(closed);
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

  class FunctionBuilder : public Builder, public IFunctionSignature {
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
    Type rettype;
    std::vector<Parameter> positional;
    std::vector<Parameter> named;
  public:
    FunctionBuilder(IAllocator& allocator, const Type& rettype, const String& name)
      : Builder(allocator, name),
        rettype(rettype) {
      assert(rettype != nullptr);
    }
    virtual void addPositionalParameter(const Type& ptype, const String& pname, IFunctionSignatureParameter::Flags pflags) {
      if (this->built) {
        throw std::logic_error("FunctionBuilder::addPositionalParameter() called after type is built");
      }
      auto pindex = this->positional.size();
      this->positional.emplace_back(ptype, pname, pflags, pindex);
    }
    virtual void addNamedParameter(const Type& ptype, const String& pname, IFunctionSignatureParameter::Flags pflags) {
      if (this->built) {
        throw std::logic_error("FunctionBuilder::addNamedParameter() called after type is built");
      }
      this->named.emplace_back(ptype, pname, pflags, SIZE_MAX);
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->rettype;
    }
    virtual size_t getParameterCount() const override {
      if (!this->built) {
        throw std::logic_error("FunctionBuilder::getParameterCount() called before type is built");
      }
      return this->positional.size() + this->named.size();
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      if (!this->built) {
        throw std::logic_error("FunctionBuilder::getParameter() called before type is built");
      }
      if (index < this->positional.size()) {
        return this->positional[index];
      }
      return this->named.at(index - this->positional.size());
    }
    virtual Type build() override {
      if (this->built) {
        throw std::logic_error("FunctionBuilder::build() called more than once");
      }
      this->built = true;
      return this->allocator.make<Built, Type>(*this, this);
    }
  };

  const ObjectShape* queryObjectShape(const IType* type) {
    assert(type != nullptr);
    auto count = type->getObjectShapeCount();
    assert(count <= 1);
    if (count == 0) {
      return nullptr;
    }
    return type->getObjectShape(0);
  }
}

egg::ovum::Type::Assignability egg::ovum::Type::queryAssignable(const IType&) const {
  throw std::logic_error("WOBBLE: Not implemented: " + std::string(__FUNCTION__));
}

const egg::ovum::IFunctionSignature* egg::ovum::Type::queryCallable() const {
  auto* shape = queryObjectShape(this->get());
  return (shape == nullptr) ? nullptr : shape->callable;
}

const egg::ovum::IPropertySignature* egg::ovum::Type::queryDotable() const {
  auto* shape = queryObjectShape(this->get());
  return (shape == nullptr) ? nullptr : shape->dotable;
}

const egg::ovum::IIndexSignature* egg::ovum::Type::queryIndexable() const {
  auto* shape = queryObjectShape(this->get());
  return (shape == nullptr) ? nullptr : shape->indexable;
}

const egg::ovum::IIteratorSignature* egg::ovum::Type::queryIterable() const {
  auto* shape = queryObjectShape(this->get());
  return (shape == nullptr) ? nullptr : shape->iterable;
}

egg::ovum::Type egg::ovum::Type::addVoid(IAllocator& allocator) const {
  // TODO optimize
  assert(!this->hasAnyFlags(ValueFlags::Void));
  return TypeFactory::createUnion(allocator, *Type::Void, **this);
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
  switch (flags) {
  case ValueFlags::Void:
    return Type::Void;
  case ValueFlags::Null:
    return Type::Null;
  case ValueFlags::Bool:
    return Type::Bool;
  case ValueFlags::Int:
    return Type::Int;
  case ValueFlags::Float:
    return Type::Float;
  case ValueFlags::Arithmetic:
    return Type::Arithmetic;
  case ValueFlags::String:
    return Type::String;
  case ValueFlags::Object:
    return Type::Object;
  case ValueFlags::Any:
    return Type::Any;
  case ValueFlags::AnyQ:
    return Type::AnyQ;
  case ValueFlags::None:
  case ValueFlags::Break:
  case ValueFlags::Continue:
  case ValueFlags::Return:
  case ValueFlags::Yield:
  case ValueFlags::Throw:
  case ValueFlags::FlowControl:
    break;
  }
  return allocator.make<TypeSimple, Type>(flags);
}

egg::ovum::Type egg::ovum::TypeFactory::createPointer(IAllocator&, const IType&) {
  throw std::logic_error("WOBBLE: Not implemented: " + std::string(__FUNCTION__));
}

egg::ovum::Type egg::ovum::TypeFactory::createUnion(IAllocator& allocator, const IType& a, const IType& b) {
  if (&a == &b) {
    return Type(&a);
  }
  return allocator.make<TypeUnion, Type>(a, b);
}

egg::ovum::TypeBuilder egg::ovum::TypeFactory::createTypeBuilder(IAllocator& allocator, const String& name) {
  return allocator.make<Builder>(name);
}

egg::ovum::TypeBuilder egg::ovum::TypeFactory::createFunctionBuilder(IAllocator& allocator, const Type& rettype, const String& name) {
  return allocator.make<FunctionBuilder>(rettype, name);
}

egg::ovum::TypeBuilder egg::ovum::TypeFactory::createGeneratorBuilder(IAllocator& allocator, const Type& gentype, const String& name) {
  // Convert the return type (e.g. 'int') into a generator function 'int...' aka '(void|int)()'
  assert(!gentype.hasAnyFlags(ValueFlags::Void));
  auto generator = TypeFactory::createFunctionBuilder(allocator, gentype.addVoid(allocator), name);
  return allocator.make<FunctionBuilder>(generator->build(), name);
}

// Common types
const egg::ovum::Type egg::ovum::Type::Void{ &typeVoid };
const egg::ovum::Type egg::ovum::Type::Null{ &typeNull };
const egg::ovum::Type egg::ovum::Type::Bool{ &typeBool };
const egg::ovum::Type egg::ovum::Type::Int{ &typeInt };
const egg::ovum::Type egg::ovum::Type::Float{ &typeFloat };
const egg::ovum::Type egg::ovum::Type::String{ &typeString };
const egg::ovum::Type egg::ovum::Type::Arithmetic{ &typeArithmetic };
const egg::ovum::Type egg::ovum::Type::Object{ &typeObject };
const egg::ovum::Type egg::ovum::Type::Any{ &typeAny };
const egg::ovum::Type egg::ovum::Type::AnyQ{ &typeAnyQ };
