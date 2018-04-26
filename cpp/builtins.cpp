#include "yolk.h"

namespace {
  using namespace egg::lang;

  class BuiltinType : public egg::gc::NotReferenceCounted<IType> {
    EGG_NO_COPY(BuiltinType);
  private:
    String name;
    String signature;
  public:
    BuiltinType(const String& name, const String& signature)
      : name(name), signature(signature) {
    }
    virtual String toString() const override {
      return this->signature;
    }
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType&) const override {
      return execution.raiseFormat("Cannot re-assign built-in function '", this->name, "'");
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("Cannot re-assign built-in function '", this->name, "'");
    }
    virtual String getName() const {
      return this->name;
    }
  };

  class Builtin : public egg::gc::NotReferenceCounted<IObject> {
    EGG_NO_COPY(Builtin);
  private:
    BuiltinType type;
  public:
    Builtin(const String& name, const String& signature)
      : type(name, signature) {
    }
    virtual bool dispose() override {
      // We don't allow disposing of built-ins
      return false;
    }
    virtual Value toString() const override {
      return Value(this->type.getName());
    }
    virtual Value getRuntimeType() const override {
      // Fetch the runtime type
      return Value(this->type);
    }
  };

  class Print : public Builtin {
    EGG_NO_COPY(Print);
  public:
    Print() : Builtin(String::fromUTF8("print"), String::fromUTF8("void print(...)")) {}
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat("print(): Named parameters are not supported");
      }
      auto n = parameters.getPositionalCount();
      std::string utf8;
      for (size_t i = 0; i < n; ++i) {
        auto parameter = parameters.getPositional(i);
        utf8 += parameter.toUTF8();
      }
      execution.print(utf8);
      return Value::Void;
    }
  };
  static Print BuiltinPrint;

  class StringBuiltin : public egg::gc::HardReferenceCounted<IObject> {
    EGG_NO_COPY(StringBuiltin);
  private:
    const BuiltinType* type;
  protected:
    String instance;
  public:
    StringBuiltin(const String& instance, const BuiltinType& type)
      : type(&type), instance(instance) {
    }
    virtual bool dispose() override {
      // We don't allow disposing of built-ins
      return false;
    }
    virtual Value toString() const override {
      return Value(this->type->getName());
    }
    virtual Value getRuntimeType() const override {
      // Fetch the runtime type
      return Value(*this->type);
    }
  };

  class StringContains : public StringBuiltin {
    EGG_NO_COPY(StringContains);
  private:
    static const BuiltinType type;
  public:
    explicit StringContains(const String& instance) : StringBuiltin(instance, type) {}
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat("string.contains(): Named parameters are not supported"); // TODO
      }
      if (parameters.getPositionalCount() != 1) {
        return execution.raiseFormat("string.contains(): Exactly one parameter was expected");
      }
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return execution.raiseFormat("string.contains(): Parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      return Value{ this->instance.contains(needle.getString()) };
    }
  };
  const BuiltinType StringContains::type(String::fromUTF8("string.contains"), String::fromUTF8("bool contains(string needle)"));
}

egg::lang::Value egg::lang::String::builtin(egg::lang::IExecution& execution, const egg::lang::String& property) const {
  // Specials
  //  string string(...)
  //  string operator[](int index)
  //  bool operator==(object other)
  //  iter iter()
  // Properties
  //  int length
  //  int compare(string other, int? start, int? other_start, int? max_length)
  //  bool contains(string needle)
  //  bool endsWith(string needle)
  //  int hash()
  //  int? indexOf(string needle, int? from_index, int? count, bool? negate)
  //  string join(...)
  //  int? lastIndexOf(string needle, int? from_index, int? count, bool? negate)
  //  string padLeft(int length, string? padding)
  //  string padRight(int length, string? padding)
  //  string repeat(int count)
  //  string replace(string needle, string replacement, int? max_occurrences)
  //  string slice(int? begin, int? end)
  //  string split(string separator, int? limit)
  //  bool startsWith(string needle)
  //  string toString()
  //  string trim(any? what)
  //  string trimLeft(any? what)
  //  string trimRight(any? what)
  // Missing
  //  format
  //  lowercase
  //  uppercase
  //  reverse
  //  codePointAt
  auto name = property.toUTF8();
  if (name == "length") {
    return Value{ int64_t(this->length()) };
  }
  if (name == "contains") {
    return Value::make<StringContains>(*this);
  }
  return execution.raiseFormat("Unknown property for type 'string': '", property, "'");
}

egg::lang::Value egg::lang::Value::Print{ BuiltinPrint };
