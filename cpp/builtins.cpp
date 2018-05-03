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
      return this->raise(execution, "Cannot re-assign built-in function");
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return this->raise(execution, "Cannot re-assign built-in function");
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
    template<typename... ARGS>
    Value raise(IExecution& execution, ARGS&&... args) const {
      // Use perfect forwarding to the constructor
      return execution.raiseFormat(this->signature.getFunctionName(), ": ", std::forward<ARGS>(args)...);
    }
  };

  template<typename T>
  class Builtin : public egg::gc::NotReferenceCounted<IObject> {
    EGG_NO_COPY(Builtin);
  protected:
    T type;
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

  class Print : public Builtin<BuiltinType> {
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

  template<typename T>
  class StringBuiltin : public egg::gc::HardReferenceCounted<IObject> {
    EGG_NO_COPY(StringBuiltin);
  protected:
    String instance;
    const T* type;
  public:
    StringBuiltin(const String& instance, const T& type)
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
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      // Let the string builtin type handle the request
      Value validation = this->type->validateCall(execution, parameters);
      if (validation.has(Discriminator::FlowControl)) {
        return validation;
      }
      return this->type->executeCall(execution, this->instance, parameters);
    }
    static Value make(const String& instance) {
      static const T typeInstance{};
      return Value::make<StringBuiltin<T>>(instance, typeInstance);
    }
  };

  class StringHashCode : public BuiltinType {
    EGG_NO_COPY(StringHashCode);
  public:
    StringHashCode()
      : BuiltinType("string.hashCode", Type::Int) {
    }
    Value executeCall(IExecution&, const String& instance, const IParameters&) const {
      // int hashCode()
      return Value{ instance.hashCode() };
    }
  };

  class StringToString : public BuiltinType {
    EGG_NO_COPY(StringToString);
  public:
    StringToString()
      : BuiltinType("string.toString", Type::String) {
    }
    Value executeCall(IExecution&, const String& instance, const IParameters&) const {
      // string toString()
      return Value{ instance };
    }
  };

  class StringContains : public BuiltinType {
    EGG_NO_COPY(StringContains);
  public:
    StringContains()
      : BuiltinType("string.contains", Type::Bool) {
      this->addPositionalParameter("needle", Type::String);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // bool contains(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      return Value{ instance.contains(needle.getString()) };
    }
  };

  class StringCompare : public BuiltinType {
    EGG_NO_COPY(StringCompare);
  public:
    StringCompare()
      : BuiltinType("string.compare", Type::Int) {
      this->addPositionalParameter("needle", Type::String);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO int compare(string other, int? start, int? other_start, int? max_length)
      auto other = parameters.getPositional(0);
      if (!other.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", other.getRuntimeType().toString(), "'");
      }
      return Value{ instance.compare(other.getString()) };
    }
  };

  class StringStartsWith : public BuiltinType {
    EGG_NO_COPY(StringStartsWith);
  public:
    StringStartsWith()
      : BuiltinType("string.startsWith", Type::Bool) {
      this->addPositionalParameter("needle", Type::String);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // bool startsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      return Value{ instance.startsWith(needle.getString()) };
    }
  };

  class StringEndsWith : public BuiltinType {
    EGG_NO_COPY(StringEndsWith);
  public:
    StringEndsWith()
      : BuiltinType("string.endsWith", Type::Bool) {
      this->addPositionalParameter("needle", Type::String);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // bool endsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      return Value{ instance.endsWith(needle.getString()) };
    }
  };

  class StringIndexOf : public BuiltinType {
    EGG_NO_COPY(StringIndexOf);
  public:
    StringIndexOf()
      : BuiltinType("string.indexOf", Type::makeSimple(Discriminator::Int | Discriminator::Null)) {
      this->addPositionalParameter("needle", Type::String);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO int? indexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      auto index = instance.indexOfString(needle.getString());
      return (index < 0) ? Value::Null : Value{ index };
    }
  };

  class StringLastIndexOf : public BuiltinType {
    EGG_NO_COPY(StringLastIndexOf);
  public:
    StringLastIndexOf()
      : BuiltinType("string.lastIndexOf", Type::makeSimple(Discriminator::Int | Discriminator::Null)) {
      this->addPositionalParameter("needle", Type::String);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO int? indexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      auto index = instance.lastIndexOfString(needle.getString());
      return (index < 0) ? Value::Null : Value{ index };
    }
  };

  class StringJoin : public BuiltinType {
    EGG_NO_COPY(StringJoin);
  public:
    StringJoin()
      : BuiltinType("string.join", Type::String) {
      this->addVariadicParameter("...", Type::Any);
    }
    Value executeCall(IExecution&, const String& instance, const IParameters& parameters) const {
      // TODO string join(...)
      auto n = parameters.getPositionalCount();
      switch (n) {
      case 0:
        // Joining nothing always produces an empty string
        return Value::EmptyString;
      case 1:
        // Joining a single value does not require a separator
        return Value{ parameters.getPositional(0).toString() };
      }
      auto separator = instance.toUTF8();
      StringBuilder sb;
      sb.add(parameters.getPositional(0).toUTF8());
      for (size_t i = 1; i < n; ++i) {
        sb.add(separator).add(parameters.getPositional(i).toUTF8());
      }
      return Value{ sb.str() };
    }
  };

  class StringSlice : public BuiltinType {
    EGG_NO_COPY(StringSlice);
  public:
    StringSlice()
      : BuiltinType("string.slice", Type::String) {
      this->addPositionalParameter("begin", Type::Int);
      this->addPositionalParameter("end", Type::Int, false);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO int? indexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto p0 = parameters.getPositional(0);
      if (!p0.is(Discriminator::Int)) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType().toString(), "'");
      }
      int64_t begin = p0.getInt();
      if (parameters.getPositionalCount() == 1) {
        return Value{ instance.slice(begin) };
      }

      auto p1 = parameters.getPositional(1);
      if (!p1.is(Discriminator::Int)) {
        return this->raise(execution, "Second parameter was expected to be an 'int', not '", p0.getRuntimeType().toString(), "'");
      }
      auto end = p1.getInt();
      return Value{ instance.slice(begin, end) };
    }
  };

}

