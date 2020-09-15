#include "yolk/yolk.h"

#include <map>

namespace {
  using Flags = egg::ovum::IFunctionSignatureParameter::Flags;

  class BuiltinFunctionType : public egg::ovum::FunctionType {
    EGG_NO_COPY(BuiltinFunctionType);
  public:
    BuiltinFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType)
      : FunctionType(allocator, name, returnType) {
    }
    egg::ovum::String getName() const {
      return this->callable()->getFunctionName();
    }
    egg::ovum::Variant validateCall(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) const {
      // TODO type checking, etc
      assert(this->callable() != nullptr);
      auto& sig = *this->callable();
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat(egg::ovum::Function::signatureToString(sig), ": Named parameters are not yet supported"); // TODO
      }
      auto maxPositional = sig.getParameterCount();
      auto minPositional = maxPositional;
      while ((minPositional > 0) && !egg::ovum::Bits::hasAnySet(sig.getParameter(minPositional - 1).getFlags(), Flags::Required)) {
        minPositional--;
      }
      auto actual = parameters.getPositionalCount();
      if (actual < minPositional) {
        if (minPositional == 1) {
          return execution.raiseFormat(egg::ovum::Function::signatureToString(sig), ": At least 1 parameter was expected");
        }
        return execution.raiseFormat(egg::ovum::Function::signatureToString(sig), ": At least ", minPositional, " parameters were expected, not ", actual);
      }
      if ((maxPositional > 0) && egg::ovum::Bits::hasAnySet(sig.getParameter(maxPositional - 1).getFlags(), Flags::Variadic)) {
        // TODO Variadic
      } else if (actual > maxPositional) {
        // Not variadic
        if (maxPositional == 1) {
          return execution.raiseFormat(egg::ovum::Function::signatureToString(sig), ": Only 1 parameter was expected, not ", actual);
        }
        return execution.raiseFormat(egg::ovum::Function::signatureToString(sig), ": No more than ", maxPositional, " parameters were expected, not ", actual);
      }
      return egg::ovum::Variant::Void;
    };
    template<typename... ARGS>
    egg::ovum::Variant raise(egg::ovum::IExecution& execution, ARGS&&... args) const {
      // Use perfect forwarding to the constructor
      return execution.raiseFormat(this->getName(), ": ", std::forward<ARGS>(args)...);
    }
  };

  class BuiltinObjectType : public BuiltinFunctionType {
    EGG_NO_COPY(BuiltinObjectType);
  private:
    egg::ovum::DictionaryUnordered<egg::ovum::String, egg::ovum::Variant> properties;
  public:
    BuiltinObjectType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType)
      : BuiltinFunctionType(allocator, name, returnType) {
    }
    void addProperty(const egg::ovum::String& name, egg::ovum::Variant&& value) {
      this->properties.emplaceUnique(name, std::move(value));
    }
    bool tryGetProperty(const egg::ovum::String& name, egg::ovum::Variant& value) const {
      return this->properties.tryGet(name, value);
    }
    virtual egg::ovum::Type dotable(const egg::ovum::String& property, egg::ovum::String& error) const override {
      if (property.empty()) {
        return egg::ovum::Type::AnyQ;
      }
      egg::ovum::Variant value;
      if (this->properties.tryGet(property, value)) {
        return value.getRuntimeType();
      }
      error = egg::ovum::StringBuilder::concat("Unknown built-in property: '", this->getName(), ".", property, "'");
      return nullptr;
    }
  };

  class BuiltinFunction : public egg::ovum::SoftReferenceCounted<egg::ovum::IObject> {
    EGG_NO_COPY(BuiltinFunction);
  protected:
    egg::ovum::HardPtr<BuiltinFunctionType> type;
  public:
    BuiltinFunction(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType)
      : SoftReferenceCounted(allocator), type(allocator.make<BuiltinFunctionType>(name, returnType)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual egg::ovum::Variant toString() const override {
      return egg::ovum::Variant(this->type->getName());
    }
    virtual egg::ovum::Type getRuntimeType() const override {
      return egg::ovum::Type(this->type.get());
    }
    virtual egg::ovum::Variant getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support properties such as '.", property, "'");
    }
    virtual egg::ovum::Variant setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const egg::ovum::Variant&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support properties such as '.", property, "'");
    }
    virtual egg::ovum::Variant getIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual egg::ovum::Variant setIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant&, const egg::ovum::Variant&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution& execution) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support iteration");
    }
  };

  class BuiltinObject : public egg::ovum::SoftReferenceCounted<egg::ovum::IObject> {
    EGG_NO_COPY(BuiltinObject);
  protected:
    egg::ovum::HardPtr<BuiltinObjectType> type;
  public:
    BuiltinObject(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType)
      : SoftReferenceCounted(allocator), type(allocator.make<BuiltinObjectType>(name, returnType)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual egg::ovum::Variant toString() const override {
      return egg::ovum::Variant(this->type->getName());
    }
    virtual egg::ovum::Type getRuntimeType() const override {
      return egg::ovum::Type(this->type.get());
    }
    virtual egg::ovum::Variant getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      egg::ovum::Variant value;
      if (this->type->tryGetProperty(property, value)) {
        return value;
      }
      return execution.raiseFormat("Unknown built-in property: '", this->type->getName(), ".", property, "'");
    }
    virtual egg::ovum::Variant setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const egg::ovum::Variant&) override {
      return execution.raiseFormat("Cannot set built-in property: '", this->type->getName(), ".", property, "'");
    }
    virtual egg::ovum::Variant getIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual egg::ovum::Variant setIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant&, const egg::ovum::Variant&) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support indexing with '[]'");
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution& execution) override {
      return execution.raiseFormat("Built-in '", this->type->getName(), "' does not support iteration");
    }
    template<typename T>
    void addProperty(egg::ovum::IAllocator& allocator, const std::string& name) {
      this->type->addProperty(name, egg::ovum::VariantFactory::createObject<T>(allocator));
    }
  };

  class BuiltinStringFrom : public BuiltinFunction {
    EGG_NO_COPY(BuiltinStringFrom);
  public:
    explicit BuiltinStringFrom(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "string.from", egg::ovum::Type::makeBasal(allocator, egg::ovum::BasalBits::String | egg::ovum::BasalBits::Null)) {
      this->type->addParameter("value", egg::ovum::Type::AnyQ, Flags::Required);
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      // Convert the parameter to a string
      // Note: Although the return type is 'string?' (for orthogonality) this function never returns 'null'
      egg::ovum::Variant result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      return parameters.getPositional(0).toString();
    }
  };

  class Builtin_String : public BuiltinObject {
    EGG_NO_COPY(Builtin_String);
  public:
    explicit Builtin_String(egg::ovum::IAllocator& allocator)
      : BuiltinObject(allocator, "string", egg::ovum::Type::String) {
      // The function call looks like: 'string string(any?... value)'
      this->type->addParameter("value", egg::ovum::Type::AnyQ, Flags::Variadic);
      this->addProperty<BuiltinStringFrom>(allocator, "from");
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      // Concatenate the string representations of all parameters
      egg::ovum::Variant result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      auto n = parameters.getPositionalCount();
      switch (n) {
      case 0:
        return egg::ovum::String();
      case 1:
        return parameters.getPositional(0).toString();
      }
      egg::ovum::StringBuilder sb;
      for (size_t i = 0; i < n; ++i) {
        sb.add(parameters.getPositional(i).toString());
      }
      return sb.str();
    }
  };

  class BuiltinTypeOf : public BuiltinFunction {
    EGG_NO_COPY(BuiltinTypeOf);
  public:
    explicit BuiltinTypeOf(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "type.of", egg::ovum::Type::String) {
      this->type->addParameter("value", egg::ovum::Type::AnyQ, Flags::Required);
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      // Fetch the runtime type of the parameter
      egg::ovum::Variant result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      return parameters.getPositional(0).getRuntimeType().toString();
    }
  };

  class Builtin_Type : public BuiltinObject {
    EGG_NO_COPY(Builtin_Type);
  public:
    explicit Builtin_Type(egg::ovum::IAllocator& allocator)
      : BuiltinObject(allocator, "type", egg::ovum::Type::AnyQ) {
      // The function call looks like: 'type type(any?... value)'
      this->type->addParameter("value", egg::ovum::Type::AnyQ, Flags::Variadic);
      this->addProperty<BuiltinTypeOf>(allocator, "of");
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution&, const egg::ovum::IParameters&) override {
      // TODO
      return egg::ovum::Variant::Null;
    }
  };

  class Builtin_Assert : public BuiltinFunction {
    EGG_NO_COPY(Builtin_Assert);
  public:
    explicit Builtin_Assert(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "assert", egg::ovum::Type::Void) {
      this->type->addParameter("predicate", egg::ovum::Type::Any, egg::ovum::Bits::set(Flags::Required, Flags::Predicate));
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      egg::ovum::Variant result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      return execution.assertion(parameters.getPositional(0));
    }
  };

  class Builtin_Print : public BuiltinFunction {
    EGG_NO_COPY(Builtin_Print);
  public:
    explicit Builtin_Print(egg::ovum::IAllocator& allocator)
      : BuiltinFunction(allocator, "print", egg::ovum::Type::Void) {
      this->type->addParameter("...", egg::ovum::Type::Any, Flags::Variadic);
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      egg::ovum::Variant result = this->type->validateCall(execution, parameters);
      if (result.hasFlowControl()) {
        return result;
      }
      egg::ovum::StringBuilder sb;
      auto n = parameters.getPositionalCount();
      for (size_t i = 0; i < n; ++i) {
        auto parameter = parameters.getPositional(i);
        sb.add(parameter.toString());
      }
      execution.print(sb.toUTF8());
      return egg::ovum::Variant::Void;
    }
  };

  class StringFunctionType : public BuiltinFunctionType {
    EGG_NO_COPY(StringFunctionType);
  public:
    StringFunctionType(egg::ovum::IAllocator& allocator, const egg::ovum::String& name, const egg::ovum::Type& returnType)
      : BuiltinFunctionType(allocator, name, returnType) {
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution&, const egg::ovum::String& instance, const egg::ovum::IParameters&) const = 0;
  };

  class StringBuiltin : public egg::ovum::SoftReferenceCounted<egg::ovum::IObject> {
    EGG_NO_COPY(StringBuiltin);
  protected:
    egg::ovum::String instance;
    egg::ovum::HardPtr<StringFunctionType> type;
  public:
    StringBuiltin(egg::ovum::IAllocator& allocator, const egg::ovum::String& instance, egg::ovum::HardPtr<StringFunctionType>&& type)
      : SoftReferenceCounted(allocator), instance(instance), type(std::move(type)) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual egg::ovum::Variant toString() const override {
      return egg::ovum::Variant(this->type->getName());
    }
    virtual egg::ovum::Type getRuntimeType() const override {
      return egg::ovum::Type(this->type.get());
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution& execution, const egg::ovum::IParameters& parameters) override {
      // Let the string builtin type handle the request
      egg::ovum::Variant validation = this->type->validateCall(execution, parameters);
      if (validation.hasFlowControl()) {
        return validation;
      }
      return this->type->executeCall(execution, this->instance, parameters);
    }
    virtual egg::ovum::Variant getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      return execution.raiseFormat(this->getRuntimeType().toString(), " does not support properties such as '.", property, "'");
    }
    virtual egg::ovum::Variant setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const egg::ovum::Variant&) override {
      return execution.raiseFormat(this->getRuntimeType().toString(), " does not support properties such as '.", property, "'");
    }
    virtual egg::ovum::Variant getIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant&) override {
      return execution.raiseFormat(this->getRuntimeType().toString(), " does not support indexing with '[]'");
    }
    virtual egg::ovum::Variant setIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant&, const egg::ovum::Variant&) override {
      return execution.raiseFormat(this->getRuntimeType().toString(), " does not support indexing with '[]'");
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution& execution) override {
      return execution.raiseFormat(this->getRuntimeType().toString(), " does not support iteration");
    }
    template<typename T>
    static egg::ovum::Variant make(egg::ovum::IAllocator& allocator, const egg::ovum::String& instance, const egg::ovum::String& property) {
      return egg::ovum::VariantFactory::createObject<StringBuiltin>(allocator, instance, allocator.make<T>(property));
    }
  };

  class StringHash : public StringFunctionType {
    EGG_NO_COPY(StringHash);
  public:
    StringHash(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Int) {
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution&, const egg::ovum::String& instance, const egg::ovum::IParameters&) const override {
      // int hash()
      return instance.hash();
    }
  };

  class StringToString : public StringFunctionType {
    EGG_NO_COPY(StringToString);
  public:
    StringToString(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::String) {
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution&, const egg::ovum::String& instance, const egg::ovum::IParameters&) const override {
      // string toString()
      return instance;
    }
  };

  class StringContains : public StringFunctionType {
    EGG_NO_COPY(StringContains);
  public:
    StringContains(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Bool) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // bool contains(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      return instance.contains(needle.getString());
    }
  };

  class StringCompareTo : public StringFunctionType {
    EGG_NO_COPY(StringCompareTo);
  public:
    StringCompareTo(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Int) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO int compare(string other, int? start, int? other_start, int? max_length)
      auto other = parameters.getPositional(0);
      if (!other.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", other.getRuntimeType().toString(), "'");
      }
      return instance.compareTo(other.getString());
    }
  };

  class StringStartsWith : public StringFunctionType {
    EGG_NO_COPY(StringStartsWith);
  public:
    StringStartsWith(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Bool) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // bool startsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      return instance.startsWith(needle.getString());
    }
  };

  class StringEndsWith : public StringFunctionType {
    EGG_NO_COPY(StringEndsWith);
  public:
    StringEndsWith(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Bool) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // bool endsWith(string needle)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "Parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      return instance.endsWith(needle.getString());
    }
  };

  class StringIndexOf : public StringFunctionType {
    EGG_NO_COPY(StringIndexOf);
  public:
    StringIndexOf(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::makeBasal(allocator, egg::ovum::BasalBits::Int | egg::ovum::BasalBits::Null)) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO int? indexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      auto index = instance.indexOfString(needle.getString());
      return (index < 0) ? egg::ovum::Variant::Null : index;
    }
  };

  class StringLastIndexOf : public StringFunctionType {
    EGG_NO_COPY(StringLastIndexOf);
  public:
    StringLastIndexOf(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::makeBasal(allocator, egg::ovum::BasalBits::Int | egg::ovum::BasalBits::Null)) {
      this->addParameter("needle", egg::ovum::Type::String, Flags::Required);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO int? lastIndexOf(string needle, int? fromIndex, int? count, bool? negate)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      auto index = instance.lastIndexOfString(needle.getString());
      return (index < 0) ? egg::ovum::Variant::Null : index;
    }
  };

  class StringJoin : public StringFunctionType {
    EGG_NO_COPY(StringJoin);
  public:
    StringJoin(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::String) {
      this->addParameter("...", egg::ovum::Type::Any, Flags::Variadic);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution&, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string join(...)
      auto n = parameters.getPositionalCount();
      switch (n) {
      case 0:
        // Joining nothing always produces an empty string
        return egg::ovum::String();
      case 1:
        // Joining a single value does not require a separator
        return parameters.getPositional(0).toString();
      }
      // Our parameters aren't in a std::vector, so we replicate String::join() here
      auto separator = instance.toUTF8();
      egg::ovum::StringBuilder sb;
      sb.add(parameters.getPositional(0).toString());
      for (size_t i = 1; i < n; ++i) {
        sb.add(separator).add(parameters.getPositional(i).toString());
      }
      return sb.str();
    }
  };

  class StringSplit : public StringFunctionType {
    EGG_NO_COPY(StringSplit);
  public:
    StringSplit(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::Any) {
      this->addParameter("separator", egg::ovum::Type::String, Flags::Required);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO string split(string separator, int? limit)
      auto separator = parameters.getPositional(0);
      if (!separator.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", separator.getRuntimeType().toString(), "'");
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
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // TODO string slice(int? begin, int? end)
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType().toString(), "'");
      }
      auto begin = p0.getInt();
      if (parameters.getPositionalCount() == 1) {
        return instance.slice(begin);
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.isInt()) {
        return this->raise(execution, "Second parameter was expected to be an 'int', not '", p0.getRuntimeType().toString(), "'");
      }
      auto end = p1.getInt();
      return instance.slice(begin, end);
    }
  };

  class StringRepeat : public StringFunctionType {
    EGG_NO_COPY(StringRepeat);
  public:
    StringRepeat(egg::ovum::IAllocator& allocator, const egg::ovum::String& name)
      : StringFunctionType(allocator, name, egg::ovum::Type::String) {
      this->addParameter("count", egg::ovum::Type::Int, Flags::Required);
    }
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string repeat(int count)
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "Parameter was expected to be an 'int', not '", p0.getRuntimeType().toString(), "'");
      }
      auto count = p0.getInt();
      if (count < 0) {
        return this->raise(execution, "Parameter was expected to be a non-negative integer, not ", count);
      }
      return instance.repeat(size_t(count));
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
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string replace(string needle, string replacement, int? occurrences)
      auto needle = parameters.getPositional(0);
      if (!needle.isString()) {
        return this->raise(execution, "First parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      auto replacement = parameters.getPositional(1);
      if (!replacement.isString()) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", needle.getRuntimeType().toString(), "'");
      }
      if (parameters.getPositionalCount() < 3) {
        return instance.replace(needle.getString(), replacement.getString());
      }
      auto occurrences = parameters.getPositional(2);
      if (!occurrences.isInt()) {
        return this->raise(execution, "Third parameter was expected to be an 'int', not '", needle.getRuntimeType().toString(), "'");
      }
      return instance.replace(needle.getString(), replacement.getString(), occurrences.getInt());
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
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string padLeft(int length, string? padding)
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType().toString(), "'");
      }
      auto length = p0.getInt();
      if (length < 0) {
        return this->raise(execution, "First parameter was expected to be a non-negative integer, not ", length);
      }
      if (parameters.getPositionalCount() < 2) {
        return instance.padLeft(size_t(length));
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.isString()) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", p1.getRuntimeType().toString(), "'");
      }
      return instance.padLeft(size_t(length), p1.getString());
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
    virtual egg::ovum::Variant executeCall(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::IParameters& parameters) const override {
      // string padRight(int length, string? padding)
      auto p0 = parameters.getPositional(0);
      if (!p0.isInt()) {
        return this->raise(execution, "First parameter was expected to be an 'int', not '", p0.getRuntimeType().toString(), "'");
      }
      auto length = p0.getInt();
      if (length < 0) {
        return this->raise(execution, "First parameter was expected to be a non-negative integer, not ", length);
      }
      if (parameters.getPositionalCount() < 2) {
        return instance.padRight(size_t(length));
      }
      auto p1 = parameters.getPositional(1);
      if (!p1.isString()) {
        return this->raise(execution, "Second parameter was expected to be a 'string', not '", p1.getRuntimeType().toString(), "'");
      }
      return instance.padRight(size_t(length), p1.getString());
    }
  };

  egg::ovum::Variant stringLength(egg::ovum::IAllocator&, const egg::ovum::String& instance, const egg::ovum::String&) {
    // This result is the actual length, not a function computing it
    return int64_t(instance.length());
  }
}

