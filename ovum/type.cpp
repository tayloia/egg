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

  class TypeBase : public NotSoftReferenceCounted<IType> {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  public:
    TypeBase() = default;
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

  class TypeVoid final : public TypeBase {
    TypeVoid(const TypeVoid&) = delete;
    TypeVoid& operator=(const TypeVoid&) = delete;
  public:
    TypeVoid() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Void;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "void", 0 };
    }
  };
  const TypeVoid typeVoid{};

  class TypeNull final : public TypeBase {
    TypeNull(const TypeNull&) = delete;
    TypeNull& operator=(const TypeNull&) = delete;
  public:
    TypeNull() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Null;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "null", 0 };
    }
  };
  const TypeNull typeNull{};

  class TypeBool final : public TypeBase {
    TypeBool(const TypeBool&) = delete;
    TypeBool& operator=(const TypeBool&) = delete;
  public:
    TypeBool() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Bool;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "bool", 0 };
    }
  };
  const TypeBool typeBool{};

  class TypeInt final : public TypeBase {
    TypeInt(const TypeInt&) = delete;
    TypeInt& operator=(const TypeInt&) = delete;
  public:
    TypeInt() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Int;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::getIntShape();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "int", 0 };
    }
  };
  const TypeInt typeInt{};

  class TypeIntQ final : public TypeBase {
    TypeIntQ(const TypeIntQ&) = delete;
    TypeIntQ& operator=(const TypeIntQ&) = delete;
  public:
    TypeIntQ() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Null | ValueFlags::Int;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::getIntShape();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "int?", 1 };
    }
  };
  const TypeIntQ typeIntQ{};

  class TypeFloat final : public TypeBase {
    TypeFloat(const TypeFloat&) = delete;
    TypeFloat& operator=(const TypeFloat&) = delete;
  public:
    TypeFloat() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Float;
    }
    virtual const FloatShape* getFloatShape() const override {
      return &BuiltinFactory::getFloatShape();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "float", 0 };
    }
  };
  const TypeFloat typeFloat{};

  class TypeArithmetic final : public TypeBase {
    TypeArithmetic(const TypeArithmetic&) = delete;
    TypeArithmetic& operator=(const TypeArithmetic&) = delete;
  public:
    TypeArithmetic() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Arithmetic;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::getIntShape();
    }
    virtual const FloatShape* getFloatShape() const override {
      return &BuiltinFactory::getFloatShape();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "int|float", 2 };
    }
  };
  const TypeArithmetic typeArithmetic{};

  class TypeString final : public TypeBase {
    TypeString(const TypeString&) = delete;
    TypeString& operator=(const TypeString&) = delete;
  public:
    TypeString() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::String;
    }
    virtual const ObjectShape* getStringShape() const override {
      return &BuiltinFactory::getStringShape();
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "string", 0 };
    }
  };
  const TypeString typeString{};

  class TypeObject final : public TypeBase {
    TypeObject(const TypeObject&) = delete;
    TypeObject& operator=(const TypeObject&) = delete;
  public:
    TypeObject() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &BuiltinFactory::getObjectShape() : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "object", 0 };
    }
  };
  const TypeObject typeObject{};

  class TypeAny final : public TypeBase {
    TypeAny(const TypeAny&) = delete;
    TypeAny& operator=(const TypeAny&) = delete;
  public:
    TypeAny() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Any;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::getIntShape();
    }
    virtual const FloatShape* getFloatShape() const override {
      return &BuiltinFactory::getFloatShape();
    }
    virtual const ObjectShape* getStringShape() const override {
      return &BuiltinFactory::getStringShape();
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &BuiltinFactory::getObjectShape() : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "any", 0 };
    }
  };
  const TypeAny typeAny{};

  class TypeAnyQ final : public TypeBase {
    TypeAnyQ(const TypeAnyQ&) = delete;
    TypeAnyQ& operator=(const TypeAnyQ&) = delete;
  public:
    TypeAnyQ() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::AnyQ;
    }
    virtual const IntShape* getIntShape() const override {
      return &BuiltinFactory::getIntShape();
    }
    virtual const FloatShape* getFloatShape() const override {
      return &BuiltinFactory::getFloatShape();
    }
    virtual const ObjectShape* getStringShape() const override {
      return &BuiltinFactory::getStringShape();
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return (index == 0) ? &BuiltinFactory::getObjectShape() : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return 1;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "any?", 1 };
    }
  };
  const TypeAnyQ typeAnyQ{};

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
      return Bits::hasAnySet(this->flags, ValueFlags::Int) ? &BuiltinFactory::getIntShape() : nullptr;
    }
    virtual const FloatShape* getFloatShape() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::Float) ? &BuiltinFactory::getFloatShape() : nullptr;
    }
    virtual const ObjectShape* getStringShape() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::String) ? &BuiltinFactory::getStringShape() : nullptr;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return ((index == 0) && Bits::hasAnySet(this->flags, ValueFlags::Object)) ? &BuiltinFactory::getObjectShape() : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      return Bits::hasAnySet(this->flags, ValueFlags::Object) ? 1u : 0u;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return flagsToStringPrecedence(this->flags);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  class TypePointer : public SoftReferenceCounted<IType> {
    TypePointer(const TypePointer&) = delete;
    TypePointer& operator=(const TypePointer&) = delete;
  private:
    std::string name;
    Type pointee; // TODO make soft
    ObjectShape shape;
  public:
    TypePointer(IAllocator& allocator, const IType& pointee)
      : SoftReferenceCounted(allocator),
        name(TypePointer::makeName(pointee.toStringPrecedence())),
        pointee(&pointee) {
      assert(this->pointee != nullptr);
      this->shape.callable = nullptr;
      this->shape.dotable = nullptr;
      this->shape.indexable = nullptr;
      this->shape.iterable = nullptr;
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
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
      return std::make_pair(this->name, 0);
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->name, "'");
    }
    static std::string makeName(const std::pair<std::string, int>& pair) {
      auto name = pair.first;
      if (pair.second > 1) {
        return '(' + pair.first + ')' + '*';
      }
      return pair.first + '*';
    }
  };

  class TypeStrip : public SoftReferenceCounted<IType> {
    TypeStrip(const TypeStrip&) = delete;
    TypeStrip& operator=(const TypeStrip&) = delete;
  private:
    Type underlying; // TODO make soft
    ValueFlags strip;
  public:
    TypeStrip(IAllocator& allocator, const IType& underlying, ValueFlags strip)
      : SoftReferenceCounted(allocator),
        underlying(&underlying),
        strip(strip) {
      assert(this->underlying != nullptr);
      assert(!Bits::hasAnySet(this->strip, ValueFlags::FlowControl | ValueFlags::Object));
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
    }
    virtual ValueFlags getFlags() const override {
      return Bits::clear(this->underlying->getFlags(), this->strip);
    }
    virtual const IntShape* getIntShape() const override {
      return Bits::hasAnySet(this->strip, ValueFlags::Int) ? nullptr : this->underlying->getIntShape();
    }
    virtual const FloatShape* getFloatShape() const override {
      return Bits::hasAnySet(this->strip, ValueFlags::Float) ? nullptr : this->underlying->getFloatShape();
    }
    virtual const ObjectShape* getStringShape() const override {
      return Bits::hasAnySet(this->strip, ValueFlags::String) ? nullptr : this->underlying->getStringShape();
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      return this->underlying->getObjectShape(index);
    }
    virtual size_t getObjectShapeCount() const override {
      return this->underlying->getObjectShapeCount();
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
      return Bits::hasAnySet(this->flags, ValueFlags::String) ? &BuiltinFactory::getStringShape() : nullptr;
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
          return BuiltinFactory::getIntShape();
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
          return BuiltinFactory::getFloatShape();
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
    virtual void defineDotable(const Type& unknownType, Modifiability unknownModifiability) override;
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
    FunctionBuilder(IAllocator& allocator, const Type& rettype, const String& name)
      : Builder(allocator, name),
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
      return this->allocator.make<Built, Type>(*this, &this->callable);
    }
  };

  const ObjectShape* queryObjectShape(const IType* type) {
    assert(type != nullptr);
    auto count = type->getObjectShapeCount();
    assert(count <= 1);
    if (count == 0) {
      return type->getStringShape();
    }
    return type->getObjectShape(0);
  }

  Type::Assignability queryAssignableInt(const IntShape* lhs, const IntShape* rhs) {
    // WOBBLE range
    assert(lhs != nullptr);
    assert(rhs != nullptr);
    (void)lhs;
    (void)rhs;
    return Type::Assignability::Always;
  }

  Type::Assignability queryAssignableFloat(const FloatShape* lhs, const FloatShape* rhs) {
    // WOBBLE range
    assert(lhs != nullptr);
    assert(rhs != nullptr);
    (void)lhs;
    (void)rhs;
    return Type::Assignability::Always;
  }

  Type::Assignability queryAssignableObject(const IType& lhs, const IType& rhs) {
    // WOBBLE shapes
    (void)lhs;
    (void)rhs;
    return Type::Assignability::Always;
  }

  Type::Assignability queryAssignablePrimitive(const IType& lhs, const IType& rhs, ValueFlags lflags, ValueFlags rflags) {
    assert(lhs.validate());
    assert(rhs.validate());
    assert(Bits::hasOneSet(rflags, ValueFlags::AnyQ));
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
    switch (rflags) {
    case ValueFlags::Null:
    case ValueFlags::Bool:
    case ValueFlags::String:
      if (lflags == rflags) {
        return Type::Assignability::Always;
      }
      if (Bits::hasAnySet(lflags, rflags)) {
        return Type::Assignability::Sometimes;
      }
      break;
    case ValueFlags::Int:
      if (lflags == rflags) {
        return queryAssignableInt(lhs.getIntShape(), rhs.getIntShape());
      }
      if (Bits::hasAnySet(lflags, ValueFlags::Arithmetic)) {
        return Type::Assignability::Sometimes;
      }
      break;
    case ValueFlags::Float:
      if (lflags == rflags) {
        return queryAssignableFloat(lhs.getFloatShape(), rhs.getFloatShape());
      }
      if (Bits::hasAnySet(lflags, rflags)) {
        return Type::Assignability::Sometimes;
      }
      break;
    case ValueFlags::Object:
      if (lflags == rflags) {
        return queryAssignableObject(lhs, rhs);
      }
      if (Bits::hasAnySet(lflags, rflags)) {
        return Type::Assignability::Sometimes;
      }
      break;
    default:
      assert(0); // not a type within 'any?'
      break;
    }
    EGG_WARNING_SUPPRESS_SWITCH_END();
    return Type::Assignability::Never;
  }

  Type::Assignability queryAssignableType(const IType& lhs, const IType& rhs) {
    assert(lhs.validate());
    assert(rhs.validate());
    auto lflags = lhs.getFlags();
    auto rflags = rhs.getFlags();
    if (Bits::hasOneSet(rflags, ValueFlags::AnyQ)) {
      return queryAssignablePrimitive(lhs, rhs, lflags, rflags);
    }
    auto rflag = Bits::topmost(rflags);
    assert(rflag != ValueFlags::None);
    auto assignability = queryAssignablePrimitive(lhs, rhs, lflags, rflag);
    if (assignability == Type::Assignability::Sometimes) {
      return Type::Assignability::Sometimes;
    }
    rflags = Bits::clear(rflags, rflag);
    rflag = Bits::topmost(rflags);
    while (rflag != ValueFlags::None) {
      if (queryAssignablePrimitive(lhs, rhs, lflags, rflag) != assignability) {
        return Type::Assignability::Sometimes;
      }
      rflags = Bits::clear(rflags, rflag);
      rflag = Bits::topmost(rflags);
    }
    return assignability;
  }
}

