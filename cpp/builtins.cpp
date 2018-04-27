#include "yolk.h"

namespace {
  using namespace egg::lang;

  class BuiltinSignatureParameter : public ISignatureParameter {
  private:
    String name; // may be empty
    ITypeRef type;
    size_t position; // may be SIZE_MAX
    bool required;
    bool variadic;
  public:
    BuiltinSignatureParameter(const std::string& name, const ITypeRef& type, size_t position, bool required, bool variadic)
      : name(String::fromUTF8(name)), type(type), position(position), required(required), variadic(variadic) {
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual const IType& getType() const override {
      return *this->type;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual bool isRequired() const override {
      return this->required;
    }
    virtual bool isVariadic() const override {
      return this->variadic;
    }
  };

  class BuiltinSignature : public ISignature {
    EGG_NO_COPY(BuiltinSignature);
  private:
    String name;
    ITypeRef returnType;
    std::vector<BuiltinSignatureParameter> parameters;
  public:
    BuiltinSignature(const std::string& name, const ITypeRef& returnType)
      : name(String::fromUTF8(name)), returnType(returnType) {
    }
    size_t getNextPosition() const {
      return this->parameters.size();
    }
    void addSignatureParameter(const std::string& parameterName, const ITypeRef& parameterType, size_t position, bool required, bool variadic) {
      this->parameters.emplace_back(parameterName, parameterType, position, required, variadic);
    }
    virtual String getFunctionName() const override {
      return this->name;
    }
    virtual const IType& getReturnType() const override {
      return *this->returnType;
    }
    virtual size_t getParameterCount() const override {
      return this->parameters.size();
    }
    virtual const ISignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters.size());
      return this->parameters[index];
    }
  };

  class BuiltinType : public egg::gc::NotReferenceCounted<IType> {
    EGG_NO_COPY(BuiltinType);
  private:
    BuiltinSignature signature;
  public:
    BuiltinType(const std::string& name, const ITypeRef& returnType)
      : signature(name, returnType) {
    }
    void addPositionalParameter(const std::string& name, const ITypeRef& type, bool required = true) {
      this->signature.addSignatureParameter(name, type, this->signature.getNextPosition(), required, false);
    }
    void addVariadicParameter(const std::string& name, const ITypeRef& type, bool required = true) {
      this->signature.addSignatureParameter(name, type, this->signature.getNextPosition(), required, true);
    }
    virtual String toString() const override {
      return this->signature.toString();
    }
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType&) const override {
      return execution.raiseFormat("Cannot re-assign built-in function '", this->getName(), "'");
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("Cannot re-assign built-in function '", this->getName(), "'");
    }
    virtual const ISignature* callable(IExecution&) const override {
      return &this->signature;
    }
    virtual String getName() const {
      return this->signature.getFunctionName();
    }
    Value validateCall(IExecution& execution, const IParameters& parameters) const {
      Value problem;
      if (!this->signature.validateCall(execution, parameters, problem)) {
        assert(problem.has(Discriminator::FlowControl));
        return problem;
      }
      return Value::Void;
    };
  };

  class Builtin : public egg::gc::NotReferenceCounted<IObject> {
    EGG_NO_COPY(Builtin);
  protected:
    BuiltinType type;
  public:
    Builtin(const std::string& name, const ITypeRef& returnType)
      : type(name, returnType) {
    }
    virtual bool dispose() override {
      // We don't allow disposing of builtins
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
    Print()
      : Builtin("print", Type::Void) {
      this->type.addVariadicParameter("...", Type::Any);
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      Value result = this->type.validateCall(execution, parameters);
      if (result.has(Discriminator::FlowControl)) {
        return result;
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

  class StringBuiltin : public egg::gc::HardReferenceCounted<IObject> {
    EGG_NO_COPY(StringBuiltin);
  protected:
    String instance;
    const BuiltinType* type;
  public:
    StringBuiltin(const String& instance, const BuiltinType& type)
      : HardReferenceCounted(0), instance(instance), type(&type) {
    }
  public:
    virtual bool dispose() override {
      // We don't allow disposing of builtins
      return false;
    }
    virtual Value toString() const override {
      return Value(this->type->getName());
    }
    virtual Value getRuntimeType() const override {
      // Fetch the runtime type
      return Value(*this->type);
    }
    template<typename T>
    static Value make(const String& instance) {
      static const BuiltinType& typeInstance = T::makeType();
      return Value::make<T>(instance, typeInstance);
    }
  };

  class StringContains : public StringBuiltin {
    EGG_NO_COPY(StringContains);
  public:
    StringContains(const String& instance, const BuiltinType& type) : StringBuiltin(instance, type) {}
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      Value result = this->type->validateCall(execution, parameters);
      if (result.has(Discriminator::FlowControl)) {
        return result;
      }
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return execution.raiseFormat("string.contains(): Parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      return Value{ this->instance.contains(needle.getString()) };
    }
    static const BuiltinType& makeType() {
      static BuiltinType instance("string.contains", Type::Bool);
      instance.addPositionalParameter("needle", Type::String);
      return instance;
    }
  };
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
    // This result is the actual length, not a function comupting it
    return Value{ int64_t(this->length()) };
  }
  if (name == "contains") {
    return StringBuiltin::make<StringContains>(*this);
  }
  return execution.raiseFormat("Unknown property for type 'string': '", property, "'");
}

egg::lang::Value egg::lang::Value::builtinPrint() {
  static Print builtin;
  return Value{ builtin };
}
