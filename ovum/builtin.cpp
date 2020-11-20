#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/builtin.h"

#include <map>

// WOBBLE
#define EGG_STRING_PROPERTIES(X) \
  X(compareTo, WIBBLE) \
  X(contains, WIBBLE) \
  X(endsWith, WIBBLE) \
  X(hash, WIBBLE) \
  X(indexOf, WIBBLE) \
  X(join, WIBBLE) \
  X(lastIndexOf, WIBBLE) \
  X(padLeft, WIBBLE) \
  X(padRight, WIBBLE) \
  X(repeat, WIBBLE) \
  X(replace, WIBBLE) \
  X(slice, WIBBLE) \
  X(startsWith, WIBBLE) \
  X(toString, WIBBLE)

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

  class StringShapeTable {
    StringShapeTable(const StringShapeTable&) = delete;
    StringShapeTable& operator=(const StringShapeTable&) = delete;
  public:
    struct Member {
      String name;
      Type type;
    };
  private:
    std::vector<Member> vec;
    std::unordered_map<String, size_t> map;
  public:
    StringShapeTable() {
      this->add({ "length", Type::Int });
    }
    const Member* findByName(const String& name) const {
      const auto& found = this->map.find(name);
      return (found != this->map.end()) ? &this->vec[found->second] : nullptr;
    }
    const Member* findByIndex(size_t index) const {
      return (index < this->vec.size()) ? &this->vec[index] : nullptr;
    }
    size_t size() const {
      return this->vec.size();
    }
  private:
    void add(Member&& member) {
      auto index = this->vec.size();
      auto inserted = this->map.insert(std::make_pair(member.name, index));
      assert(inserted.second);
      (void)inserted;
      this->vec.emplace_back(std::move(member));
    }
  };
  const StringShapeTable stringShapeTable{};

  class StringShapeDotable : public IPropertySignature {
  public:
    virtual Type getType(const String& property) const override {
      auto* found = stringShapeTable.findByName(property);
      return (found == nullptr) ? Type::Void : found->type;
    }
    virtual Modifiability getModifiability(const String& property) const override {
      auto* found = stringShapeTable.findByName(property);
      return (found == nullptr) ? Modifiability::None : Modifiability::Read;
    }
    virtual String getName(size_t index) const override {
      auto* found = stringShapeTable.findByIndex(index);
      return (found == nullptr) ? String() : found->name;
    }
    virtual size_t getNameCount() const override {
      return stringShapeTable.size();
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
      this->builder->addPositionalParameter(Type::Any, "values", IFunctionSignatureParameter::Flags::Variadic);
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
}

const egg::ovum::IParameters& egg::ovum::Object::ParametersNone{ parametersNone };
const egg::ovum::IFunctionSignature& egg::ovum::Object::OmniFunctionSignature{ objectShapeCallable };

const egg::ovum::IntShape egg::ovum::BuiltinFactory::IntShape{ IntShape::Minimum, IntShape::Maximum };
const egg::ovum::FloatShape egg::ovum::BuiltinFactory::FloatShape{ FloatShape::Minimum, FloatShape::Maximum, true, true, true };
const egg::ovum::ObjectShape egg::ovum::BuiltinFactory::StringShape{ nullptr, &stringShapeDotable, &stringShapeIndexable, &stringShapeIterable };
const egg::ovum::ObjectShape egg::ovum::BuiltinFactory::ObjectShape{ &objectShapeCallable, &objectShapeDotable, &objectShapeIndexable, &objectShapeIterable };

egg::ovum::Value egg::ovum::BuiltinFactory::createAssert(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Assert>(allocator));
}

egg::ovum::Value egg::ovum::BuiltinFactory::createPrint(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Print>(allocator));
}

egg::ovum::Value egg::ovum::BuiltinFactory::createString(IAllocator&) {
  throw std::logic_error("WOBBLE: Not implemented: " + std::string(__FUNCTION__));
}

egg::ovum::Value egg::ovum::BuiltinFactory::createType(IAllocator&) {
  throw std::logic_error("WOBBLE: Not implemented: " + std::string(__FUNCTION__));
}
