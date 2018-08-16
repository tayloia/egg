#include "yolk/yolk.h"

namespace {
  using namespace egg::lang;
  using Flags = IFunctionSignatureParameter::Flags;

  class BuiltinFunctionType : public egg::yolk::FunctionType {
    EGG_NO_COPY(BuiltinFunctionType);
  public:
    BuiltinFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const ITypeRef& returnType)
      : FunctionType(allocator, name, returnType) {
    }
    String getName() const {
      return this->callable()->getFunctionName();
    }
    Value validateCall(IExecution& execution, const IParameters& parameters) const {
      Value problem;
      if (!this->callable()->validateCall(execution, parameters, problem)) {
        assert(problem.has(Discriminator::FlowControl));
        return problem;
      }
      return Value::Void;
    };
    template<typename... ARGS>
    Value raise(IExecution& execution, ARGS&&... args) const {
      // Use perfect forwarding to the constructor
      return execution.raiseFormat(this->getName(), ": ", std::forward<ARGS>(args)...);
    }
  };

  class BuiltinObjectType : public BuiltinFunctionType {
    EGG_NO_COPY(BuiltinObjectType);
  private:
    egg::yolk::DictionaryUnordered<String, Value> properties;
  public:
    BuiltinObjectType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const ITypeRef& returnType)
      : BuiltinFunctionType(allocator, name, returnType) {
    }
    void addProperty(const String& name, Value&& value) {
      this->properties.emplaceUnique(name, std::move(value));
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
      reason = egg::ovum::StringBuilder::concat("Unknown built-in property: '", this->getName(), ".", *property, "'");
      return false;
    }
  };

  class BuiltinFunction : public egg::ovum::SoftReferenceCounted<IObject> {
    EGG_NO_COPY(BuiltinFunction);
  protected:
    egg::ovum::HardPtr<BuiltinFunctionType> type;
  public:
    BuiltinFunction(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const ITypeRef& returnType)
      : SoftReferenceCounted(allocator), type(allocator.make<BuiltinFunctionType>(name, returnType)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual Value toString() const override {
      return Value(this->type->getName());
    }
    virtual ITypeRef getRuntimeType() const override {
      return ITypeRef(this->type.get());
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

  class BuiltinObject : public egg::ovum::SoftReferenceCounted<IObject> {
    EGG_NO_COPY(BuiltinObject);
  protected:
    egg::ovum::HardPtr<BuiltinObjectType> type;
  public:
    BuiltinObject(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const ITypeRef& returnType)
      : SoftReferenceCounted(allocator), type(allocator.make<BuiltinObjectType>(name, returnType)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual Value toString() const override {
      return Value(this->type->getName());
    }
    virtual ITypeRef getRuntimeType() const override {
      return ITypeRef(this->type.get());
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
    template<typename T>
    void addProperty(egg::ovum::IAllocator& allocator, const std::string& name) {
      this->type->addProperty(name, Value::makeObject<T>(allocator));
    }
  };

  class BuiltinStringFrom : public BuiltinFunction {
    EGG_NO_COPY(BuiltinStringFrom);
  public:
    explicit BuiltinStringFrom(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "string.from", Type::makeSimple(Discriminator::String | Discriminator::Null)) {
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
    explicit BuiltinString(egg::ovum::IAllocator& allocator)
      : BuiltinObject(allocator, "string", Type::String) {
      // The function call looks like: 'string string(any?... value)'
      this->type->addParameter("value", Type::AnyQ, Flags::Variadic);
      this->addProperty<BuiltinStringFrom>(allocator, "from");
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
      egg::ovum::StringBuilder sb;
      for (size_t i = 0; i < n; ++i) {
        sb.add(parameters.getPositional(i).toString());
      }
      return Value{ sb.str() };
    }
  };

  class BuiltinTypeOf : public BuiltinFunction {
    EGG_NO_COPY(BuiltinTypeOf);
  public:
    explicit BuiltinTypeOf(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "type.of", Type::Type_) {
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
    explicit BuiltinType(egg::ovum::IAllocator& allocator)
      : BuiltinObject(allocator, "type", Type::Type_) {
      // The function call looks like: 'type type(any?... value)'
      this->type->addParameter("value", Type::AnyQ, Flags::Variadic);
      this->addProperty<BuiltinTypeOf>(allocator, "of");
    }
    virtual Value call(IExecution&, const IParameters&) override {
      // TODO
      return Value::Null;
    }
  };

  class BuiltinAssert : public BuiltinFunction {
    EGG_NO_COPY(BuiltinAssert);
  public:
    explicit BuiltinAssert(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "assert", Type::Void) {
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
    explicit BuiltinPrint(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "print", Type::Void) {
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

  class StringFunctionType : public BuiltinFunctionType {
    EGG_NO_COPY(StringFunctionType);
  public:
    StringFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const ITypeRef& returnType)
      : BuiltinFunctionType(allocator, name, returnType) {
    }
    virtual Value executeCall(IExecution&, const String& instance, const IParameters&) const = 0;
  };

  class StringBuiltin : public egg::ovum::SoftReferenceCounted<IObject> {
    EGG_NO_COPY(StringBuiltin);
  protected:
    String instance;
    egg::ovum::HardPtr<StringFunctionType> type;
  public:
    StringBuiltin(egg::ovum::IAllocator& allocator, const String& instance, const egg::ovum::HardPtr<StringFunctionType>& type)
      : SoftReferenceCounted(allocator), instance(instance), type(type) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual Value toString() const override {
      return Value(this->type->getName());
    }
    virtual ITypeRef getRuntimeType() const override {
      return this->type;
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
    template<typename T>
    static Value make(egg::ovum::IAllocator& allocator, const egg::ovum::String& instance, const egg::ovum::String& property) {
      return Value::makeObject<StringBuiltin>(allocator, instance, allocator.make<T>(property));
    }
  };

  class StringHashCode : public StringFunctionType {
    EGG_NO_COPY(StringHashCode);
  public:
    StringHashCode(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Int) {
    }
    virtual Value executeCall(IExecution&, const String& instance, const IParameters&) const override {
      // int hashCode()
      return Value{ StringLegacy(instance).hashCode() };
    }
  };

  class StringToString : public StringFunctionType {
    EGG_NO_COPY(StringToString);
  public:
    StringToString(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::String) {
    }
    virtual Value executeCall(IExecution&, const String& instance, const IParameters&) const override {
      // string toString()
      return Value{ instance };
    }
  };

  class StringContains : public StringFunctionType {
    EGG_NO_COPY(StringContains);
  public:
    StringContains(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Bool) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
      // bool contains(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return Value{ StringLegacy(instance).contains(needle.getString()) };
    }
  };

  class StringCompare : public StringFunctionType {
    EGG_NO_COPY(StringCompare);
  public:
    StringCompare(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Int) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
      // TODO int compare(string other, int? start, int? other_start, int? max_length)
      auto other = parameters.getPositional(0);
      if (!other.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", other.getRuntimeType()->toString(), "'");
      }
      return Value{ StringLegacy(instance).compare(other.getString()) };
    }
  };

  class StringStartsWith : public StringFunctionType {
    EGG_NO_COPY(StringStartsWith);
  public:
    StringStartsWith(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Bool) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
      // bool startsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return Value{ StringLegacy(instance).startsWith(needle.getString()) };
    }
  };

  class StringEndsWith : public StringFunctionType {
    EGG_NO_COPY(StringEndsWith);
  public:
    StringEndsWith(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Bool) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
      // bool endsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return Value{ StringLegacy(instance).endsWith(needle.getString()) };
    }
  };

  class StringIndexOf : public StringFunctionType {
    EGG_NO_COPY(StringIndexOf);
  public:
    StringIndexOf(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::makeSimple(Discriminator::Int | Discriminator::Null)) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
      // TODO int? indexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      auto index = StringLegacy(instance).indexOfString(needle.getString());
      return (index < 0) ? Value::Null : Value{ index };
    }
  };

  class StringLastIndexOf : public StringFunctionType {
    EGG_NO_COPY(StringLastIndexOf);
  public:
    StringLastIndexOf(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::makeSimple(Discriminator::Int | Discriminator::Null)) {
      this->addParameter("needle", Type::String, Flags::Required);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
      // TODO int? lastIndexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      auto index = StringLegacy(instance).lastIndexOfString(needle.getString());
      return (index < 0) ? Value::Null : Value{ index };
    }
  };

  class StringJoin : public StringFunctionType {
    EGG_NO_COPY(StringJoin);
  public:
    StringJoin(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::String) {
      this->addParameter("...", Type::Any, Flags::Variadic);
    }
    virtual Value executeCall(IExecution&, const String& instance, const IParameters& parameters) const override {
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
      egg::ovum::StringBuilder sb;
      sb.add(parameters.getPositional(0).toUTF8());
      for (size_t i = 1; i < n; ++i) {
        sb.add(separator).add(parameters.getPositional(i).toUTF8());
      }
      return Value{ sb.str() };
    }
  };

  class StringSplit : public StringFunctionType {
    EGG_NO_COPY(StringSplit);
  public:
    StringSplit(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Any) {
      this->addParameter("separator", Type::String, Flags::Required);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
      // TODO string split(string separator, int? limit)
      auto separator = parameters.getPositional(0);
      if (!separator.is(Discriminator::String)) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", separator.getRuntimeType()->toString(), "'");
      }
      auto split = StringLegacy(instance).split(separator.getString());
      assert(split.size() > 0);
      return this->raise(execution, "TODO: Return an array of strings"); //TODO
    }
  };

  class StringSlice : public StringFunctionType {
    EGG_NO_COPY(StringSlice);
  public:
    StringSlice(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::String) {
      this->addParameter("begin", Type::Int, Flags::Required);
      this->addParameter("end", Type::Int, Flags::None);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
      // TODO string slice(int? begin, int? end)
      auto p0 = parameters.getPositional(0);
      if (!p0.is(Discriminator::Int)) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto begin = p0.getInt();
      if (parameters.getPositionalCount() == 1) {
        return Value{ StringLegacy(instance).slice(begin) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.is(Discriminator::Int)) {
        return this->raise(execution, "Second parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto end = p1.getInt();
      return Value{ StringLegacy(instance).slice(begin, end) };
    }
  };

  class StringRepeat : public StringFunctionType {
    EGG_NO_COPY(StringRepeat);
  public:
    StringRepeat(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::String) {
      this->addParameter("count", Type::Int, Flags::Required);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
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
      egg::ovum::StringBuilder sb;
      for (auto i = 0; i < count; ++i) {
        sb.add(instance);
      }
      return Value{ sb.str() };
    }
  };

  class StringReplace : public StringFunctionType {
    EGG_NO_COPY(StringReplace);
  public:
    StringReplace(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Any) {
      this->addParameter("needle", Type::String, Flags::Required);
      this->addParameter("replacement", Type::String, Flags::Required);
      this->addParameter("occurrences", Type::Int, Flags::None);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
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
        return Value{ StringLegacy(instance).replace(needle.getString(), replacement.getString()) };
      }
      auto occurrences = parameters.getPositional(2);
      if (!occurrences.is(Discriminator::Int)) {
        return this->raise(execution, "Third parameter was expected to be an 'int', not '", needle.getRuntimeType()->toString(), "'");
      }
      return Value{ StringLegacy(instance).replace(needle.getString(), replacement.getString(), occurrences.getInt()) };
    }
  };

  class StringPadLeft : public StringFunctionType {
    EGG_NO_COPY(StringPadLeft);
  public:
    StringPadLeft(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Any) {
      this->addParameter("length", Type::Int, Flags::Required);
      this->addParameter("padding", Type::String, Flags::None);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
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
        return Value{ StringLegacy(instance).padLeft(size_t(length)) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.is(Discriminator::String)) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", p1.getRuntimeType()->toString(), "'");
      }
      return Value{ StringLegacy(instance).padLeft(size_t(length), p1.getString()) };
    }
  };

  class StringPadRight : public StringFunctionType {
    EGG_NO_COPY(StringPadRight);
  public:
    StringPadRight(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, Type::Any) {
      this->addParameter("length", Type::Int, Flags::Required);
      this->addParameter("padding", Type::String, Flags::None);
    }
    virtual Value executeCall(IExecution& execution, const String& instance, const IParameters& parameters) const override {
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
        return Value{ StringLegacy(instance).padRight(size_t(length)) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.is(Discriminator::String)) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", p1.getRuntimeType()->toString(), "'");
      }
      return Value{ StringLegacy(instance).padRight(size_t(length), p1.getString()) };
    }
  };

  Value stringLength(egg::ovum::IAllocator&, const egg::ovum::String& instance, const egg::ovum::String&) {
    // This result is the actual length, not a function computing it
    return Value{ int64_t(instance.length()) };
  }
}

egg::yolk::Builtins::StringBuiltinFactory egg::yolk::Builtins::stringBuiltinFactory(const egg::ovum::String& property) {
  // See http://chilliant.blogspot.co.uk/2018/05/egg-strings.html
  static const egg::ovum::StringMap<StringBuiltinFactory> table = {
    { "compare",      StringBuiltin::make<StringCompare> },
    { "contains",     StringBuiltin::make<StringContains> },
    { "endsWith",     StringBuiltin::make<StringEndsWith> },
    { "hashCode",     StringBuiltin::make<StringHashCode> },
    { "indexOf",      StringBuiltin::make<StringIndexOf> },
    { "join",         StringBuiltin::make<StringJoin> },
    { "lastIndexOf",  StringBuiltin::make<StringLastIndexOf> },
    { "length",       stringLength },
    { "padLeft",      StringBuiltin::make<StringPadLeft> },
    { "padRight",     StringBuiltin::make<StringPadRight> },
    { "repeat",       StringBuiltin::make<StringRepeat> },
    { "replace",      StringBuiltin::make<StringReplace> },
    { "slice",        StringBuiltin::make<StringSlice> },
    { "split",        StringBuiltin::make<StringSplit> },
    { "startsWith",   StringBuiltin::make<StringStartsWith> },
    { "toString",     StringBuiltin::make<StringToString> }
  };
  auto entry = table.find(property);
  if (entry != table.end()) {
    return entry->second;
  }
  return nullptr;
}

egg::lang::Value egg::yolk::Builtins::stringBuiltin(egg::lang::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::String& property){
  auto factory = Builtins::stringBuiltinFactory(property);
  if (factory != nullptr) {
    return factory(execution.getAllocator(), instance, egg::ovum::StringBuilder::concat("string.", property));
  }
  return execution.raiseFormat("Unknown property for type 'string': '", property, "'");
}

egg::lang::Value egg::lang::Value::builtinString(egg::ovum::IAllocator& allocator) {
  return Value::makeObject<BuiltinString>(allocator);
}

egg::lang::Value egg::lang::Value::builtinType(egg::ovum::IAllocator& allocator) {
  return Value::makeObject<BuiltinType>(allocator);
}

egg::lang::Value egg::lang::Value::builtinAssert(egg::ovum::IAllocator& allocator) {
  return Value::makeObject<BuiltinAssert>(allocator);
}

egg::lang::Value egg::lang::Value::builtinPrint(egg::ovum::IAllocator& allocator) {
  return Value::makeObject<BuiltinPrint>(allocator);
}
