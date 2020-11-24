#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/builtin.h"
#include "ovum/function.h"

#include <map>

namespace {
  using namespace egg::ovum;

  class ParametersNone : public IParameters {
  public:
    virtual size_t getPositionalCount() const override {
      return 0;
    }
    virtual Value getPositional(size_t) const override {
      return Value::Void;
    }
    virtual const LocationSource* getPositionalLocation(size_t) const override {
      return nullptr;
    }
    virtual size_t getNamedCount() const override {
      return 0;
    }
    virtual String getName(size_t) const override {
      return String();
    }
    virtual Value getNamed(const String&) const override {
      return Value::Void;
    }
    virtual const LocationSource* getNamedLocation(const String&) const override {
      return nullptr;
    }
  };
  const ParametersNone parametersNone{};

  class StringProperty_FunctionType : public NotSoftReferenceCounted<IType>, public IFunctionSignature {
    StringProperty_FunctionType(const StringProperty_FunctionType&) = delete;
    StringProperty_FunctionType& operator=(const StringProperty_FunctionType&) = delete;
  private:
    struct Parameter : public IFunctionSignatureParameter {
      String name;
      Type type;
      size_t position;
      Flags flags;
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
    static const size_t MAX_PARAMETERS = 2;
    String name;
    Type rettype;
    size_t parameters;
    Parameter parameter[MAX_PARAMETERS];
    ObjectShape shape;
  public:
    StringProperty_FunctionType(const String& name, const Type& rettype)
      : name(name),
        rettype(rettype),
        parameters(0),
        shape({}) {
      this->shape.callable = this;
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
      return std::make_pair(this->name.toUTF8(), 0);
    }
    virtual String describeValue() const override {
      return StringBuilder::concat("Built-in 'string.", this->name, "'");
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual Type getReturnType() const override {
      return this->rettype;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters;
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters);
      return this->parameter[index];
    }
    void addParameter(const String& pname, const Type& ptype, IFunctionSignatureParameter::Flags pflags) {
      assert(this->parameters < MAX_PARAMETERS);
      auto index = this->parameters++;
      auto& entry = this->parameter[index];
      entry.name = pname;
      entry.type = ptype;
      entry.position = index;
      entry.flags = pflags;
    }
  };

  class StringProperty_Length final : public BuiltinFactory::StringProperty {
    StringProperty_Length(const StringProperty_Length&) = delete;
    StringProperty_Length& operator=(const StringProperty_Length&) = delete;
  public:
    StringProperty_Length() {}
    virtual Value createInstance(IAllocator& allocator, const String& string) const override {
      return ValueFactory::createInt(allocator, Int(string.length()));
    }
    virtual String getName() const override {
      return "length";
    }
    virtual Type getType() const override {
      return Type::Int;
    }
  };

  class StringProperty_Member : public BuiltinFactory::StringProperty {
    StringProperty_Member(const StringProperty_Member&) = delete;
    StringProperty_Member& operator=(const StringProperty_Member&) = delete;
  private:
    StringProperty_FunctionType ftype;
  public:
    StringProperty_Member(const String& name, const Type& rettype)
      : ftype(name, rettype) {
    }
    virtual Value createInstance(IAllocator& allocator, const String& string) const override;
    virtual String getName() const override {
      return this->ftype.getFunctionName();
    }
    virtual Type getType() const override {
      return Type(&this->ftype);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const = 0;
    void addRequiredParameter(const String& pname, const Type& ptype) {
      this->ftype.addParameter(pname, ptype, IFunctionSignatureParameter::Flags::Required);
    }
    void addOptionalParameter(const String& pname, const Type& ptype) {
      this->ftype.addParameter(pname, ptype, IFunctionSignatureParameter::Flags::None);
    }
    void addVariadicParameter(const String& pname, const Type& ptype) {
      this->ftype.addParameter(pname, ptype, IFunctionSignatureParameter::Flags::Variadic);
    }
    Value call(IExecution& execution, const String& string, const IParameters& parameters) const {
      if (parameters.getNamedCount() > 0) {
        return this->raise(execution, "does not accept named parameters"); // TODO
      }
      ParameterChecker checker{ parameters, &this->ftype };
      if (!checker.validateCount()) {
        return this->raise(execution, checker.error);
      }
      return this->invoke(execution, string, checker);
    }
    template<typename... ARGS>
    Value raise(IExecution& execution, ARGS&&... args) const {
      return execution.raiseFormat("String property function '", this->getName(), "' ", std::forward<ARGS>(args)...);
    }
  };

