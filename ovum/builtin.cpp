#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/vanilla.h"

#include <map>

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
      return this->raiseBuiltin(execution, "does not support pointer referencing with '*'");
    }
    virtual Value setPointee(IExecution& execution, const Value&) override {
      return this->raiseBuiltin(execution, "does not support pointer referencing with '*'");
    }
    virtual Value mutPointee(IExecution& execution, Mutation, const Value&) override {
      return this->raiseBuiltin(execution, "does not support pointer referencing with '*'");
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

  class BuiltinType_Base : public NotReferenceCounted<IType> {
    BuiltinType_Base(const BuiltinType_Base&) = delete;
    BuiltinType_Base& operator=(const BuiltinType_Base&) = delete;
  private:
    String name;
  public:
    explicit BuiltinType_Base(const String& name)
      : name(name) {
      assert(!name.empty());
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual Assignability tryAssign(IAllocator&, ISlot&, const Value&) const override {
      return Assignability::Readonly;
    }
    virtual Assignability tryMutate(IAllocator&, ISlot&, Mutation, const Value&) const override {
      return Assignability::Readonly;
    }
    virtual Assignability queryAssignable(const IType&) const override {
      return Assignability::Readonly;
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
      return std::make_pair("<builtin-" + this->name.toUTF8() + ">", 0);
    }
    virtual String describeValue() const override {
      return StringBuilder::concat("Type '", this->name, "'");
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

  class BuiltinType_String : public BuiltinType_Base {
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
    class PropertySignature : public IPropertySignature {
    public:
      virtual Type getType(const String&) const override {
        return {};
      }
      virtual Modifiability getModifiability(const String&) const override {
        return Modifiability::None;
      }
      virtual String getName(size_t) const override {
        return {};
      }
    };
  public:
    BuiltinType_String() : BuiltinType_Base("string") {}
    virtual Type makeUnion(IAllocator& allocator, const IType& rhs) const override {
      return TypeFactory::createUnionJoin(allocator, *this, rhs);
    }
    virtual const IFunctionSignature* queryCallable() const override {
      static const FunctionSignature signature;
      return &signature;
    }
    virtual const IPropertySignature* queryDotable() const override {
      static const PropertySignature signature;
      return &signature;
    }
  };

  class Builtin_String : public Builtin_Base {
    Builtin_String(const Builtin_String&) = delete;
    Builtin_String& operator=(const Builtin_String&) = delete;
  public:
    explicit Builtin_String(IAllocator& allocator, const BuiltinType_Base& type)
      : Builtin_Base(allocator, type) {
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

  class BuiltinType_Type : public BuiltinType_Base {
    BuiltinType_Type(const BuiltinType_Type&) = delete;
    BuiltinType_Type& operator=(const BuiltinType_Type&) = delete;
  private:
    class PropertySignature : public IPropertySignature {
    public:
      virtual Type getType(const String& property) const override {
        if (property.equals("of")) {
          return Type::AnyQ; // WIBBLE
        }
        return nullptr;
      }
      virtual Modifiability getModifiability(const String& property) const override {
        if (property.equals("of")) {
          return Modifiability::Read; // WIBBLE
        }
        return Modifiability::None;
      }
      virtual String getName(size_t index) const override {
        if (index == 0) {
          return "of"; // WIBBLE
        }
        return {};
      }
    };
  public:
    BuiltinType_Type() : BuiltinType_Base("type") {}
    virtual Type makeUnion(IAllocator& allocator, const IType& rhs) const override {
      return TypeFactory::createUnionJoin(allocator, *this, rhs);
    }
    virtual const IFunctionSignature* queryCallable() const override {
      return nullptr;
    }
    virtual const IPropertySignature* queryDotable() const override {
      static const PropertySignature signature;
      return &signature;
    }
    virtual const IPointerSignature* queryPointable() const override {
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

  class Builtin_Type : public Builtin_Base {
    Builtin_Type(const Builtin_Type&) = delete;
    Builtin_Type& operator=(const Builtin_Type&) = delete;
  public:
    explicit Builtin_Type(IAllocator& allocator, const BuiltinType_Base& type)
      : Builtin_Base(allocator, type) {
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
      auto* found = BuiltinStringFunction::findPropertyByName(property);
      if (found != nullptr) {
        auto name = StringBuilder::concat("string.", found);
        return ValueFactory::createObject(allocator, ObjectFactory::create<BuiltinStringFunction>(allocator, name, found->function, string));
      }
      return Value::Void;
    }
    static Modifiability queryPropertyModifiability(const String& property) {
      // Treat 'length' as a special case
      if (property.equals("length")) {
        return Modifiability::Read;
      }
      auto* found = BuiltinStringFunction::findPropertyByName(property);
      if (found != nullptr) {
        return Modifiability::Read;
      }
      return Modifiability::None;
    }
    static Type queryPropertyType(const String& property) {
      // Treat 'length' as a special case
      if (property.equals("length")) {
        return Type::Int;
      }
      auto* found = BuiltinStringFunction::findPropertyByName(property);
      if (found != nullptr) {
        return found->type;
      }
      return nullptr;
    }
    static String queryPropertyName(size_t index) {
      auto* found = BuiltinStringFunction::findPropertyByIndex(index);
      if (found != nullptr) {
        return *found;
      }
      return {};
    }
  private:
    static const Property* findPropertyByName(const String& property) {
      // Excluding 'length'
#define EGG_STRING_PROPERTY_MAP(name, WIBBLE) { #name, { &BuiltinStringFunction::name, Type::Int } },
      static const std::map<String, Property> lookup = {
        EGG_STRING_PROPERTIES(EGG_STRING_PROPERTY_MAP)
      };
#undef EGG_STRING_PROPERTY_MAP
      auto found = lookup.find(property);
      if (found == lookup.end()) {
        return nullptr;
      }
      return &found->second;
    }
    static const String* findPropertyByIndex(size_t index) {
      // Including 'length'
#define EGG_STRING_PROPERTY_LIST(name, _) , #name
      static const String lookup[] = {
        "length"
        EGG_STRING_PROPERTIES(EGG_STRING_PROPERTY_LIST)
      };
#undef EGG_STRING_PROPERTY_LIST
      if (index < (sizeof(lookup) / sizeof(lookup[0]))) {
        return &lookup[index];
      }
      return nullptr;
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

egg::ovum::Type egg::ovum::ValueFactory::queryBuiltinStringPropertyType(const String& property) {
  return BuiltinStringFunction::queryPropertyType(property);
}

egg::ovum::Modifiability egg::ovum::ValueFactory::queryBuiltinStringPropertyModifiability(const String& property) {
  return BuiltinStringFunction::queryPropertyModifiability(property);
}

egg::ovum::String egg::ovum::ValueFactory::queryBuiltinStringPropertyName(size_t index) {
  return BuiltinStringFunction::queryPropertyName(index);
}
