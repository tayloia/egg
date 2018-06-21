#include "yolk.h"

namespace {
  using namespace egg::lang;
  using Flags = IFunctionSignatureParameter::Flags;

  class BuiltinSignatureParameter : public IFunctionSignatureParameter {
  private:
    String name; // may be empty
    ITypeRef type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    BuiltinSignatureParameter(const std::string& name, const ITypeRef& type, size_t position, Flags flags)
      : name(String::fromUTF8(name)), type(type), position(position), flags(flags) {
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
    virtual Flags getFlags() const override {
      return this->flags;
    }
  };

  class BuiltinSignature : public IFunctionSignature {
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
    void addSignatureParameter(const std::string& parameterName, const ITypeRef& parameterType, size_t position, BuiltinSignatureParameter::Flags flags) {
      this->parameters.emplace_back(parameterName, parameterType, position, flags);
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
    virtual const IFunctionSignatureParameter& getParameter(size_t index) const override {
      assert(index < this->parameters.size());
      return this->parameters[index];
    }
  };

  class BuiltinFunctionType : public egg::gc::HardReferenceCounted<IType> {
    EGG_NO_COPY(BuiltinFunctionType);
  private:
    BuiltinSignature signature;
  public:
    BuiltinFunctionType(const std::string& name, const ITypeRef& returnType)
      : HardReferenceCounted(0), signature(name, returnType) {
    }
    void addParameter(const std::string& name, const ITypeRef& type, IFunctionSignatureParameter::Flags flags) {
      this->signature.addSignatureParameter(name, type, this->signature.getNextPosition(), flags);
    }
    virtual String toString() const override {
      // Exclude the names
      return this->signature.toString(false);
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rtype) const {
      // We can assign if the signatures are the same (TODO equal?)
      if (&this->signature == rtype.callable()) {
        return AssignmentSuccess::Always;
      }
      return AssignmentSuccess::Never;
    }
    virtual const IFunctionSignature* callable() const override {
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

  class BuiltinObjectType : public BuiltinFunctionType {
    EGG_NO_COPY(BuiltinObjectType);
  private:
    egg::yolk::DictionaryUnordered<String, Value> properties;
  public:
    BuiltinObjectType(const std::string& name, const ITypeRef& returnType)
      : BuiltinFunctionType(name, returnType) {
    }
    void addProperty(const String& name, const Value& value) {
      this->properties.addOrUpdate(name, value);
    }
    bool tryGetProperty(const String& name, Value& value) const {
      return this->properties.tryGet(name, value);
    }
    virtual bool dotable(const String* property, ITypeRef& type, String& reason) const {
      if (property == nullptr) {
        type = Type::AnyQ;
        return true;
      }
      Value value;
      if (this->properties.tryGet(*property, value)) {
        type = value.getRuntimeType();
        return true;
      }
      reason = String::concat("Unknown built-in property: '", this->getName(), ".", *property, "'");
      return false;
    }
  };

  class BuiltinFunction : public egg::gc::HardReferenceCounted<IObject> {
    EGG_NO_COPY(BuiltinFunction);
  protected:
    egg::gc::HardRef<BuiltinFunctionType> type;
  public:
    BuiltinFunction(const std::string& name, const ITypeRef& returnType)
      : HardReferenceCounted(0), type(new BuiltinFunctionType(name, returnType)) {
    }
    virtual bool dispose() override {
      // We don't allow disposing of builtins
      return false;
    }
    virtual Value toString() const override {
      return Value(this->type->getName());
    }
    virtual const IType& getRuntimeType() const override {
      return *this->type;
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support properties such as '.", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support properties such as '.", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual Value iterate(IExecution& execution) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support iteration");
    }
  };