  class StringProperty_Instance : public SoftReferenceCounted<IObject> {
    StringProperty_Instance(const StringProperty_Instance&) = delete;
    StringProperty_Instance& operator=(const StringProperty_Instance&) = delete;
  private:
    const StringProperty_Member& member;
    String string;
  public:
    StringProperty_Instance(IAllocator& allocator, const StringProperty_Member& member, const String& string)
      : SoftReferenceCounted(allocator),
        member(member),
        string(string) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // Nothing to visit
    }
    virtual Type getRuntimeType() const override {
      return this->member.getType();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      return this->member.call(execution, this->string, parameters);
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value mutProperty(IExecution& execution, const String& property, Mutation, const Value&) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value delProperty(IExecution& execution, const String& property) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value refProperty(IExecution& execution, const String& property) override {
      return this->unsupported(execution, "properties such as '", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value mutIndex(IExecution& execution, const Value&, Mutation, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value delIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value refIndex(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "indexing with '[]'");
    }
    virtual Value getPointee(IExecution& execution) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value setPointee(IExecution& execution, const Value&) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value mutPointee(IExecution& execution, Mutation, const Value&) override {
      return this->unsupported(execution, "pointer dereferencing with '*'");
    }
    virtual Value iterate(IExecution& execution) override {
      return this->unsupported(execution, "iteration");
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      sb.add("<string-", this->member.getName(), '>');
    }
  private:
    template<typename... ARGS>
    Value unsupported(IExecution& execution, ARGS&&... args) {
      return this->member.raise(execution, "does not support ", std::forward<ARGS>(args)...);
    }
  };

  Value StringProperty_Member::createInstance(IAllocator& allocator, const String& string) const {
    auto object = ObjectFactory::create<StringProperty_Instance>(allocator, *this, string);
    return ValueFactory::createObject(allocator, object);
  }

  class StringProperty_CompareTo final : public StringProperty_Member {
    StringProperty_CompareTo(const StringProperty_CompareTo&) = delete;
    StringProperty_CompareTo& operator=(const StringProperty_CompareTo&) = delete;
  public:
    StringProperty_CompareTo() : StringProperty_Member("compareTo", Type::Int) {
      this->addRequiredParameter("other", Type::String);
    }
    virtual Value invoke(IExecution& execution, const String& string, ParameterChecker& checker) const override {
      String other;
      if (!checker.validateCount() || !checker.validateParameter(0, other)) {
        return this->raise(execution, checker.error);
      }
      auto result = string.compareTo(other);
      return execution.makeValue(result);
    }
  };

  class StringProperties : public OrderedMap<String, std::unique_ptr<BuiltinFactory::StringProperty>> {
    StringProperties(const StringProperties&) = delete;
    StringProperties& operator=(const StringProperties&) = delete;
    typedef Type (*MemberBuilder)();
  public:
    StringProperties() {
      this->addMember<StringProperty_Length>();
      this->addMember<StringProperty_CompareTo>();
      //this->addMember<StringProperty_Contains>();
      //this->addMember<StringProperty_EndsWith>();
      //this->addMember<StringProperty_Hash>();
      //this->addMember<StringProperty_IndexOf>();
      //this->addMember<StringProperty_Join>();
      //this->addMember<StringProperty_LastIndexOf>();
      //this->addMember<StringProperty_PadLeft>();
      //this->addMember<StringProperty_PadRight>();
      //this->addMember<StringProperty_Repeat>();
      //this->addMember<StringProperty_Replace>();
      //this->addMember<StringProperty_Slice>();
      //this->addMember<StringProperty_StartsWith>();
    };
    static const StringProperties& instance() {
      static const StringProperties result;
      return result;
    }
  private:
    template<typename T>
    void addMember() {
      auto property = std::make_unique<T>();
      auto name = property->getName();
      this->add(name, std::move(property));
    }
  };

  class StringShapeDotable : public IPropertySignature {
  public:
    virtual Type getType(const String& property) const override {
      auto* found = StringProperties::instance().get(property);
      return (found == nullptr) ? Type::Void : (*found)->getType();
    }
    virtual Modifiability getModifiability(const String& property) const override {
      auto* found = StringProperties::instance().get(property);
      return (found == nullptr) ? Modifiability::None : Modifiability::Read;
    }
    virtual String getName(size_t index) const override {
      String key;
      auto* found = StringProperties::instance().get(index, key);
      return (found == nullptr) ? String() : key;
    }
    virtual size_t getNameCount() const override {
      return StringProperties::instance().size();
    }
    virtual bool isClosed() const override {
      return true;
    }
  };
  const StringShapeDotable stringShapeDotable{};

  class StringShapeIndexable : public IIndexSignature {
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
  const StringShapeIndexable stringShapeIndexable{};

  class StringShapeIterable : public IIteratorSignature {
  public:
    virtual Type getType() const override {
      return Type::String;
    }
  };
  const StringShapeIterable stringShapeIterable{};

  // An "omni" function looks like this: 'any?(...any?[])'
  class ObjectShapeCallable : public IFunctionSignature {
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
  const ObjectShapeCallable objectShapeCallable{};

  class ObjectShapeDotable : public IPropertySignature {
  public:
    virtual Type getType(const String&) const override {
      return Type::AnyQ;
    }
    virtual Modifiability getModifiability(const String&) const override {
      return Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
    }
    virtual String getName(size_t) const override {
      return String();
    }
    virtual size_t getNameCount() const override {
      return 0;
    }
    virtual bool isClosed() const override {
      return false;
    }
  };
  const ObjectShapeDotable objectShapeDotable{};

  class ObjectShapeIndexable : public IIndexSignature {
  public:
    virtual Type getResultType() const override {
      return Type::AnyQ;
    }
    virtual Type getIndexType() const override {
      return Type::AnyQ;
    }
    virtual Modifiability getModifiability() const override {
      return Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
    }
  };
  const ObjectShapeIndexable objectShapeIndexable{};

  class ObjectShapeIterable : public IIteratorSignature {
  public:
    virtual Type getType() const override {
      return Type::AnyQ;
    }
  };
  const ObjectShapeIterable objectShapeIterable{};

  class BuiltinBase : public SoftReferenceCounted<IObject> {
    BuiltinBase(const BuiltinBase&) = delete;
    BuiltinBase& operator=(const BuiltinBase&) = delete;
  protected:
    String name;
  public:
    BuiltinBase(IAllocator& allocator, const String& name)
      : SoftReferenceCounted(allocator),
        name(name) {
      assert(!name.empty());
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      return this->raiseBuiltin(execution, "does not support properties such as '", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      return this->raiseBuiltin(execution, "does not support properties such as '", property, "'");
    }
    virtual Value mutProperty(IExecution& execution, const String& property, Mutation, const Value&) override {
      return this->raiseBuiltin(execution, "does not support properties such as '", property, "'");
    }
    virtual Value delProperty(IExecution& execution, const String& property) override {
      return this->raiseBuiltin(execution, "does not support properties such as '", property, "'");
    }
    virtual Value refProperty(IExecution& execution, const String& property) override {
      return this->raiseBuiltin(execution, "does not support properties such as '", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value mutIndex(IExecution& execution, const Value&, Mutation, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value delIndex(IExecution& execution, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value refIndex(IExecution& execution, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value getPointee(IExecution& execution) override {
      return this->raiseBuiltin(execution, "does not support pointer dereferencing with '*'");
    }
    virtual Value setPointee(IExecution& execution, const Value&) override {
      return this->raiseBuiltin(execution, "does not support pointer dereferencing with '*'");
    }
    virtual Value mutPointee(IExecution& execution, Mutation, const Value&) override {
      return this->raiseBuiltin(execution, "does not support pointer dereferencing with '*'");
    }
    virtual Value iterate(IExecution& execution) override {
      return this->raiseBuiltin(execution, "does not support iteration");
    }
  protected:
    template<typename... ARGS>
    Value raiseBuiltin(IExecution& execution, ARGS&&... args) {
      return execution.raiseFormat("Built-in '", this->name, "' ", std::forward<ARGS>(args)...);
    }
    template<typename T>
    Value makeValue(T value) const {
      return ValueFactory::create(this->allocator, value);
    }
  };

  class BuiltinType_Base : public SoftReferenceCounted<IType> {
    BuiltinType_Base(const BuiltinType_Base&) = delete;
    BuiltinType_Base& operator=(const BuiltinType_Base&) = delete;
  private:
    String name;
  public:
    BuiltinType_Base(IAllocator& allocator, const String& name)
      : SoftReferenceCounted(allocator),
        name(name) {
      assert(!name.empty());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("<builtin-" + this->name.toUTF8() + ">", 0);
    }
    virtual String describeValue() const override {
      return StringBuilder::concat("Type '", this->name, "'");
    }
    virtual void softVisitLinks(const ICollectable::Visitor&) const override {
      // No children
    }
    const String& getName() const {
      return this->name;
    }
  };

  class Builtin_Base : public BuiltinBase {
    Builtin_Base(const Builtin_Base&) = delete;
    Builtin_Base& operator=(const Builtin_Base&) = delete;
  private:
    HardPtr<BuiltinType_Base> type;
  public:
    Builtin_Base(IAllocator& allocator, const BuiltinType_Base& type)
      : BuiltinBase(allocator, type.getName()),
        type(&type) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
      assert(false);
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      sb.add('<', this->name, '>');
    }
    virtual Type getRuntimeType() const override {
      return Type(this->type.get());
    }
  };

  class BuiltinFunction : public BuiltinBase {
    BuiltinFunction(const BuiltinFunction&) = delete;
    BuiltinFunction& operator=(const BuiltinFunction&) = delete;
  protected:
    TypeBuilder builder;
    Type type;
  public:
    BuiltinFunction(IAllocator& allocator, const Type& rettype, const String& name)
      : BuiltinBase(allocator, StringBuilder::concat(name, "()")),
        builder(TypeFactory::createFunctionBuilder(allocator, rettype, name)),
        type() {
      // 'type' will be initialized by derived class constructors
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // There are no soft links to visit
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      sb.add(this->name);
    }
    virtual Type getRuntimeType() const override {
      return Type(this->type.get());
    }
  };

  class Builtin_Assert : public BuiltinFunction {
    Builtin_Assert(const Builtin_Assert&) = delete;
    Builtin_Assert& operator=(const Builtin_Assert&) = delete;
  public:
    explicit Builtin_Assert(IAllocator& allocator)
      : BuiltinFunction(allocator, Type::Void, "assert") {
      this->builder->addPositionalParameter(Type::Any, "predicate", Bits::set(IFunctionSignatureParameter::Flags::Required, IFunctionSignatureParameter::Flags::Predicate));
      this->type = this->builder->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      if (parameters.getPositionalCount() != 1) {
        return this->raiseBuiltin(execution, "accepts only 1 parameter, not ", parameters.getPositionalCount());
      }
      return execution.assertion(parameters.getPositional(0));
    }
  };

  class Builtin_Print : public BuiltinFunction {
    Builtin_Print(const Builtin_Print&) = delete;
    Builtin_Print& operator=(const Builtin_Print&) = delete;
  public:
    explicit Builtin_Print(IAllocator& allocator)
      : BuiltinFunction(allocator, Type::Void, "print") {
      this->builder->addPositionalParameter(Type::AnyQ, "values", IFunctionSignatureParameter::Flags::Variadic);
      this->type = this->builder->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      StringBuilder sb;
      auto n = parameters.getPositionalCount();
      for (size_t i = 0; i < n; ++i) {
        sb.add(parameters.getPositional(i)->toString());
      }
      execution.print(sb.toUTF8());
      return Value::Void;
    }
  };

  class Builtin_String : public BuiltinFunction {
    Builtin_String(const Builtin_String&) = delete;
    Builtin_String& operator=(const Builtin_String&) = delete;
  public:
    explicit Builtin_String(IAllocator& allocator)
      : BuiltinFunction(allocator, Type::String, "string") {
      this->builder->addPositionalParameter(Type::AnyQ, "values", IFunctionSignatureParameter::Flags::Variadic);
      this->type = this->builder->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      StringBuilder sb;
      auto n = parameters.getPositionalCount();
      for (size_t i = 0; i < n; ++i) {
        sb.add(parameters.getPositional(i)->toString());
      }
      return ValueFactory::createString(allocator, sb.str());
    }
  };

  class Builtin_Type : public BuiltinFunction {
    Builtin_Type(const Builtin_Type&) = delete;
    Builtin_Type& operator=(const Builtin_Type&) = delete;
  public:
    explicit Builtin_Type(IAllocator& allocator)
      : BuiltinFunction(allocator, Type::String, "type") {
      this->type = this->builder->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      // TODO 'type.of' et al
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      if (parameters.getPositionalCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept parameters");
      }
      return ValueFactory::createString(allocator, "WOBBLE");
    }
  };
}

const egg::ovum::IParameters& egg::ovum::Object::ParametersNone{ parametersNone };
const egg::ovum::IFunctionSignature& egg::ovum::Object::OmniFunctionSignature{ objectShapeCallable };

const egg::ovum::IntShape& egg::ovum::BuiltinFactory::getIntShape() {
  static const IntShape instance{ IntShape::Minimum, IntShape::Maximum };
  return instance;
}

const egg::ovum::FloatShape& egg::ovum::BuiltinFactory::getFloatShape() {
  static const FloatShape instance{ FloatShape::Minimum, FloatShape::Maximum, true, true, true };
  return instance;
}

const egg::ovum::ObjectShape& egg::ovum::BuiltinFactory::getStringShape() {
  static const ObjectShape instance{ nullptr, &stringShapeDotable, &stringShapeIndexable, &stringShapeIterable };
  return instance;
}

const egg::ovum::ObjectShape& egg::ovum::BuiltinFactory::getObjectShape() {
  static const ObjectShape instance{ &objectShapeCallable, &objectShapeDotable, &objectShapeIndexable, &objectShapeIterable };
  return instance;
}

const egg::ovum::BuiltinFactory::StringProperty* egg::ovum::BuiltinFactory::getStringPropertyByName(const String& property) {
  auto* entry = StringProperties::instance().get(property);
  return (entry == nullptr) ? nullptr : entry->get();
}

const egg::ovum::BuiltinFactory::StringProperty* egg::ovum::BuiltinFactory::getStringPropertyByIndex(size_t index) {
  String key;
  auto* entry = StringProperties::instance().get(index, key);
  return (entry == nullptr) ? nullptr : entry->get();
}

size_t egg::ovum::BuiltinFactory::getStringPropertyCount() {
  return StringProperties::instance().size();
}

egg::ovum::Value egg::ovum::BuiltinFactory::createAssertInstance(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Assert>(allocator));
}

egg::ovum::Value egg::ovum::BuiltinFactory::createPrintInstance(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Print>(allocator));
}

egg::ovum::Value egg::ovum::BuiltinFactory::createStringInstance(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_String>(allocator));
}

egg::ovum::Value egg::ovum::BuiltinFactory::createTypeInstance(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Type>(allocator));
}