egg::yolk::Builtins::StringBuiltinFactory egg::yolk::Builtins::stringBuiltinFactory(const egg::ovum::String& property) {
  // See http://chilliant.blogspot.co.uk/2018/05/egg-strings.html
  static const std::map<egg::ovum::String, StringBuiltinFactory> table = {
    { "compareTo",    StringBuiltin::make<StringCompareTo> },
    { "contains",     StringBuiltin::make<StringContains> },
    { "endsWith",     StringBuiltin::make<StringEndsWith> },
    { "hash",         StringBuiltin::make<StringHash> },
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

egg::ovum::Variant egg::yolk::Builtins::stringBuiltin(egg::ovum::IExecution& execution, const egg::ovum::String& instance, const egg::ovum::String& property){
  auto factory = Builtins::stringBuiltinFactory(property);
  if (factory != nullptr) {
    return factory(execution.getAllocator(), instance, egg::ovum::StringBuilder::concat("string.", property));
  }
  return execution.raiseFormat("Unknown property for type 'string': '", property, "'");
}

egg::ovum::Variant egg::yolk::Builtins::builtinString(egg::ovum::IAllocator& allocator) {
  return egg::ovum::VariantFactory::createObject<Builtin_String>(allocator);
}

egg::ovum::Variant egg::yolk::Builtins::builtinType(egg::ovum::IAllocator& allocator) {
  return egg::ovum::VariantFactory::createObject<Builtin_Type>(allocator);
}

egg::ovum::Variant egg::yolk::Builtins::builtinAssert(egg::ovum::IAllocator& allocator) {
  return egg::ovum::VariantFactory::createObject<Builtin_Assert>(allocator);
}

egg::ovum::Variant egg::yolk::Builtins::builtinPrint(egg::ovum::IAllocator& allocator) {
  return egg::ovum::VariantFactory::createObject<Builtin_Print>(allocator);
}