egg::ovum::Type::Assignability egg::ovum::Type::queryAssignable(const IType& from) const {
  auto* to = this->get();
  assert(to != nullptr);
  return queryAssignableType(*to, from);
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

egg::ovum::Type egg::ovum::Type::addFlags(IAllocator& allocator, ValueFlags flags) const {
  // TODO optimize
  if (flags != ValueFlags::Void) {
    throw std::logic_error("Type::addFlags() can only add 'void'");
  }
  if (this->hasAnyFlags(ValueFlags::Void)) {
    return *this;
  }
  return TypeFactory::createUnion(allocator, *Type::Void, **this);
}

egg::ovum::Type egg::ovum::Type::stripFlags(IAllocator& allocator, ValueFlags flags) const {
  // TODO optimize
  auto* underlying = this->get();
  if (underlying != nullptr) {
    const auto acceptable = ValueFlags::Void | ValueFlags::Null;
    if (Bits::set(flags, acceptable) != acceptable) {
      throw std::logic_error("Type::stripFlags() can only strip 'void' and 'null'");
    }
    if (Bits::hasAnySet(underlying->getFlags(), flags)) {
      if (underlying->getFlags() == flags) {
        return nullptr;
      }
      return allocator.make<TypeStrip, Type>(*underlying, flags);
    }
  }
  return *this;
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

egg::ovum::Type egg::ovum::TypeFactory::createPointer(IAllocator& allocator, const IType& pointee) {
  return allocator.make<TypePointer, Type>(pointee);
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
  auto generator = TypeFactory::createFunctionBuilder(allocator, gentype.addFlags(allocator, ValueFlags::Void), name);
  return allocator.make<FunctionBuilder>(generator->build(), name);
}

// Common types
const egg::ovum::Type egg::ovum::Type::Void{ &typeVoid };
const egg::ovum::Type egg::ovum::Type::Null{ &typeNull };
const egg::ovum::Type egg::ovum::Type::Bool{ &typeBool };
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
    if (!this->hasAnyFlags(ValueFlags::Null)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Bool:
    if (!this->hasAnyFlags(ValueFlags::Bool)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Int:
    // WOBBLE range
    if (!this->hasAnyFlags(ValueFlags::Int)) {
      if (this->hasAnyFlags(ValueFlags::Float)) {
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
    if (!this->hasAnyFlags(ValueFlags::Float)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::String:
    if (!this->hasAnyFlags(ValueFlags::String)) {
      return Assignment::Incompatible;
    }
    break;
  case ValueFlags::Object:
    if (!this->hasAnyFlags(ValueFlags::Object)) {
      return Assignment::Incompatible;
    }
    break;
  case egg::ovum::ValueFlags::None:
  case egg::ovum::ValueFlags::Any:
  case egg::ovum::ValueFlags::AnyQ:
  case egg::ovum::ValueFlags::Arithmetic:
  case egg::ovum::ValueFlags::FlowControl:
  case egg::ovum::ValueFlags::Break:
  case egg::ovum::ValueFlags::Continue:
  case egg::ovum::ValueFlags::Return:
  case egg::ovum::ValueFlags::Yield:
  case egg::ovum::ValueFlags::Throw:
  default:
    return Assignment::Incompatible;
  }
  out = rhs;
  return Assignment::Success;
}

egg::ovum::Type::Assignment egg::ovum::Type::mutate(IAllocator& allocator, const Value& lhs, const Value& rhs, Mutation mutation, Value& out) const {
  assert(lhs.validate());
  assert(rhs.validate());
  egg::ovum::Int i;
  switch (mutation) {
  case Mutation::Assign:
    return this->promote(allocator, rhs, out);
  case Mutation::Noop:
    out = lhs;
    return Assignment::Success;
  case Mutation::Decrement:
    assert(rhs->getVoid());
    if (!lhs->getInt(i)) {
      return Assignment::Incompatible;
    }
    out = ValueFactory::createInt(allocator, i - 1);
    return Assignment::Success;
  case Mutation::Increment:
    assert(rhs->getVoid());
    if (!lhs->getInt(i)) {
      return Assignment::Incompatible;
    }
    out = ValueFactory::createInt(allocator, i + 1);
    return Assignment::Success;
  case Mutation::Add:
  case Mutation::BitwiseAnd:
  case Mutation::BitwiseOr:
  case Mutation::BitwiseXor:
  case Mutation::Divide:
  case Mutation::IfNull:
  case Mutation::LogicalAnd:
  case Mutation::LogicalOr:
  case Mutation::Multiply:
  case Mutation::Remainder:
  case Mutation::ShiftLeft:
  case Mutation::ShiftRight:
  case Mutation::ShiftRightUnsigned:
  case Mutation::Subtract:
  default:
    return Assignment::Unimplemented;
  }
}
