#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/vanilla.h"

#include <map>

namespace {
  using namespace egg::ovum;

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
    virtual bool validate() const override {
      return true; // WIBBLE
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
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value mutIndex(IExecution& execution, const Value&, Mutation, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value iterate(IExecution& execution) override {
      return this->raiseBuiltin(execution, "does not support iteration");
    }
    template<typename... ARGS>
    Value raiseBuiltin(IExecution& execution, ARGS&&... args) {
      return execution.raiseFormat("Built-in '", this->name, "' ", std::forward<ARGS>(args)...);
    }
  protected:
    template<typename T>
    Value makeValue(T value) const {
      return ValueFactory::create(this->allocator, value);
    }
  };

  class BuiltinObjectType : public NotReferenceCounted<IType> {
    BuiltinObjectType(const BuiltinObjectType&) = delete;
    BuiltinObjectType& operator=(const BuiltinObjectType&) = delete;
  private:
    String name;
  public:
    explicit BuiltinObjectType(const String& name)
      : name(name) {
      assert(!name.empty());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual Error tryAssign(IAllocator&, ISlot&, const Value&) const override {
      return Error("Cannot assign to built-in '", this->name, "'");
    }
    virtual Error tryMutate(IAllocator&, ISlot&, Mutation, const Value&) const override {
      return Error("Cannot modify built-in '", this->name, "'");
    }
    virtual Erratic<bool> queryAssignableAlways(const IType&) const override {
      return Erratic<bool>::fail("Cannot assign to built-in '", this->name, "'");
    }
    virtual Erratic<Type> queryIterable() const override {
      return Erratic<Type>::fail("Built-in '", this->name, "' is not iterable");
    }
    virtual Erratic<Type> queryPointeeType() const override {
      return Erratic<Type>::fail("Built-in '", this->name, "' does not support the pointer dereferencing '*' operator");
    }
    virtual const IIndexSignature* queryIndexable() const override {
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("<builtin-" + this->name.toUTF8() + ">", 0);
    }
    const String& getName() const {
      return this->name;
    }
  };

  class BuiltinObject : public BuiltinBase {
    BuiltinObject(const BuiltinObject&) = delete;
    BuiltinObject& operator=(const BuiltinObject&) = delete;
  private:
    HardPtr<BuiltinObjectType> type;
  public:
    BuiltinObject(IAllocator& allocator, const BuiltinObjectType& type)
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

  class BuiltinType_String : public BuiltinObjectType {
    BuiltinType_String(const BuiltinType_String&) = delete;
    BuiltinType_String& operator=(const BuiltinType_String&) = delete;
  private:
    class FunctionSignatureParameter : public IFunctionSignatureParameter {
    public:
      virtual String getName() const override {
        return "values";
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
    class FunctionSignature : public IFunctionSignature {
    public:
      virtual String getFunctionName() const override {
        return "string";
      }
      virtual Type getReturnType() const override {
        return Type::String;
      }
      virtual size_t getParameterCount() const override {
        return 1;
      }
      virtual const IFunctionSignatureParameter& getParameter(size_t) const override {
        static const FunctionSignatureParameter parameter;
        return parameter;
      }
    };
  public:
    BuiltinType_String() : BuiltinObjectType("string") {}
    virtual Type makeUnion(IAllocator& allocator, const IType& rhs) const override {
      return TypeFactory::createUnionJoin(allocator, *this, rhs);
    }
    virtual Erratic<Type> queryPropertyType(const String& property) const override {
      return Erratic<Type>::fail("Unknown built-in property: 'string.", property, "'");
    }
    virtual const IFunctionSignature* queryCallable() const override {
      static const FunctionSignature signature;
      return &signature;
    }
  };

  class Builtin_String : public BuiltinObject {
    Builtin_String(const Builtin_String&) = delete;
    Builtin_String& operator=(const Builtin_String&) = delete;
  public:
    explicit Builtin_String(IAllocator& allocator, const BuiltinObjectType& type)
      : BuiltinObject(allocator, type) {
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
      return this->makeValue(sb.str());
    }
  };

  class BuiltinType_Type : public BuiltinObjectType {
    BuiltinType_Type(const BuiltinType_Type&) = delete;
    BuiltinType_Type& operator=(const BuiltinType_Type&) = delete;
  public:
    BuiltinType_Type() : BuiltinObjectType("type") {}
    virtual Type makeUnion(IAllocator& allocator, const IType& rhs) const override {
      return TypeFactory::createUnionJoin(allocator, *this, rhs);
    }
    virtual Erratic<Type> queryPropertyType(const String& property) const override {
      if (property.equals("of")) {
        return Type::AnyQ; // WIBBLE
      }
      return Erratic<Type>::fail("Unknown built-in property: 'type.", property, "'");
    }
    virtual const IFunctionSignature* queryCallable() const override {
      return nullptr;
    }
  };

  class Builtin_TypeOf : public BuiltinFunction {
    Builtin_TypeOf(const Builtin_TypeOf&) = delete;
    Builtin_TypeOf& operator=(const Builtin_TypeOf&) = delete;
  public:
    explicit Builtin_TypeOf(IAllocator& allocator)
      : BuiltinFunction(allocator, Vanilla::Object, "type.of") {
      this->builder->addPositionalParameter(Type::String, "value", IFunctionSignatureParameter::Flags::Required);
      this->type = this->builder->build();
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      if (parameters.getPositionalCount() != 1) {
        return this->raiseBuiltin(execution, "accepts only 1 parameter, not ", parameters.getPositionalCount());
      }
      return this->makeValue(parameters.getPositional(0)->getRuntimeType().toString());
    }
  };

  class Builtin_Type : public BuiltinObject {
    Builtin_Type(const Builtin_Type&) = delete;
    Builtin_Type& operator=(const Builtin_Type&) = delete;
  public:
    explicit Builtin_Type(IAllocator& allocator, const BuiltinObjectType& type)
      : BuiltinObject(allocator, type) {
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      return this->raiseBuiltin(execution, "not yet implemented"); // WIBBLE
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      if (property.equals("of")) {
        return this->makeValue(ObjectFactory::create<Builtin_TypeOf>(allocator));
      }
      return this->raiseBuiltin(execution, "does not support property '", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      return this->raiseBuiltin(execution, "does not support assigning to properties such as '", property, "'");
    }
    virtual Value mutProperty(IExecution& execution, const String& property, Mutation, const Value&) override {
      return this->raiseBuiltin(execution, "does not support modifying properties such as '", property, "'");
    }
  };

  class BuiltinStringFunction : public BuiltinBase {
    BuiltinStringFunction(const BuiltinStringFunction&) = delete;
    BuiltinStringFunction& operator=(const BuiltinStringFunction&) = delete;
  public:
    using Function = Value(BuiltinStringFunction::*)(IExecution& execution, const IParameters& parameters) const;
    struct Property {
      Function function;
      Type type;
    };
  private:
    Function function;
    String string;
  public:
    BuiltinStringFunction(IAllocator& allocator, const String& name, Function function, const String& string)
      : BuiltinBase(allocator, name),
        function(function),
        string(string) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // There are no soft links to visit
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      sb.add('<', this->name, '>'); // WIBBLE
    }
    virtual Type getRuntimeType() const override {
      return nullptr; // WIBBLE
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raise(execution, "does not accept named parameters");
      }
      return (this->*function)(execution, parameters);
    }
    template<typename... ARGS>
    Value raise(IExecution& execution, ARGS&&... args) const {
      return execution.raiseFormat("Function '", this->name, "' ", std::forward<ARGS>(args)...);
    }
    Value compareTo(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameter, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      String other;
      if (!parameter->getString(other)) {
        return this->raise(execution, "expects its parameter to be a 'string', but got '", parameter->getRuntimeType().toString(), "' instead");
      }
      return this->makeValue(this->string.compareTo(other));
    }
    Value contains(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameter, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      String other;
      if (!parameter->getString(other)) {
        return this->raise(execution, "expects its parameter to be a 'string', but got '", parameter->getRuntimeType().toString(), "' instead");
      }
      return this->makeValue(this->string.contains(other));
    }
    Value endsWith(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameter, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      String other;
      if (!parameter->getString(other)) {
        return this->raise(execution, "expects its parameter to be a 'string', but got '", parameter->getRuntimeType().toString(), "' instead");
      }
      return this->makeValue(this->string.endsWith(other));
    }
    Value hash(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() > 0) {
        return this->raise(execution, "does not expect any parameters, but got ", parameters.getPositionalCount());
      }
      return this->makeValue(this->string.hash());
    }
    Value indexOf(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      String s0;
      if (!p0->getString(s0)) {
        return this->raise(execution, "expects its first parameter to be a 'string', but got '", p0->getRuntimeType().toString(), "' instead");
      }
      Int index;
      if (n == 1) {
        index = this->string.indexOfString(s0);
      } else {
        auto p1 = parameters.getPositional(1);
        if (p1->getNull()) {
          index = this->string.indexOfString(s0);
        } else {
          Int i1;
          if (!p1->getInt(i1)) {
            return this->raise(execution, "expects its optional second parameter to be an 'int', but got '", p1->getRuntimeType().toString(), "' instead");
          }
          if (i1 < 0) {
            return this->raise(execution, "expects its optional second parameter to be a non-negative 'int', but got ", i1, " instead");
          }
          index = this->string.indexOfString(s0, size_t(i1));
        }
      }
      return (index < 0) ? Value::Null : this->makeValue(index);
    }
    Value join(IExecution&, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      switch (n) {
      case 0:
        // Joining nothing always produces an empty string
        return this->makeValue(String()); // OPTIMIZE
      case 1:
        // Joining a single value does not require a separator
        return this->makeValue(parameters.getPositional(0)->toString());
      }
      // Our parameters aren't in a std::vector, so we replicate String::join() here
      auto separator = string.toUTF8();
      StringBuilder sb;
      sb.add(parameters.getPositional(0)->toString());
      for (size_t i = 1; i < n; ++i) {
        sb.add(separator).add(parameters.getPositional(i)->toString());
      }
      return this->makeValue(sb.str());
    }
    Value lastIndexOf(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      String s0;
      if (!p0->getString(s0)) {
        return this->raise(execution, "expects its first parameter to be a 'string', but got '", p0->getRuntimeType().toString(), "' instead");
      }
      Int index;
      if (n == 1) {
        index = this->string.lastIndexOfString(s0);
      } else {
        auto p1 = parameters.getPositional(1);
        if (p1->getNull()) {
          index = this->string.lastIndexOfString(s0);
        } else {
          Int i1;
          if (!p1->getInt(i1)) {
            return this->raise(execution, "expects its optional second parameter to be an 'int', but got '", p1->getRuntimeType().toString(), "' instead");
          }
          if (i1 < 0) {
            return this->raise(execution, "expects its optional second parameter to be a non-negative 'int', but got ", i1, " instead");
          }
          index = this->string.lastIndexOfString(s0, size_t(i1));
        }
      }
      return (index < 0) ? Value::Null : this->makeValue(index);
    }
    Value padLeft(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      Int i0;
      if (!p0->getInt(i0)) {
        return this->raise(execution, "expects its first parameter to be an 'int', but got '", p0->getRuntimeType().toString(), "' instead");
      }
      if (i0 < 0) {
        return this->raise(execution, "expects its first parameter to be a non-negative 'int', but got '", i0, "' instead");
      }
      if (n == 1) {
        return this->makeValue(this->string.padLeft(size_t(i0)));
      }
      auto p1 = parameters.getPositional(1);
      if (p1->getNull()) {
        return this->makeValue(this->string.padLeft(size_t(i0)));
      }
      String s1;
      if (!p1->getString(s1)) {
        return this->raise(execution, "expects its optional second parameter to be a 'string', but got '", p1->getRuntimeType().toString(), "' instead");
      }
      return this->makeValue(this->string.padLeft(size_t(i0), s1));
    }
    Value padRight(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      Int i0;
      if (!p0->getInt(i0)) {
        return this->raise(execution, "expects its first parameter to be an 'int', but got '", p0->getRuntimeType().toString(), "' instead");
      }
      if (i0 < 0) {
        return this->raise(execution, "expects its first parameter to be a non-negative 'int', but got '", i0, "' instead");
      }
      if (n == 1) {
        return this->makeValue(this->string.padRight(size_t(i0)));
      }
      auto p1 = parameters.getPositional(1);
      if (p1->getNull()) {
        return this->makeValue(this->string.padRight(size_t(i0)));
      }
      String s1;
      if (!p1->getString(s1)) {
        return this->raise(execution, "expects its optional second parameter to be a 'string', but got '", p1->getRuntimeType().toString(), "' instead");
      }
      return this->makeValue(this->string.padRight(size_t(i0), s1));
    }
    Value repeat(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameter, but got ", parameters.getPositionalCount());
      }
      auto p0 = parameters.getPositional(0);
      Int i0;
      if (!p0->getInt(i0)) {
        return this->raise(execution, "expects its parameter to be an 'int', but got '", p0->getRuntimeType().toString(), "' instead");
      }
      if (i0 < 0) {
        return this->raise(execution, "expects its parameter to be a non-negative 'int', but got ", i0, " instead");
      }
      return this->makeValue(this->string.repeat(size_t(i0)));
    }
    Value replace(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 2) && (n != 3)) {
        return this->raise(execution, "expects two or three parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      String s0;
      if (!p0->getString(s0)) {
        return this->raise(execution, "expects its first parameter to be a 'string', but got '", p0->getRuntimeType().toString(), "' instead");
      }
      auto p1 = parameters.getPositional(1);
      String s1;
      if (!p1->getString(s1)) {
        return this->raise(execution, "expects its second parameter to be a 'string', but got '", p1->getRuntimeType().toString(), "' instead");
      }
      if (n == 2) {
        return this->makeValue(this->string.replace(s0, s1));
      }
      auto p2 = parameters.getPositional(2);
      if (p2->getNull()) {
        return this->makeValue(this->string.replace(s0, s1));
      }
      Int i2;
      if (!p2->getInt(i2)) {
        return this->raise(execution, "expects its optional third parameter to be an 'int', but got '", p2->getRuntimeType().toString(), "' instead");
      }
      return this->makeValue(this->string.replace(s0, s1, i2));
    }
    Value slice(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      Int i0;
      if (!p0->getInt(i0)) {
        return this->raise(execution, "expects its first parameter to be an 'int', but got '", p0->getRuntimeType().toString(), "' instead");
      }
      if (n == 1) {
        return this->makeValue(this->string.slice(i0));
      }
      auto p1 = parameters.getPositional(1);
      if (p1->getNull()) {
        return this->makeValue(this->string.slice(i0));
      }
      Int i1;
      if (!p1->getInt(i1)) {
        return this->raise(execution, "expects its optional second parameter to be an 'int', but got '", p1->getRuntimeType().toString(), "' instead");
      }
      return this->makeValue(this->string.slice(i0, i1));
    }
    Value startsWith(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameter, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      String other;
      if (!parameter->getString(other)) {
        return this->raise(execution, "expects its parameter to be a 'string', but got '", parameter->getRuntimeType().toString(), "' instead");
      }
      return this->makeValue(this->string.startsWith(other));
    }
    Value toString(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() > 0) {
        return this->raise(execution, "does not expect any parameters, but got ", parameters.getPositionalCount());
      }
      return this->makeValue(this->string);
    }
    static Value createProperty(IAllocator& allocator, const String& string, const String& property) {
      // Treat 'length' as a special case
      if (property.equals("length")) {
        return ValueFactory::createInt(allocator, Int(string.length()));
      }
      auto* found = BuiltinStringFunction::findProperty(property);
      if (found != nullptr) {
        auto name = StringBuilder::concat("string.", found);
        return ValueFactory::createObject(allocator, ObjectFactory::create<BuiltinStringFunction>(allocator, name, found->function, string));
      }
      return Value::Void;
    }
    static Type queryProperty(const String& property) {
      // Treat 'length' as a special case
      if (property.equals("length")) {
        return Type::Int;
      }
      auto* found = BuiltinStringFunction::findProperty(property);
      if (found != nullptr) {
        return found->type;
      }
      return nullptr;
    }
  private:
    static const Property* findProperty(const String& property) {
      // Excluding 'length'
      static const std::map<String, Property> lookup = {
#define EGG_STRING_PROPERTY(name, WIBBLE) { #name, { &BuiltinStringFunction::name, Type::Int } },
      EGG_STRING_PROPERTY(compareTo, WIBBLE)
      EGG_STRING_PROPERTY(contains, WIBBLE)
      EGG_STRING_PROPERTY(endsWith, WIBBLE)
      EGG_STRING_PROPERTY(hash, WIBBLE)
      EGG_STRING_PROPERTY(indexOf, WIBBLE)
      EGG_STRING_PROPERTY(join, WIBBLE)
      EGG_STRING_PROPERTY(lastIndexOf, WIBBLE)
      EGG_STRING_PROPERTY(padLeft, WIBBLE)
      EGG_STRING_PROPERTY(padRight, WIBBLE)
      EGG_STRING_PROPERTY(repeat, WIBBLE)
      EGG_STRING_PROPERTY(replace, WIBBLE)
      EGG_STRING_PROPERTY(slice, WIBBLE)
      EGG_STRING_PROPERTY(startsWith, WIBBLE)
      EGG_STRING_PROPERTY(toString, WIBBLE)
#undef EGG_STRING_PROPERTY
      };
      auto found = lookup.find(property);
      if (found == lookup.end()) {
        return nullptr;
      }
      return &found->second;
    }
  };
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinAssert(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Assert>(allocator));
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinPrint(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Print>(allocator));
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinType(IAllocator& allocator) {
  static const BuiltinType_Type type;
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Type>(allocator, type));
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinString(IAllocator& allocator) {
  static const BuiltinType_String type;
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_String>(allocator, type));
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinStringProperty(IAllocator& allocator, const String& string, const String& property) {
  return BuiltinStringFunction::createProperty(allocator, string, property);
}

egg::ovum::Type egg::ovum::ValueFactory::queryBuiltinStringProperty(const String& property) {
  return BuiltinStringFunction::queryProperty(property);
}
