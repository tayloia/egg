#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"

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
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
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
    virtual Type makeUnion(IAllocator&, const IType& rhs) const override {
      assert(false); // WIBBLE
      return Type(&rhs);
    }
    virtual Assignable assignable(const IType&) const override {
      return Assignable::Never;
    }
    virtual const IFunctionSignature* callable() const override {
      return nullptr;
    }
    virtual const IIndexSignature* indexable() const override {
      return nullptr;
    }
    virtual const IDotSignature* dotable() const override {
      return nullptr;
    }
    virtual const IPointSignature* pointable() const override {
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("<" + this->name.toUTF8() + ">", 0);
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
    virtual Value toString() const override {
      return this->makeValue(StringBuilder::concat('<', this->name, '>'));
    }
    virtual Type getRuntimeType() const override {
      return Type(this->type.get());
    }
  };

  class BuiltinFunction : public BuiltinBase {
    BuiltinFunction(const BuiltinFunction&) = delete;
    BuiltinFunction& operator=(const BuiltinFunction&) = delete;
  protected:
    HardPtr<ITypeFunction> type;
  public:
    BuiltinFunction(IAllocator& allocator, const String& name, const Type& rettype)
      : BuiltinBase(allocator, StringBuilder::concat(name, "()")),
        type(TypeFactory::createFunction(allocator, name, rettype)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // There are no soft links to visit
    }
    virtual Value toString() const override {
      return this->makeValue(this->name);
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
      : BuiltinFunction(allocator, "assert", Type::Void) {
      this->type->addParameter("predicate", Type::Any, IFunctionSignatureParameter::Flags::Required);
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
      : BuiltinFunction(allocator, "print", Type::Void) {
      this->type->addParameter("values", Type::Any, IFunctionSignatureParameter::Flags::Variadic);
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

  class Builtin_TypeOf : public BuiltinFunction {
    Builtin_TypeOf(const Builtin_TypeOf&) = delete;
    Builtin_TypeOf& operator=(const Builtin_TypeOf&) = delete;
  public:
    explicit Builtin_TypeOf(IAllocator& allocator)
      : BuiltinFunction(allocator, "type.of", Type::Object) {
      this->type->addParameter("value", Type::String, IFunctionSignatureParameter::Flags::Required);
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
  };

  class BuiltinStringFunction : public BuiltinBase {
    BuiltinStringFunction(const BuiltinStringFunction&) = delete;
    BuiltinStringFunction& operator=(const BuiltinStringFunction&) = delete;
  public:
    using Function = Value(BuiltinStringFunction::*)(IExecution& execution, const IParameters& parameters) const;
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
    virtual Value toString() const override {
      return this->makeValue(StringBuilder::concat('<', this->name, '>')); // WIBBLE
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
        return this->raise(execution, "expects one parameters, but got ", parameters.getPositionalCount());
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
        return this->raise(execution, "expects one parameters, but got ", parameters.getPositionalCount());
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
        return this->raise(execution, "expects one parameters, but got ", parameters.getPositionalCount());
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
        return this->raise(execution, "expects one parameters, but got ", parameters.getPositionalCount());
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
    static Value makeFunction(IAllocator& allocator, const String& name, Function function, const String& string) {
      return ValueFactory::createObject(allocator, ObjectFactory::create<BuiltinStringFunction>(allocator, name, function, string));
    }
  };
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinAssert(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Assert>(allocator));
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinPrint(IAllocator& allocator) {
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Print>(allocator));
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinString(IAllocator& allocator) {
  static const BuiltinObjectType type{ "string" };
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_String>(allocator, type));
}

egg::ovum::Value egg::ovum::ValueFactory::createBuiltinType(IAllocator& allocator) {
  static const BuiltinObjectType type{ "type" };
  return ValueFactory::createObject(allocator, ObjectFactory::create<Builtin_Type>(allocator, type));
}

egg::ovum::Value egg::ovum::ValueFactory::createStringProperty(IAllocator& allocator, const String& string, const String& property) {
  // Treat 'length' as a special case
  if (property.equals("length")) {
    return ValueFactory::createInt(allocator, Int(string.length()));
  }
  // TODO optimize with lookup table
#define EGG_STRING_PROPERTY(name) if (property == #name) return BuiltinStringFunction::makeFunction(allocator, "string." #name, &BuiltinStringFunction::name, string)
  EGG_STRING_PROPERTY(compareTo);
  EGG_STRING_PROPERTY(contains);
  EGG_STRING_PROPERTY(endsWith);
  EGG_STRING_PROPERTY(hash);
  EGG_STRING_PROPERTY(indexOf);
  EGG_STRING_PROPERTY(join);
  EGG_STRING_PROPERTY(lastIndexOf);
  EGG_STRING_PROPERTY(padLeft);
  EGG_STRING_PROPERTY(padRight);
  EGG_STRING_PROPERTY(repeat);
  EGG_STRING_PROPERTY(replace);
  EGG_STRING_PROPERTY(slice);
  EGG_STRING_PROPERTY(startsWith);
  EGG_STRING_PROPERTY(toString);
#undef EGG_STRING_PROPERTY
  return Value::Void;
}