egg::lang::Value egg::lang::String::builtin(egg::lang::IExecution& execution, const egg::lang::String& property) const {
  // See http://chilliant.blogspot.co.uk/2018/05/egg-strings.html
  //  
  //  string padLeft(int length, string? padding)
  //  string padRight(int length, string? padding)
  //  string repeat(int count)
  //  string replace(string needle, string replacement, int? maxOccurrences)
  //  string slice(int? begin, int? end)
  //  string split(string separator, int? limit)

  // OPTIMIZE
  auto name = property.toUTF8();
  if (name == "length") {
    // This result is the actual length, not a function computing it
    return Value{ int64_t(this->length()) };
  }
  if (name == "hashCode") {
    return StringBuiltin<StringHashCode>::make(*this);
  }
  if (name == "toString") {
    return StringBuiltin<StringToString>::make(*this);
  }
  if (name == "contains") {
    return StringBuiltin<StringContains>::make(*this);
  }
  if (name == "compare") {
    return StringBuiltin<StringCompare>::make(*this);
  }
  if (name == "startsWith") {
    return StringBuiltin<StringStartsWith>::make(*this);
  }
  if (name == "endsWith") {
    return StringBuiltin<StringEndsWith>::make(*this);
  }
  if (name == "indexOf") {
    return StringBuiltin<StringIndexOf>::make(*this);
  }
  if (name == "lastIndexOf") {
    return StringBuiltin<StringLastIndexOf>::make(*this);
  }
  if (name == "join") {
    return StringBuiltin<StringJoin>::make(*this);
  }
  if (name == "slice") {
    return StringBuiltin<StringSlice>::make(*this);
  }
  return execution.raiseFormat("Unknown property for type 'string': '", property, "'");
}

egg::lang::Value egg::lang::Value::builtinPrint() {
  static Print builtin;
  return Value{ builtin };
}
