#include "yolk/yolk.h"

namespace {
  using namespace egg::lang;
  using Flags = egg::ovum::IFunctionSignatureParameter::Flags;

  class BuiltinFunctionType : public egg::yolk::FunctionType {
    EGG_NO_COPY(BuiltinFunctionType);
  public:
    BuiltinFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType)
      : FunctionType(allocator, name, returnType) {
    }
    egg::ovum::String getName() const {
      return this->callable()->getFunctionName();
    }
    ValueLegacy validateCall(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) const {
      ValueLegacy problem;
      if (!this->callable()->validateCall(execution, parameters, problem)) {
        assert(problem.hasFlowControl());
        return problem;
      }
      return ValueLegacy::Void;
    };
    template<typename... ARGS>
    ValueLegacy raise(egg::ovum::IExecution& execution, ARGS&&... args) const {
      // Use perfect forwarding to the constructor
      return execution.raiseFormat(this->getName(), ": ", std::forward<ARGS>(args)...);
    }
  };

  class BuiltinObjectType : public BuiltinFunctionType {
    EGG_NO_COPY(BuiltinObjectType);
  private:
    egg::yolk::DictionaryUnordered<egg::ovum::String, ValueLegacy> properties;
  public:
    BuiltinObjectType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType)
      : BuiltinFunctionType(allocator, name, returnType) {
    }
    void addProperty(const egg::ovum::String& name, ValueLegacy&& value) {
      this->properties.emplaceUnique(name, std::move(value));
    }
    bool tryGetProperty(const egg::ovum::String& name, ValueLegacy& value) const {
      return this->properties.tryGet(name, value);
    }
    virtual bool dotable(const egg::ovum::String* property, egg::ovum::ITypeRef& type, egg::ovum::String& reason) const {
      if (property == nullptr) {
        type = egg::ovum::Type::AnyQ;
        return true;
      }
      ValueLegacy value;
      if (this->properties.tryGet(*property, value)) {
        type = value.getRuntimeType();
        return true;
      }
      reason = egg::ovum::StringBuilder::concat("Unknown built-in property: '", this->getName(), ".", *property, "'");
      return false;
    }
  };

  class BuiltinFunction : public egg::ovum::SoftReferenceCounted<egg::ovum::IObject> {
    EGG_NO_COPY(BuiltinFunction);
  protected:
    egg::ovum::HardPtr<BuiltinFunctionType> type;
  public:
    BuiltinFunction(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType)
      : SoftReferenceCounted(allocator), type(allocator.make<BuiltinFunctionType>(name, returnType)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual ValueLegacy toString() const override {
      return ValueLegacy(this->type->getName());
    }
    virtual egg::ovum::ITypeRef getRuntimeType() const override {
      return egg::ovum::ITypeRef(this->type.get());
    }
    virtual ValueLegacy getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support properties such as '.", property, "'");
    }
    virtual ValueLegacy setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const ValueLegacy&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support properties such as '.", property, "'");
    }
    virtual ValueLegacy getIndex(egg::ovum::IExecution& execution, const ValueLegacy&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual ValueLegacy setIndex(egg::ovum::IExecution& execution, const ValueLegacy&, const ValueLegacy&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual ValueLegacy iterate(egg::ovum::IExecution& execution) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support iteration");
    }
  };

  class BuiltinObject : public egg::ovum::SoftReferenceCounted<egg::ovum::IObject> {
    EGG_NO_COPY(BuiltinObject);
  protected:
    egg::ovum::HardPtr<BuiltinObjectType> type;
  public:
    BuiltinObject(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType)
      : SoftReferenceCounted(allocator), type(allocator.make<BuiltinObjectType>(name, returnType)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual ValueLegacy toString() const override {
      return ValueLegacy(this->type->getName());
    }
    virtual egg::ovum::ITypeRef getRuntimeType() const override {
      return egg::ovum::ITypeRef(this->type.get());
    }
    virtual ValueLegacy getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      ValueLegacy value;
      if (this->type->tryGetProperty(property, value)) {
        return value;
      }
      return execution.raiseFormat("Unknown built-in property: '", this->type->getName(), ".", property, "'");
    }
    virtual ValueLegacy setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const ValueLegacy&) override {
      return execution.raiseFormat("Cannot set built-in property: '", this->type->getName(), ".", property, "'");
    }
    virtual ValueLegacy getIndex(egg::ovum::IExecution& execution, const ValueLegacy&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual ValueLegacy setIndex(egg::ovum::IExecution& execution, const ValueLegacy&, const ValueLegacy&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual ValueLegacy iterate(egg::ovum::IExecution& execution) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support iteration");
    }
    template<typename T>
    void addProperty(egg::ovum::IAllocator& allocator, const std::string& name) {
      this->type->addProperty(name, ValueLegacy::makeObject<T>(allocator));
    }
  };

  class BuiltinStringFrom : public BuiltinFunction {
    EGG_NO_COPY(BuiltinStringFrom);
  public:
    explicit BuiltinStringFrom(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "string.from", egg::ovum::Type::makeBasal(egg::ovum::BasalBits::String | egg::ovum::BasalBits::Null)) {
      this->type->addParameter("value", egg::ovum::Type::AnyQ, Flags::Required);
    }
    virtual ValueLegacy call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      // Convert the parameter to a string
      // Note: Although the return type is 'string?' (for orthogonality) this function never returns 'null'
      ValueLegacy result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      return ValueLegacy{ parameters.getPositional(0).toString() };
    }
  };

  class BuiltinString : public BuiltinObject {
    EGG_NO_COPY(BuiltinString);
  public:
    explicit BuiltinString(egg::ovum::IAllocator& allocator)
      : BuiltinObject(allocator, "string", egg::ovum::Type::String) {
      // The function call looks like: 'string string(any?... value)'
      this->type->addParameter("value", egg::ovum::Type::AnyQ, Flags::Variadic);
      this->addProperty<BuiltinStringFrom>(allocator, "from");
    }
    virtual ValueLegacy call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      // Concatenate the string representations of all parameters
      ValueLegacy result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      auto n = parameters.getPositionalCount();
      switch (n) {
      case 0:
        return ValueLegacy::EmptyString;
      case 1:
        return ValueLegacy{ parameters.getPositional(0).toString() };
      }
      egg::ovum::StringBuilder sb;
      for (size_t i = 0; i < n; ++i) {
        sb.add(parameters.getPositional(i).toString());
      }
      return ValueLegacy{ sb.str() };
    }
  };

  class BuiltinTypeOf : public BuiltinFunction {
    EGG_NO_COPY(BuiltinTypeOf);
  public:
    explicit BuiltinTypeOf(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "type.of", egg::ovum::Type::String) {
      this->type->addParameter("value", egg::ovum::Type::AnyQ, Flags::Required);
    }
    virtual ValueLegacy call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      // Fetch the runtime type of the parameter
      ValueLegacy result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      return ValueLegacy{ parameters.getPositional(0).getRuntimeType()->toString() };
    }
  };

  class BuiltinType : public BuiltinObject {
    EGG_NO_COPY(BuiltinType);
  public:
    explicit BuiltinType(egg::ovum::IAllocator& allocator)
      : BuiltinObject(allocator, "type", egg::ovum::Type::AnyQ) {
      // The function call looks like: 'type type(any?... value)'
      this->type->addParameter("value", egg::ovum::Type::AnyQ, Flags::Variadic);
      this->addProperty<BuiltinTypeOf>(allocator, "of");
    }
    virtual ValueLegacy call(egg::ovum::IExecution&, const egg::ovum::IParameters&) override {
      // TODO
      return ValueLegacy::Null;
    }
  };

  class BuiltinAssert : public BuiltinFunction {
    EGG_NO_COPY(BuiltinAssert);
  public:
    explicit BuiltinAssert(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "assert", egg::ovum::Type::Void) {
      this->type->addParameter("predicate", egg::ovum::Type::Any, egg::ovum::Bits::set(Flags::Required, Flags::Predicate));
    }
    virtual ValueLegacy call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      ValueLegacy result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      return execution.assertion(parameters.getPositional(0));
    }
  };

  class BuiltinPrint : public BuiltinFunction {
    EGG_NO_COPY(BuiltinPrint);
  public:
    explicit BuiltinPrint(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "print", egg::ovum::Type::Void) {
      this->type->addParameter("...", egg::ovum::Type::Any, Flags::Variadic);
    }
    virtual ValueLegacy call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      ValueLegacy result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      auto n = parameters.getPositionalCount();
      std::string utf8;
      for (size_t i = 0; i < n; ++i) {
        auto parameter = parameters.getPositional(i);
        utf8 += parameter.toUTF8();
      }
      execution.print(utf8);
      return ValueLegacy::Void;
    }
  };

  class StringFunctionType : public BuiltinFunctionType {
    EGG_NO_COPY(StringFunctionType);
  public:
    StringFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::ITypeRef& returnType)
      : BuiltinFunctionType(allocator, name, returnType) {
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution&, const egg::ovum::String& instance, const egg::ovum::IParameters&) const = 0;
  };

  class StringBuiltin : public egg::ovum::SoftReferenceCounted<egg::ovum::IObject> {
    EGG_NO_COPY(StringBuiltin);
  protected:
    egg::ovum::String instance;
    egg::ovum::HardPtr<StringFunctionType> type;
  public:
    StringBuiltin(egg::ovum::IAllocator& allocator, const egg::ovum::String& instance, const egg::ovum::HardPtr<StringFunctionType>& type)
      : SoftReferenceCounted(allocator), instance(instance), type(type) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual ValueLegacy toString() const override {
      return ValueLegacy(this->type->getName());
    }
    virtual egg::ovum::ITypeRef getRuntimeType() const override {
      return this->type;
    }
    virtual ValueLegacy call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      // Let the string builtin type handle the request
      ValueLegacy validation = this->type->validateCall(execution, parameters);
      if (validation.hasFlowControl()) {
        return validation;
      }
      return this->type->executeCall(execution, this->instance, parameters);
    }
    virtual ValueLegacy getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      return execution.raiseFormat(this->type->toString(), " does not support properties such as '.", property, "'");
    }
    virtual ValueLegacy setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const ValueLegacy&) override {
      return execution.raiseFormat(this->type->toString(), " does not support properties such as '.", property, "'");
    }
    virtual ValueLegacy getIndex(egg::ovum::IExecution& execution, const ValueLegacy&) override {
      return execution.raiseFormat(this->type->toString(), " does not support indexing with '[]'");
    }
    virtual ValueLegacy setIndex(egg::ovum::IExecution& execution, const ValueLegacy&, const ValueLegacy&) override {
      return execution.raiseFormat(this->type->toString(), " does not support indexing with '[]'");
    }
    virtual ValueLegacy iterate(egg::ovum::IExecution& execution) override {
      return execution.raiseFormat(this->type->toString(), " does not support iteration");
    }
    template<typename T>
    static ValueLegacy make(egg::ovum::IAllocator& allocator, const egg::ovum::String& instance, const egg::ovum::String& property) {
      return ValueLegacy::makeObject<StringBuiltin>(allocator, instance, allocator.make<T>(property));
    }
  };

  class StringHashCode : public StringFunctionType {
    EGG_NO_COPY(StringHashCode);
  public:
    StringHashCode(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Int) {
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution&, const egg::ovum::String& instance, const egg::ovum::IParameters&) const override {
      // int hashCode()
      return ValueLegacy{ instance.hashCode() };
    }
  };

  class StringToString : public StringFunctionType {
    EGG_NO_COPY(StringToString);
  public:
    StringToString(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::String) {
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution&, const egg::ovum::String& instance, const egg::ovum::IParameters&) const override {
      // string toString()
      return ValueLegacy{ instance };
    }
  };

  class StringContains : public StringFunctionType {
    EGG_NO_COPY(StringContains);
  public:
    StringContains(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Bool) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // bool contains(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return ValueLegacy{ instance.contains(needle.getString()) };
    }
  };

  class StringCompare : public StringFunctionType {
    EGG_NO_COPY(StringCompare);
  public:
    StringCompare(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Int) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO int compare(string other, int? start, int? other_start, int? max_length)
      auto other = parameters.getPositional(0);
      if (!other.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", other.getRuntimeType()->toString(), "'");
      }
      return ValueLegacy{ instance.compareTo(other.getString()) };
    }
  };

  class StringStartsWith : public StringFunctionType {
    EGG_NO_COPY(StringStartsWith);
  public:
    StringStartsWith(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Bool) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // bool startsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return ValueLegacy{ instance.startsWith(needle.getString()) };
    }
  };

  class StringEndsWith : public StringFunctionType {
    EGG_NO_COPY(StringEndsWith);
  public:
    StringEndsWith(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Bool) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // bool endsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      return ValueLegacy{ instance.endsWith(needle.getString()) };
    }
  };

  class StringIndexOf : public StringFunctionType {
    EGG_NO_COPY(StringIndexOf);
  public:
    StringIndexOf(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::makeBasal(egg::ovum::BasalBits::Int | egg::ovum::BasalBits::Null)) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO int? indexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      auto index = instance.indexOfString(needle.getString());
      return (index < 0) ? ValueLegacy::Null : ValueLegacy{ index };
    }
  };

  class StringLastIndexOf : public StringFunctionType {
    EGG_NO_COPY(StringLastIndexOf);
  public:
    StringLastIndexOf(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::makeBasal(egg::ovum::BasalBits::Int | egg::ovum::BasalBits::Null)) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO int? lastIndexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      auto index = instance.lastIndexOfString(needle.getString());
      return (index < 0) ? ValueLegacy::Null : ValueLegacy{ index };
    }
  };

  class StringJoin : public StringFunctionType {
    EGG_NO_COPY(StringJoin);
  public:
    StringJoin(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::String) {
      this->addParameter("...", egg::ovum::Type::Any, Flags::Variadic);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution&, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string join(...)
      auto n = parameters.getPositionalCount();
      switch (n) {
      case 0:
        // Joining nothing always produces an empty string
        return ValueLegacy::EmptyString;
      case 1:
        // Joining a single value does not require a separator
        return ValueLegacy{ parameters.getPositional(0).toString() };
      }
      // Our parameters aren't in a std::vector, so we replicate String::join() here
      auto separator = instance.toUTF8();
      egg::ovum::StringBuilder sb;
      sb.add(parameters.getPositional(0).toUTF8());
      for (size_t i = 1; i < n; ++i) {
        sb.add(separator).add(parameters.getPositional(i).toUTF8());
      }
      return ValueLegacy{ sb.str() };
    }
  };

  class StringSplit : public StringFunctionType {
    EGG_NO_COPY(StringSplit);
  public:
    StringSplit(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Any) {
      this->addParameter("separator", egg::ovum::Type::String, Flags::Required);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO string split(string separator, int? limit)
      auto separator = parameters.getPositional(0);
      if (!separator.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", separator.getRuntimeType()->toString(), "'");
      }
      auto split = instance.split(separator.getString());
      assert(split.size() > 0);
      return this->raise(execution, "TODO: Return an array of strings"); //TODO
    }
  };

  class StringSlice : public StringFunctionType {
    EGG_NO_COPY(StringSlice);
  public:
    StringSlice(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::String) {
      this->addParameter("begin", egg::ovum::Type::Int, Flags::Required);
      this->addParameter("end", egg::ovum::Type::Int, Flags::None);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO string slice(int? begin, int? end)
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto begin = p0.getInt();
      if (parameters.getPositionalCount() == 1) {
        return ValueLegacy{ instance.slice(begin) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.isInt()) {
        return this->raise(execution, "Second parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto end = p1.getInt();
      return ValueLegacy{ instance.slice(begin, end) };
    }
  };

  class StringRepeat : public StringFunctionType {
    EGG_NO_COPY(StringRepeat);
  public:
    StringRepeat(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::String) {
      this->addParameter("count", egg::ovum::Type::Int, Flags::Required);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string repeat(int count)
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "Parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto count = p0.getInt();
      if (count < 0) {
        return this->raise(execution, "Parameter was expected to be a non-negative integer, not ", count);
      }
      return ValueLegacy{ instance.repeat(size_t(count)) };
    }
  };

  class StringReplace : public StringFunctionType {
    EGG_NO_COPY(StringReplace);
  public:
    StringReplace(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Any) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
      this->addParameter("replacement", egg::ovum::Type::String, Flags::Required);
      this->addParameter("occurrences", egg::ovum::Type::Int, Flags::None);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string replace(string needle, string replacement, int? occurrences)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      auto replacement = parameters.getPositional(1);
      if (!replacement.isString()) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", needle.getRuntimeType()->toString(), "'");
      }
      if (parameters.getPositionalCount() < 3) {
        return ValueLegacy{ instance.replace(needle.getString(), replacement.getString()) };
      }
      auto occurrences = parameters.getPositional(2);
      if (!occurrences.isInt()) {
        return this->raise(execution, "Third parameter was expected to be an 'int', not '", needle.getRuntimeType()->toString(), "'");
      }
      return ValueLegacy{ instance.replace(needle.getString(), replacement.getString(), occurrences.getInt()) };
    }
  };

  class StringPadLeft : public StringFunctionType {
    EGG_NO_COPY(StringPadLeft);
  public:
    StringPadLeft(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Any) {
      this->addParameter("length", egg::ovum::Type::Int, Flags::Required);
      this->addParameter("padding", egg::ovum::Type::String, Flags::None);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string padLeft(int length, string? padding)
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto length = p0.getInt();
      if (length < 0) {
        return this->raise(execution, "First parameter was expected to be a non-negative integer, not ", length);
      }
      if (parameters.getPositionalCount() < 2) {
        return ValueLegacy{ instance.padLeft(size_t(length)) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.isString()) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", p1.getRuntimeType()->toString(), "'");
      }
      return ValueLegacy{ instance.padLeft(size_t(length), p1.getString()) };
    }
  };

  class StringPadRight : public StringFunctionType {
    EGG_NO_COPY(StringPadRight);
  public:
    StringPadRight(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Any) {
      this->addParameter("length", egg::ovum::Type::Int, Flags::Required);
      this->addParameter("padding", egg::ovum::Type::String, Flags::None);
    }
    virtual ValueLegacy executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string padRight(int length, string? padding)
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType()->toString(), "'");
      }
      auto length = p0.getInt();
      if (length < 0) {
        return this->raise(execution, "First parameter was expected to be a non-negative integer, not ", length);
      }
      if (parameters.getPositionalCount() < 2) {
        return ValueLegacy{ instance.padRight(size_t(length)) };
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.isString()) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", p1.getRuntimeType()->toString(), "'");
      }
      return ValueLegacy{ instance.padRight(size_t(length), p1.getString()) };
    }
  };

  ValueLegacy stringLength(egg::ovum::IAllocator&, const egg::ovum::String& instance, const egg::ovum::String&) {
    // This result is the actual length, not a function computing it
    return ValueLegacy{ int64_t(instance.length()) };
  }
}