  class BuiltinObject : public egg::gc::HardReferenceCounted<IObject> {
    EGG_NO_COPY(BuiltinObject);
  protected:
    egg::gc::HardRef<BuiltinObjectType> type;
  public:
    BuiltinObject(const std::string& name, const ITypeRef& returnType)
      : HardReferenceCounted(0), type(new BuiltinObjectType(name, returnType)) {
    }
    void addProperty(const std::string& name, const Value& value) {
      this->type->addProperty(String::fromUTF8(name), value);
    }
    virtual bool dispose() override {
      // We don't allow disposing of builtins
      return false;
    }
    virtual Value toString() const override {
      return Value(this->type->getName());
    }
    virtual const IType& getRuntimeType() const override {
      return *this->type;
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (this->type->tryGetProperty(property, value)) {
        return value;
      }
      return execution.raiseFormat("Unknown built-in property: '", this->type->getName(), ".", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      return execution.raiseFormat("Cannot set built-in property: '", this->type->getName(), ".", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual Value iterate(IExecution& execution) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support iteration");
    }
  };

  class BuiltinStringFrom : public BuiltinFunction {
    EGG_NO_COPY(BuiltinStringFrom);
  public:
    BuiltinStringFrom()
      : BuiltinFunction("string.from", Type::makeSimple(Discriminator::String | Discriminator::Null)) {
      this->type->addParameter("value", Type::AnyQ, Flags::Required);
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      // Convert the parameter to a string
      // Note: Although the return type is 'string?' (for orthogonality) this function never returns 'null'
      Value result = this->type->validateCall(execution, parameters);
      if (result.has(Discriminator::FlowControl)) {
        return result;
      }
      return Value{ parameters.getPositional(0).toString() };
    }
  };

  class BuiltinString : public BuiltinObject {
    EGG_NO_COPY(BuiltinString);
  public:
    BuiltinString()
      : BuiltinObject("string", Type::String) {
      // The function call looks like: 'string string(any?... value)'
      this->type->addParameter("value", Type::AnyQ, Flags::Variadic);
      this->addProperty("from", Value::make<BuiltinStringFrom>());
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      // Concatenate the string representations of all parameters
      Value result = this->type->validateCall(execution, parameters);
      if (result.has(Discriminator::FlowControl)) {
        return result;
      }
      auto n = parameters.getPositionalCount();
      switch (n) {
      case 0:
        return Value::EmptyString;
      case 1:
        return Value{ parameters.getPositional(0).toString() };
      }
      StringBuilder sb;
      for (size_t i = 0; i < n; ++i) {
        sb.add(parameters.getPositional(i).toString());
      }
      return Value{ sb.str() };
    }
  };

  class BuiltinTypeOf : public BuiltinFunction {
    EGG_NO_COPY(BuiltinTypeOf);
  public:
    BuiltinTypeOf()
      : BuiltinFunction("type.of", Type::Type_) {
      this->type->addParameter("value", Type::AnyQ, Flags::Required);
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      // Fetch the runtime type of the parameter
      Value result = this->type->validateCall(execution, parameters);
      if (result.has(Discriminator::FlowControl)) {
        return result;
      }
      return Value{ parameters.getPositional(0).getRuntimeType()->toString() };
    }
  };

  class BuiltinType : public BuiltinObject {
    EGG_NO_COPY(BuiltinType);
  public:
    BuiltinType()
      : BuiltinObject("type", Type::Type_) {
      // The function call looks like: 'type type(any?... value)'
      this->type->addParameter("value", Type::AnyQ, Flags::Variadic);
      this->addProperty("of", Value::make<BuiltinTypeOf>());
    }
    virtual Value call(IExecution&, const IParameters&) override {
      // TODO
      return Value::Null;
    }
  };

  class BuiltinAssert : public BuiltinFunction {
    EGG_NO_COPY(BuiltinAssert);
  public:
    BuiltinAssert()
      : BuiltinFunction("assert", Type::Void) {
      this->type->addParameter("predicate", Type::Any, Bits::set(Flags::Required, Flags::Predicate));
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      Value result = this->type->validateCall(execution, parameters);
      if (result.has(Discriminator::FlowControl)) {
        return result;
      }
      return execution.assertion(parameters.getPositional(0));
    }
  };

  class BuiltinPrint : public BuiltinFunction {
    EGG_NO_COPY(BuiltinPrint);
  public:
    BuiltinPrint()
      : BuiltinFunction("print", Type::Void) {
      this->type->addParameter("...", Type::Any, Flags::Variadic);
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      Value result = this->type->validateCall(execution, parameters);
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
    virtual const IType& getRuntimeType() const override {
      return *this->type;
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      // Let the string builtin type handle the request
      Value validation = this->type->validateCall(execution, parameters);
      if (validation.has(Discriminator::FlowControl)) {
        return validation;
      }
      return this->type->executeCall(execution, this->instance, parameters);
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      return execution.raiseFormat(this->type->toString(), " does not support properties such as '.", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support properties such as '.", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return execution.raiseFormat(this->type->toString(), " does not support indexing with '[]'");
    }
    virtual Value iterate(IExecution& execution) override {
      return execution.raiseFormat(this->type->toString(), " does not support iteration");
    }
    static Value make(const String& instance) {
      static egg::gc::HardRef<T> type(new T());
      return Value::make<StringBuiltin<T>>(instance, *type);
    }
  };

  class StringHashCode : public BuiltinFunctionType {
    EGG_NO_COPY(StringHashCode);
  public:
    StringHashCode()
      : BuiltinFunctionType("string.hashCode", Type::Int) {
    }
    Value executeCall(IExecution&, const String& instance, const IParameters&) const {
      // int hashCode()
      return Value{ instance.hashCode() };
    }
  };

  class StringToString : public BuiltinFunctionType {
    EGG_NO_COPY(StringToString);
  public:
    StringToString()
      : BuiltinFunctionType("string.toString", Type::String) {
    }
    Value executeCall(IExecution&, const String& instance, const IParameters&) const {
      // string toString()
      return Value{ instance };
    }
  };

  class StringContains : public BuiltinFunctionType {
    EGG_NO_COPY(StringContains);
  public:
    StringContains()
      : BuiltinFunctionType("string.contains", Type::Bool) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // bool contains(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return Value{ instance.contains(needle.getString()) };
    }
  };

  class StringCompare : public BuiltinFunctionType {
    EGG_NO_COPY(StringCompare);
  public:
    StringCompare()
      : BuiltinFunctionType("string.compare", Type::Int) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO int compare(string other, int? start, int? other_start, int? max_length)
      auto other = parameters.getPositional(0);
      if (!other.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", other.getRuntimeType()->toString(), "'");
      }
      return Value{ instance.compare(other.getString()) };
    }
  };

  class StringStartsWith : public BuiltinFunctionType {
    EGG_NO_COPY(StringStartsWith);
  public:
    StringStartsWith()
      : BuiltinFunctionType("string.startsWith", Type::Bool) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // bool startsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return Value{ instance.startsWith(needle.getString()) };
    }
  };

