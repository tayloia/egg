#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"
#include "ovum/function.h"

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
    virtual Variant getProperty(IExecution& execution, const String& property) override {
      return this->raiseBuiltin(execution, "does not support properties such as '", property, "'");
    }
    virtual Variant setProperty(IExecution& execution, const String& property, const Variant&) override {
      return this->raiseBuiltin(execution, "does not support properties such as '", property, "'");
    }
    virtual Variant getIndex(IExecution& execution, const Variant&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Variant setIndex(IExecution& execution, const Variant&, const Variant&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Variant iterate(IExecution& execution) override {
      return this->raiseBuiltin(execution, "does not support iteration");
    }
    template<typename... ARGS>
    Variant raiseBuiltin(IExecution& execution, ARGS&&... args) {
      return execution.raiseFormat("Built-in '", this->name, "' ", std::forward<ARGS>(args)...);
    }
  };

  class BuiltinObjectType : public NotReferenceCounted<TypeBase> {
    BuiltinObjectType(const BuiltinObjectType&) = delete;
    BuiltinObjectType& operator=(const BuiltinObjectType&) = delete;
  private:
    String name;
  public:
    explicit BuiltinObjectType(const String& name)
      : name(name) {
      assert(!name.empty());
    }
    virtual Variant tryAssign(IExecution& execution, Variant&, const egg::ovum::Variant&) const override {
      return execution.raiseFormat("Cannot re-assign built-in value: '", this->name, "'");
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
    virtual Variant toString() const override {
      return StringBuilder::concat('<', this->name, '>');
    }
    virtual Type getRuntimeType() const override {
      return Type(this->type.get());
    }
  };

  class BuiltinFunctionType : public FunctionType {
    BuiltinFunctionType(const BuiltinFunctionType&) = delete;
    BuiltinFunctionType& operator=(const BuiltinFunctionType&) = delete;
  public:
    BuiltinFunctionType(IAllocator& allocator, const String& name, const Type& rettype)
      : FunctionType(allocator, name, rettype) {
    }
    virtual Variant tryAssign(IExecution& execution, Variant&, const egg::ovum::Variant&) const override {
      return execution.raiseFormat("Cannot re-assign built-in value: '", this->signature.getFunctionName(), "'");
    }
  };

  class BuiltinFunction : public BuiltinBase {
    BuiltinFunction(const BuiltinFunction&) = delete;
    BuiltinFunction& operator=(const BuiltinFunction&) = delete;
  protected:
    HardPtr<BuiltinFunctionType> type;
  public:
    BuiltinFunction(IAllocator& allocator, const String& name, const Type& rettype)
      : BuiltinBase(allocator, StringBuilder::concat(name, "()")),
      type(allocator.make<BuiltinFunctionType>(name, rettype)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // There are no soft links to visit
    }
    virtual Variant toString() const override {
      return this->name;
    }
    virtual Type getRuntimeType() const override {
      return Type(this->type.get());
    }
  };

  class BuiltinAssert : public BuiltinFunction {
    BuiltinAssert(const BuiltinAssert&) = delete;
    BuiltinAssert& operator=(const BuiltinAssert&) = delete;
  public:
    explicit BuiltinAssert(IAllocator& allocator)
      : BuiltinFunction(allocator, "assert", Type::Void) {
      this->type->addParameter("predicate", Type::Any, IFunctionSignatureParameter::Flags::Required);
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      if (parameters.getPositionalCount() != 1) {
        return this->raiseBuiltin(execution, "accepts only 1 parameter, not ", parameters.getPositionalCount());
      }
      return execution.assertion(parameters.getPositional(0).direct());
    }
  };

  class BuiltinPrint : public BuiltinFunction {
    BuiltinPrint(const BuiltinPrint&) = delete;
    BuiltinPrint& operator=(const BuiltinPrint&) = delete;
  public:
    explicit BuiltinPrint(IAllocator& allocator)
      : BuiltinFunction(allocator, "print", Type::Void) {
      this->type->addParameter("values", Type::Any, IFunctionSignatureParameter::Flags::Variadic);
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      StringBuilder sb;
      auto n = parameters.getPositionalCount();
      for (size_t i = 0; i < n; ++i) {
        sb.add(parameters.getPositional(i).toString());
      }
      execution.print(sb.toUTF8());
      return Variant::Void;
    }
  };

  class BuiltinString : public BuiltinObject {
    BuiltinString(const BuiltinString&) = delete;
    BuiltinString& operator=(const BuiltinString&) = delete;
  public:
    explicit BuiltinString(IAllocator& allocator, const BuiltinObjectType& type)
      : BuiltinObject(allocator, type) {
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      StringBuilder sb;
      auto n = parameters.getPositionalCount();
      for (size_t i = 0; i < n; ++i) {
        sb.add(parameters.getPositional(i).toString());
      }
      return sb.str();
    }
  };

  class BuiltinStringFunction : public BuiltinBase {
    BuiltinStringFunction(const BuiltinStringFunction&) = delete;
    BuiltinStringFunction& operator=(const BuiltinStringFunction&) = delete;
  public:
    using Function = Variant(BuiltinStringFunction::*)(IExecution& execution, const IParameters& parameters) const;
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
    virtual Variant toString() const override {
      return StringBuilder::concat('<', this->name, '>'); // WIBBLE
    }
    virtual Type getRuntimeType() const override {
      return nullptr; // WIBBLE
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raise(execution, "does not accept named parameters");
      }
      return (this->*function)(execution, parameters);
    }
    template<typename... ARGS>
    Variant raise(IExecution& execution, ARGS&&... args) const {
      return execution.raiseFormat("Function '", this->name, "' ", std::forward<ARGS>(args)...);
    }
    Variant compareTo(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameters, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      if (!parameter.isString()) {
        return this->raise(execution, "expects its parameter to be a 'string', but got '", parameter.getRuntimeType().toString(), "' instead");
      }
      return this->string.compareTo(parameter.getString());
    }
    Variant contains(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameters, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      if (!parameter.isString()) {
        return this->raise(execution, "expects its parameter to be a 'string', but got '", parameter.getRuntimeType().toString(), "' instead");
      }
      return this->string.contains(parameter.getString());
    }
    Variant endsWith(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameters, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      if (!parameter.isString()) {
        return this->raise(execution, "expects its parameter to be a 'string', but got '", parameter.getRuntimeType().toString(), "' instead");
      }
      return this->string.endsWith(parameter.getString());
    }
    Variant hash(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() > 0) {
        return this->raise(execution, "does not expect any parameters, but got ", parameters.getPositionalCount());
      }
      return this->string.hash();
    }
    Variant indexOf(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      if (!p0.isString()) {
        return this->raise(execution, "expects its first parameter to be a 'string', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      Int index;
      if (n == 1) {
        index = this->string.indexOfString(p0.getString());
      } else {
        auto p1 = parameters.getPositional(1);
        if (p1.isNull()) {
          index = this->string.indexOfString(p0.getString());
        } else {
          if (!p1.isInt()) {
            return this->raise(execution, "expects its optional second parameter to be an 'int', but got '", p1.getRuntimeType().toString(), "' instead");
          }
          auto from = p1.getInt();
          if (from < 0) {
            return this->raise(execution, "expects its optional second parameter to be a non-negative 'int', but got ", from, " instead");
          }
          index = this->string.indexOfString(p0.getString(), size_t(from));
        }
      }
      return (index < 0) ? Variant::Null : index;
    }
    Variant join(IExecution&, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      switch (n) {
      case 0:
        // Joining nothing always produces an empty string
        return Variant::EmptyString;
      case 1:
        // Joining a single value does not require a separator
        return parameters.getPositional(0).toString();
      }
      // Our parameters aren't in a std::vector, so we replicate String::join() here
      auto separator = string.toUTF8();
      StringBuilder sb;
      sb.add(parameters.getPositional(0).toString());
      for (size_t i = 1; i < n; ++i) {
        sb.add(separator).add(parameters.getPositional(i).toString());
      }
      return sb.str();
    }
    Variant lastIndexOf(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      if (!p0.isString()) {
        return this->raise(execution, "expects its first parameter to be a 'string', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      Int index;
      if (n == 1) {
        index = this->string.lastIndexOfString(p0.getString());
      } else {
        auto p1 = parameters.getPositional(1);
        if (p1.isNull()) {
          index = this->string.lastIndexOfString(p0.getString());
        } else {
          if (!p1.isInt()) {
            return this->raise(execution, "expects its optional second parameter to be an 'int', but got '", p1.getRuntimeType().toString(), "' instead");
          }
          auto from = p1.getInt();
          if (from < 0) {
            return this->raise(execution, "expects its optional second parameter to be a non-negative 'int', but got ", from, " instead");
          }
          index = this->string.lastIndexOfString(p0.getString(), size_t(from));
        }
      }
      return (index < 0) ? Variant::Null : index;
    }
    Variant padLeft(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "expects its first parameter to be an 'int', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      auto target = p0.getInt();
      if (target < 0) {
        return this->raise(execution, "expects its first parameter to be a non-negative 'int', but got '", target, "' instead");
      }
      if (n == 1) {
        return this->string.padLeft(size_t(target));
      }
      auto p1 = parameters.getPositional(1);
      if (p1.isNull()) {
        return this->string.padLeft(size_t(target));
      }
      if (!p1.isString()) {
        return this->raise(execution, "expects its optional second parameter to be a 'string', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      return this->string.padLeft(size_t(target), p1.getString());
    }
    Variant padRight(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "expects its first parameter to be an 'int', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      auto target = p0.getInt();
      if (target < 0) {
        return this->raise(execution, "expects its first parameter to be a non-negative 'int', but got '", target, "' instead");
      }
      if (n == 1) {
        return this->string.padRight(size_t(target));
      }
      auto p1 = parameters.getPositional(1);
      if (p1.isNull()) {
        return this->string.padRight(size_t(target));
      }
      if (!p1.isString()) {
        return this->raise(execution, "expects its optional second parameter to be a 'string', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      return this->string.padRight(size_t(target), p1.getString());
    }
    Variant repeat(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameter, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      if (!parameter.isInt()) {
        return this->raise(execution, "expects its parameter to be an 'int', but got '", parameter.getRuntimeType().toString(), "' instead");
      }
      auto count = parameter.getInt();
      if (count < 0) {
        return this->raise(execution, "expects its parameter to be a non-negative 'int', but got ", count, " instead");
      }
      return this->string.repeat(size_t(count));
    }
    Variant replace(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 2) && (n != 3)) {
        return this->raise(execution, "expects two or three parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      if (!p0.isString()) {
        return this->raise(execution, "expects its first parameter to be a 'string', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.isString()) {
        return this->raise(execution, "expects its second parameter to be a 'string', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      if (n == 2) {
        return this->string.replace(p0.getString(), p1.getString());
      }
      auto p2 = parameters.getPositional(2);
      if (p2.isNull()) {
        return this->string.replace(p0.getString(), p1.getString());
      }
      if (!p2.isInt()) {
        return this->raise(execution, "expects its optional third parameter to be an 'int', but got '", p1.getRuntimeType().toString(), "' instead");
      }
      return this->string.replace(p0.getString(), p1.getString(), p2.getInt());
    }
    Variant slice(IExecution& execution, const IParameters& parameters) const {
      auto n = parameters.getPositionalCount();
      if ((n != 1) && (n != 2)) {
        return this->raise(execution, "expects one or two parameters, but got ", n);
      }
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "expects its first parameter to be an 'int', but got '", p0.getRuntimeType().toString(), "' instead");
      }
      if (n == 1) {
        return this->string.slice(p0.getInt());
      }
      auto p1 = parameters.getPositional(1);
      if (p1.isNull()) {
        return this->string.slice(p0.getInt());
      }
      if (!p1.isInt()) {
        return this->raise(execution, "expects its optional second parameter to be an 'int', but got '", p1.getRuntimeType().toString(), "' instead");
      }
      return this->string.slice(p0.getInt(), p1.getInt());
    }
    Variant startsWith(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() != 1) {
        return this->raise(execution, "expects one parameters, but got ", parameters.getPositionalCount());
      }
      auto parameter = parameters.getPositional(0);
      if (!parameter.isString()) {
        return this->raise(execution, "expects its parameter to be a 'string', but got '", parameter.getRuntimeType().toString(), "' instead");
      }
      return this->string.startsWith(parameter.getString());
    }
    Variant toString(IExecution& execution, const IParameters& parameters) const {
      if (parameters.getPositionalCount() > 0) {
        return this->raise(execution, "does not expect any parameters, but got ", parameters.getPositionalCount());
      }
      return this->string;
    }
    static Variant make(IAllocator& allocator, const String& name, Function function, const String& string) {
      return VariantFactory::createObject<BuiltinStringFunction>(allocator, name, function, string);
    }
  };
}

egg::ovum::Variant egg::ovum::VariantFactory::createBuiltinAssert(IAllocator& allocator) {
  return VariantFactory::createObject<BuiltinAssert>(allocator);
}

egg::ovum::Variant egg::ovum::VariantFactory::createBuiltinPrint(IAllocator& allocator) {
  return VariantFactory::createObject<BuiltinPrint>(allocator);
}

egg::ovum::Variant egg::ovum::VariantFactory::createBuiltinString(IAllocator& allocator) {
  static const BuiltinObjectType type{ "string" };
  return VariantFactory::createObject<BuiltinString>(allocator, type);
}

egg::ovum::Variant egg::ovum::VariantFactory::createStringProperty(IAllocator& allocator, const String& string, const String& property) {
  // Treat 'length' as a special case
  if (property.equals("length")) {
    return Int(string.length());
  }
  // TODO optimize with lookup table
#define EGG_STRING_PROPERTY(name) if (property == #name) return BuiltinStringFunction::make(allocator, "string." #name, &BuiltinStringFunction::name, string)
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
  return Variant::Void;
}