egg::yolk::Builtins::StringBuiltinFactory egg::yolk::Builtins::stringBuiltinFactory(const egg::ovum::String& property) {
  // See http://chilliant.blogspot.co.uk/2018/05/egg-strings.html
  static const std::map<egg::ovum::String, StringBuiltinFactory> table = {
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

egg::lang::ValueLegacy egg::yolk::Builtins::stringBuiltin(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::String& property){
  auto factory = Builtins::stringBuiltinFactory(property);
  if (factory != nullptr) {
    return factory(execution.getAllocator(), instance, egg::ovum::StringBuilder::concat("string.", property));
  }
  return execution.raiseFormat("Unknown property for type 'string': '", property, "'");
}

egg::lang::ValueLegacy egg::lang::ValueLegacy::builtinString(egg::ovum::IAllocator& allocator) {
  return ValueLegacy::makeObject<BuiltinString>(allocator);
}

egg::lang::ValueLegacy egg::lang::ValueLegacy::builtinType(egg::ovum::IAllocator& allocator) {
  return ValueLegacy::makeObject<BuiltinType>(allocator);
}

egg::lang::ValueLegacy egg::lang::ValueLegacy::builtinAssert(egg::ovum::IAllocator& allocator) {
  return ValueLegacy::makeObject<BuiltinAssert>(allocator);
}

egg::lang::ValueLegacy egg::lang::ValueLegacy::builtinPrint(egg::ovum::IAllocator& allocator) {
  return ValueLegacy::makeObject<BuiltinPrint>(allocator);
}