  class StringEndsWith : public BuiltinFunctionType {
    EGG_NO_COPY(StringEndsWith);
  public:
    StringEndsWith()
      : BuiltinFunctionType("string.endsWith", Type::Bool) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // bool endsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return Value{ instance.endsWith(needle.getString()) };
    }
  };

  class StringIndexOf : public BuiltinFunctionType {
    EGG_NO_COPY(StringIndexOf);
  public:
    StringIndexOf()
      : BuiltinFunctionType("string.indexOf", Type::makeSimple(Discriminator::Int | Discriminator::Null)) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO int? indexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      auto index = instance.indexOfString(needle.getString());
      return (index < 0) ? Value::Null : Value{ index };
    }
  };

  class StringLastIndexOf : public BuiltinFunctionType {
    EGG_NO_COPY(StringLastIndexOf);
  public:
    StringLastIndexOf()
      : BuiltinFunctionType("string.lastIndexOf", Type::makeSimple(Discriminator::Int | Discriminator::Null)) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO int? lastIndexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      auto index = instance.lastIndexOfString(needle.getString());
      return (index < 0) ? Value::Null : Value{ index };
    }
  };

  class StringJoin : public BuiltinFunctionType {
    EGG_NO_COPY(StringJoin);
  public:
    StringJoin()
      : BuiltinFunctionType("string.join", Type::String) {
      this->addParameter("...", Type::Any, Flags::Variadic);
    }
    Value executeCall(IExecution&, const String& instance, const IParameters& parameters) const {
      // string join(...)
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

  class StringSplit : public BuiltinFunctionType {
    EGG_NO_COPY(StringSplit);
  public:
    StringSplit()
      : BuiltinFunctionType("string.split", Type::Any) {
      this->addParameter("separator", Type::String, Flags::Required);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO string split(string separator, int? limit)
      auto separator = parameters.getPositional(0);
      if (!separator.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", separator.getRuntimeType()->toString(), "'");
      }
      auto split = instance.split(separator.getString());
      assert(split.size() > 0);
      return this->raise(execution, "TODO: Return an array of strings"); //TODO
    }
  };

  class StringSlice : public BuiltinFunctionType {
    EGG_NO_COPY(StringSlice);
  public:
    StringSlice()
      : BuiltinFunctionType("string.slice", Type::String) {
      this->addParameter("begin", Type::Int, Flags::Required);
      this->addParameter("end", Type::Int, Flags::None);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // TODO string slice(int? begin, int? end)
      auto p0 = parameters.getPositional(0);
      if (!p0.is(Discriminator::Int)) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto begin = p0.getInt();
      if (parameters.getPositionalCount() == 1) {
        return Value{ instance.slice(begin) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.is(Discriminator::Int)) {
        return this->raise(execution, "Second parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto end = p1.getInt();
      return Value{ instance.slice(begin, end) };
    }
  };

  class StringRepeat : public BuiltinFunctionType {
    EGG_NO_COPY(StringRepeat);
  public:
    StringRepeat()
      : BuiltinFunctionType("string.repeat", Type::String) {
      this->addParameter("count", Type::Int, Flags::Required);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // string repeat(int count)
      auto p0 = parameters.getPositional(0);
      if (!p0.is(Discriminator::Int)) {
        return this->raise(execution, "Parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto count = p0.getInt();
      if (count < 0) {
        return this->raise(execution, "Parameter was expected to be a non-negative integer, not ", count);
      }
      if (count == 0) {
        return Value::EmptyString;
      }
      if (count == 1) {
        return Value{ instance };
      }
      StringBuilder sb;
      for (auto i = 0; i < count; ++i) {
        sb.add(instance);
      }
      return Value{ sb.str() };
    }
  };

  class StringReplace : public BuiltinFunctionType {
    EGG_NO_COPY(StringReplace);
  public:
    StringReplace()
      : BuiltinFunctionType("string.replace", Type::Any) {
      this->addParameter("needle", Type::String, Flags::Required);
      this->addParameter("replacement", Type::String, Flags::Required);
      this->addParameter("occurrences", Type::Int, Flags::None);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // string replace(string needle, string replacement, int? occurrences)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      auto replacement = parameters.getPositional(1);
      if (!replacement.is(Discriminator::String)) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      if (parameters.getPositionalCount() < 3) {
        return Value{ instance.replace(needle.getString(), replacement.getString()) };
      }
      auto occurrences = parameters.getPositional(2);
      if (!occurrences.is(Discriminator::Int)) {
        return this->raise(execution, "Third parameter was expected to be an 'int', not '", needle.getRuntimeType()->toString(), "'");
      }
      return Value{ instance.replace(needle.getString(), replacement.getString(), occurrences.getInt()) };
    }
  };

  class StringPadLeft : public BuiltinFunctionType {
    EGG_NO_COPY(StringPadLeft);
  public:
    StringPadLeft()
      : BuiltinFunctionType("string.padLeft", Type::Any) {
      this->addParameter("length", Type::Int, Flags::Required);
      this->addParameter("padding", Type::String, Flags::None);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // string padLeft(int length, string? padding)
      auto p0 = parameters.getPositional(0);
      if (!p0.is(Discriminator::Int)) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto length = p0.getInt();
      if (length < 0) {
        return this->raise(execution, "First parameter was expected to be a non-negative integer, not ", length);
      }
      if (parameters.getPositionalCount() < 2) {
        return Value{ instance.padLeft(size_t(length)) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.is(Discriminator::String)) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", p1.getRuntimeType()->toString(), "'");
      }
      return Value{ instance.padLeft(size_t(length), p1.getString()) };
    }
  };

  class StringPadRight : public BuiltinFunctionType {
    EGG_NO_COPY(StringPadRight);
  public:
    StringPadRight()
      : BuiltinFunctionType("string.padRight", Type::Any) {
      this->addParameter("length", Type::Int, Flags::Required);
      this->addParameter("padding", Type::String, Flags::None);
    }
    Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const {
      // string padRight(int length, string? padding)
      auto p0 = parameters.getPositional(0);
      if (!p0.is(Discriminator::Int)) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto length = p0.getInt();
      if (length < 0) {
        return this->raise(execution, "First parameter was expected to be a non-negative integer, not ", length);
      }
      if (parameters.getPositionalCount() < 2) {
        return Value{ instance.padRight(size_t(length)) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.is(Discriminator::String)) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", p1.getRuntimeType()->toString(), "'");
      }
      return Value{ instance.padRight(size_t(length), p1.getString()) };
    }
  };

  Value stringLength(const String& instance) {
    // This result is the actual length, not a function computing it
    return Value{ int64_t(instance.length()) };
  }
}

std::function<Value(const String&)> egg::lang::String::builtinFactory(const egg::lang::String& property) {
  // See http://chilliant.blogspot.co.uk/2018/05/egg-strings.html
  static const std::map<std::string, std::function<Value(const String&)>> table = {
    { "compare", StringBuiltin<StringCompare>::make },
    { "contains", StringBuiltin<StringContains>::make },
    { "endsWith", StringBuiltin<StringEndsWith>::make },
    { "hashCode", StringBuiltin<StringHashCode>::make },
    { "indexOf", StringBuiltin<StringIndexOf>::make },
    { "join", StringBuiltin<StringJoin>::make },
    { "lastIndexOf", StringBuiltin<StringLastIndexOf>::make },
    { "length", stringLength },
    { "padLeft", StringBuiltin<StringPadLeft>::make },
    { "padRight", StringBuiltin<StringPadRight>::make },
    { "repeat", StringBuiltin<StringRepeat>::make },
    { "replace", StringBuiltin<StringReplace>::make },
    { "slice", StringBuiltin<StringSlice>::make },
    { "split", StringBuiltin<StringSplit>::make },
    { "startsWith", StringBuiltin<StringStartsWith>::make },
    { "toString", StringBuiltin<StringToString>::make },
  };
  auto name = property.toUTF8();
  auto entry = table.find(name);
  if (entry != table.end()) {
    return entry->second;
  }
  return nullptr;
}

egg::lang::Value egg::lang::String::builtin(egg::lang::IExecution& execution, const egg::lang::String& property) const {
  auto factory = String::builtinFactory(property);
  if (factory != nullptr) {
    return factory(*this);
  }
  return execution.raiseFormat("Unknown property for type 'string': '", property, "'");
}

egg::lang::Value egg::lang::Value::builtinString() {
  static Value builtin = Value::make<BuiltinString>();
  return builtin;
}

egg::lang::Value egg::lang::Value::builtinType() {
  static Value builtin = Value::make<BuiltinType>();
  return builtin;
}

egg::lang::Value egg::lang::Value::builtinAssert() {
  static Value builtin = Value::make<BuiltinAssert>();
  return builtin;
}

egg::lang::Value egg::lang::Value::builtinPrint() {
  static Value builtin = Value::make<BuiltinPrint>();
  return builtin;
}
