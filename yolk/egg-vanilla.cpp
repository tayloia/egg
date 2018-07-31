#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

namespace {
  class VanillaBase : public egg::lang::IObject {
    EGG_NO_COPY(VanillaBase);
  protected:
    std::string kind;
    egg::lang::ITypeRef type;
  public:
    VanillaBase(const std::string& kind, const egg::lang::IType& type)
      : kind(kind), type(&type) {
    }
    virtual egg::lang::ITypeRef getRuntimeType() const override {
      return this->type;
    }
    virtual egg::lang::Value call(egg::lang::IExecution& execution, const egg::lang::IParameters&) override {
      return execution.raiseFormat(this->kind, "s do not support calling with '()'");
    }
    virtual egg::lang::Value getIndex(egg::lang::IExecution& execution, const egg::lang::Value& index) override {
      if (!index.is(egg::lang::Discriminator::String)) {
        return execution.raiseFormat(this->kind, " index (property name) was expected to be 'string', not '", index.getRuntimeType()->toString(), "'");
      }
      return this->getProperty(execution, index.getString());
    }
    virtual egg::lang::Value setIndex(egg::lang::IExecution& execution, const egg::lang::Value& index, const egg::lang::Value& value) override {
      if (!index.is(egg::lang::Discriminator::String)) {
        return execution.raiseFormat(this->kind, " index (property name) was expected to be 'string', not '", index.getRuntimeType()->toString(), "'");
      }
      return this->setProperty(execution, index.getString(), value);
    }
  };

  class VanillaIteratorType : public egg::gc::NotReferenceCounted<egg::lang::IType> {
    EGG_NO_COPY(VanillaIteratorType);
  public:
    VanillaIteratorType() {}
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("<iterator>", 0);
    }
    // TODO iterable() for forEachRemaining() like Java?
    virtual AssignmentSuccess canBeAssignedFrom(const IType&) const {
      return AssignmentSuccess::Never; // TODO
    }
    virtual egg::lang::Value promoteAssignment(egg::lang::IExecution& execution, const egg::lang::Value&) const override {
      return execution.raiseFormat("Cannot re-assign iterators"); // TODO
    }
    static const VanillaIteratorType instance;
  };
  const VanillaIteratorType VanillaIteratorType::instance{};

  class VanillaIteratorBase : public VanillaBase {
    EGG_NO_COPY(VanillaIteratorBase);
  public:
    VanillaIteratorBase()
      : VanillaBase("Iterator", VanillaIteratorType::instance) {
    }
    virtual egg::lang::Value toString() const override {
      return egg::lang::Value{ this->type->toString() };
    }
    virtual egg::lang::Value getProperty(egg::lang::IExecution& execution, const egg::lang::String& property) override {
      return execution.raiseFormat("Iterators do not support properties: '.", property, "'");
    }
    virtual egg::lang::Value setProperty(egg::lang::IExecution& execution, const egg::lang::String& property, const egg::lang::Value&) override {
      return execution.raiseFormat("Iterators do not support properties: '.", property, "'");
    }
  };

  class VanillaKeyValueType : public egg::gc::NotReferenceCounted<egg::lang::IType> {
  public:
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("<keyvalue>", 0);
    }
    virtual bool iterable(egg::lang::ITypeRef& type) const override {
      // A keyvalue is a dictionary of two elements, so it is itself iterable
      type.set(&VanillaKeyValueType::instance);
      return true;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rtype) const {
      // TODO Only allow assignment of vanilla keyvalues
      if (this == &rtype) {
        return AssignmentSuccess::Always;
      }
      return AssignmentSuccess::Never;
    }
    static const VanillaKeyValueType instance;
  };
  const VanillaKeyValueType VanillaKeyValueType::instance{};

  class VanillaKeyValue : public VanillaBase {
    EGG_NO_COPY(VanillaKeyValue);
  private:
    egg::lang::Value key;
    egg::lang::Value value;
  public:
    VanillaKeyValue(const egg::lang::Value& key, const egg::lang::Value& value)
      : VanillaBase("Key-value", VanillaKeyValueType::instance), key(key), value(value) {
    }
    explicit VanillaKeyValue(const std::pair<egg::lang::String, egg::lang::Value>& keyvalue)
      : VanillaKeyValue(egg::lang::Value{ keyvalue.first }, keyvalue.second) {
    }
    virtual egg::lang::Value toString() const override {
      return egg::lang::Value{ egg::lang::String::concat("{key:", this->key.toString(), ",value:", this->value.toString(), "}") };
    }
    virtual egg::lang::Value getProperty(egg::lang::IExecution& execution, const egg::lang::String& property) override {
      auto name = property.toUTF8();
      if (name == "key") {
        return this->key;
      }
      if (name == "value") {
        return this->value;
      }
      return execution.raiseFormat("Key-values do not support property: '.", property, "'");
    }
    virtual egg::lang::Value setProperty(egg::lang::IExecution& execution, const egg::lang::String& property, const egg::lang::Value&) override {
      return execution.raiseFormat("Key-values do not support addition or modification of properties: '.", property, "'");
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution& execution) override {
      return execution.raiseFormat("Key-values do not support iteration");
    }
  };

  class VanillaArrayIndexSignature : public egg::lang::IIndexSignature {
  public:
    static const VanillaArrayIndexSignature instance;
    virtual egg::lang::ITypeRef getResultType() const override {
      return egg::lang::Type::AnyQ;
    }
    virtual egg::lang::ITypeRef getIndexType() const override {
      return egg::lang::Type::Int;
    }
  };
  const VanillaArrayIndexSignature VanillaArrayIndexSignature::instance{};

  class VanillaArrayType : public egg::gc::NotReferenceCounted<egg::lang::IType> {
  public:
    static const egg::lang::IType* getPropertyType(const std::string& property) {
      if (property == "length") {
        return egg::lang::Type::Int.get();
      }
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("any?[]", 0); // TODO
    }
    virtual const egg::lang::IIndexSignature* indexable() const override {
      // Indexing an array returns an element
      return &VanillaArrayIndexSignature::instance;
    }
    virtual bool dotable(const egg::lang::String* property, egg::lang::ITypeRef& type, egg::lang::String& reason) const override {
      // Arrays support limited properties
      if (property == nullptr) {
        type = egg::lang::Type::AnyQ;
        return true;
      }
      auto* retval = VanillaArrayType::getPropertyType(property->toUTF8());
      if (retval == nullptr) {
        reason = egg::lang::String::concat("Arrays do not support property '.", *property, "'");
        return false;
      }
      type.set(retval);
      return true;
    }
    virtual bool iterable(egg::lang::ITypeRef& type) const override {
      // Iterating an array returns the elements
      type = egg::lang::Type::AnyQ;
      return true;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rtype) const {
      // TODO Only allow assignment of vanilla arrays
      if (this == &rtype) {
        return AssignmentSuccess::Always;
      }
      return AssignmentSuccess::Never;
    }
    static const VanillaArrayType instance;
  };
  const VanillaArrayType VanillaArrayType::instance{};

  class VanillaArray : public VanillaBase {
    EGG_NO_COPY(VanillaArray);
  private:
    std::vector<egg::lang::Value> values;
  public:
    VanillaArray()
      : VanillaBase("Array", VanillaArrayType::instance) {
    }
    virtual egg::lang::Value toString() const override {
      if (this->values.empty()) {
        return egg::lang::Value{ egg::lang::String::fromUTF8("[]") };
      }
      egg::lang::StringBuilder sb;
      char between = '[';
      for (auto& value : this->values) {
        sb.add(between, value.toUTF8());
        between = ',';
      }
      sb.add(']');
      return egg::lang::Value{ sb.str() };
    }
    virtual egg::lang::Value getProperty(egg::lang::IExecution& execution, const egg::lang::String& property) override {
      auto name = property.toUTF8();
      auto retval = this->getPropertyInternal(execution, name);
      if (retval.has(egg::lang::Discriminator::FlowControl)) {
        assert(VanillaArrayType::getPropertyType(name) == nullptr);
      } else {
        assert(VanillaArrayType::getPropertyType(name) != nullptr);
      }
      return retval;
    }
    virtual egg::lang::Value setProperty(egg::lang::IExecution& execution, const egg::lang::String& property, const egg::lang::Value& value) override {
      auto name = property.toUTF8();
      if (name == "length") {
        return this->setLength(execution, value);
      }
      return execution.raiseFormat("Arrays do not support property '.", property, "'");
    }
    virtual egg::lang::Value getIndex(egg::lang::IExecution& execution, const egg::lang::Value& index) override {
      if (!index.is(egg::lang::Discriminator::Int)) {
        return execution.raiseFormat("Array index was expected to be 'int', not '", index.getRuntimeType()->toString(), "'");
      }
      auto i = index.getInt();
      if ((i < 0) || (uint64_t(i) >= uint64_t(this->values.size()))) {
        return execution.raiseFormat("Invalid array index for an array with ", this->values.size(), " element(s): ", i);
      }
      auto& element = this->values.at(size_t(i));
      assert(!element.is(egg::lang::Discriminator::Void));
      return element;
    }
    virtual egg::lang::Value setIndex(egg::lang::IExecution& execution, const egg::lang::Value& index, const egg::lang::Value& value) override {
      if (!index.is(egg::lang::Discriminator::Int)) {
        return execution.raiseFormat("Array index was expected to be 'int', not '", index.getRuntimeType()->toString(), "'");
      }
      auto i = index.getInt();
      if ((i < 0) || (i >= 0x7FFFFFFF)) {
        return execution.raiseFormat("Invalid array index: ", i);
      }
      auto u = size_t(i);
      if (u >= this->values.size()) {
        this->values.resize(u + 1, egg::lang::Value::Null);
      }
      this->values[u] = value;
      return egg::lang::Value::Void;
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution& execution) override;
    egg::lang::Value iterateNext(size_t& index) const {
      // Used by VanillaArrayIterator
      // TODO What if the array has been modified?
      if (index < this->values.size()) {
        return this->values.at(index++);
      }
      return egg::lang::Value::Void;
    }
  private:
    egg::lang::Value getPropertyInternal(egg::lang::IExecution& execution, const std::string& property) {
      if (property == "length") {
        return egg::lang::Value{ int64_t(this->values.size()) };
      }
      return execution.raiseFormat("Arrays do not support property '.", property, "'");
    }
    egg::lang::Value setPropertyInternal(egg::lang::IExecution& execution, const std::string& property, const egg::lang::Value& value) {
      if (property == "length") {
        return this->setLength(execution, value);
      }
      return execution.raiseFormat("Arrays do not support property '.", property, "'");
    }
    egg::lang::Value setLength(egg::lang::IExecution& execution, const egg::lang::Value& value) {
      if (!value.is(egg::lang::Discriminator::Int)) {
        return execution.raiseFormat("Array length was expected to be set to an 'int', not '", value.getRuntimeType()->toString(), "'");
      }
      auto n = value.getInt();
      if ((n < 0) || (n >= 0x7FFFFFFF)) {
        return execution.raiseFormat("Invalid array length: ", n);
      }
      auto u = size_t(n);
      this->values.resize(u, egg::lang::Value::Null);
      return egg::lang::Value::Void;
    }
  };

  class VanillaArrayIterator : public VanillaIteratorBase {
    EGG_NO_COPY(VanillaArrayIterator);
  private:
    egg::gc::HardRef<VanillaArray> array;
    size_t next;
  public:
    VanillaArrayIterator(egg::lang::IExecution&, const VanillaArray& array)
      : VanillaIteratorBase(), array(&array), next(0) {
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution&) override {
      return this->array->iterateNext(this->next);
    }
  };

  egg::lang::Value VanillaArray::iterate(egg::lang::IExecution& execution) {
    return egg::lang::Value::make<VanillaArrayIterator>(execution, *this);
  }

  class VanillaDictionaryIterator : public VanillaIteratorBase {
    EGG_NO_COPY(VanillaDictionaryIterator);
  private:
    typedef egg::yolk::Dictionary<egg::lang::String, egg::lang::Value> Dictionary;
    Dictionary::KeyValues keyvalues;
    size_t next;
  public:
    VanillaDictionaryIterator(egg::lang::IExecution&, const Dictionary& dictionary)
      : VanillaIteratorBase(), next(0) {
      (void)dictionary.getKeyValues(this->keyvalues);
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution&) override {
      if (this->next < this->keyvalues.size()) {
        return egg::lang::Value::make<VanillaKeyValue>(keyvalues[this->next++]);
      }
      return egg::lang::Value::Void;
    }
  };

  class VanillaDictionary : public VanillaBase {
    EGG_NO_COPY(VanillaDictionary);
  protected:
    typedef egg::yolk::Dictionary<egg::lang::String, egg::lang::Value> Dictionary;
    Dictionary dictionary;
  public:
    VanillaDictionary(const std::string& kind, const egg::lang::IType& type)
      : VanillaBase(kind, type) {
    }
    virtual egg::lang::Value toString() const override {
      Dictionary::KeyValues keyvalues;
      if (this->dictionary.getKeyValues(keyvalues) == 0) {
        return egg::lang::Value{ egg::lang::String::fromUTF8("{}") };
      }
      egg::lang::StringBuilder sb;
      char between = '{';
      for (auto& keyvalue : keyvalues) {
        sb.add(between, keyvalue.first.toUTF8(), ':', keyvalue.second.toUTF8());
        between = ',';
      }
      sb.add('}');
      return egg::lang::Value{ sb.str() };
    }
    virtual egg::lang::Value getProperty(egg::lang::IExecution& execution, const egg::lang::String& property) override {
      egg::lang::Value value;
      if (this->dictionary.tryGet(property, value)) {
        return value;
      }
      return execution.raiseFormat(this->kind, " does not support property '", property, "'");
    }
    virtual egg::lang::Value setProperty(egg::lang::IExecution&, const egg::lang::String& property, const egg::lang::Value& value) override {
      (void)this->dictionary.addOrUpdate(property, value);
      return egg::lang::Value::Void;
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution& execution) override {
      return egg::lang::Value::make<VanillaDictionaryIterator>(execution, this->dictionary);
    }
  };

  class VanillaObjectIndexSignature : public egg::lang::IIndexSignature {
  public:
    static const VanillaObjectIndexSignature instance;
    virtual egg::lang::ITypeRef getResultType() const override {
      return egg::lang::Type::AnyQ;
    }
    virtual egg::lang::ITypeRef getIndexType() const override {
      return egg::lang::Type::String;
    }
  };
  const VanillaObjectIndexSignature VanillaObjectIndexSignature::instance{};

  class VanillaObjectType : public egg::gc::NotReferenceCounted<egg::lang::IType> {
  public:
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("any?{string}", 0); // TODO
    }
    virtual const egg::lang::IIndexSignature* indexable() const override {
      // Indexing an object returns a property
      return &VanillaObjectIndexSignature::instance;
    }
    virtual bool dotable(const egg::lang::String*, egg::lang::ITypeRef& type, egg::lang::String&) const override {
      // Objects support properties
      type = egg::lang::Type::AnyQ;
      return true;
    }
    virtual bool iterable(egg::lang::ITypeRef& type) const override {
      // Iterating an object, returns the dictionary keyvalue pairs
      type.set(&VanillaKeyValueType::instance);
      return true;
    }
    virtual AssignmentSuccess canBeAssignedFrom(const IType& rtype) const {
      // TODO Only allow assignment of vanilla objects
      if (this == &rtype) {
        return AssignmentSuccess::Always;
      }
      return AssignmentSuccess::Never;
    }
    static const VanillaObjectType instance;
  };
  const VanillaObjectType VanillaObjectType::instance{};

  class VanillaObject : public VanillaDictionary {
    EGG_NO_COPY(VanillaObject);
  public:
    VanillaObject()
      : VanillaDictionary("Object", VanillaObjectType::instance) {
    }
  };

  class VanillaException : public VanillaDictionary {
    EGG_NO_COPY(VanillaException);
  private:
    static const egg::lang::String keyMessage;
    static const egg::lang::String keyLocation;
  public:
    explicit VanillaException(const egg::lang::LocationRuntime& location, const egg::lang::String& message)
      : VanillaDictionary("Exception", VanillaObjectType::instance) {
      this->dictionary.addUnique(keyMessage, egg::lang::Value{ message });
      this->dictionary.addUnique(keyLocation, egg::lang::Value{ location.toSourceString() }); // TODO use toRuntimeString
    }
    virtual egg::lang::Value toString() const override {
      egg::lang::StringBuilder sb;
      egg::lang::Value value;
      if (this->dictionary.tryGet(keyLocation, value)) {
        sb.add(value.toString(), ": ");
      }
      if (this->dictionary.tryGet(keyMessage, value)) {
        sb.add(value.toString());
      } else {
        sb.add("Exception (no message)");
      }
      return egg::lang::Value{ sb.str() };
    }
  };
  const egg::lang::String VanillaException::keyMessage = egg::lang::String::fromUTF8("message");
  const egg::lang::String VanillaException::keyLocation = egg::lang::String::fromUTF8("location");

  class VanillaFunction : public egg::lang::IObject {
    EGG_NO_COPY(VanillaFunction);
  protected:
    egg::gc::SoftRef<egg::yolk::EggProgramContext> program;
    egg::lang::ITypeRef type;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    VanillaFunction(egg::yolk::EggProgramContext& program, const egg::lang::ITypeRef& type, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : program(),
      type(type),
      block(block) {
      assert(block != nullptr);
      this->linkSoft(this->program, &program);
    }
    virtual egg::lang::Value toString() const override {
      return egg::lang::Value(egg::lang::String::concat("<", this->type->toString(), ">"));
    }
    virtual egg::lang::ITypeRef getRuntimeType() const override {
      return this->type;
    }
    virtual egg::lang::Value call(egg::lang::IExecution&, const egg::lang::IParameters& parameters) override {
      return this->program->executeFunctionCall(this->type, parameters, this->block);
    }
    virtual egg::lang::Value getProperty(egg::lang::IExecution& execution, const egg::lang::String& property) override {
      return execution.raiseFormat("'", this->type->toString(), "' does not support properties such as '.", property, "'");
    }
    virtual egg::lang::Value setProperty(egg::lang::IExecution& execution, const egg::lang::String& property, const egg::lang::Value&) override {
      return execution.raiseFormat("'", this->type->toString(), "' does not support properties such as '.", property, "'");
    }
    virtual egg::lang::Value getIndex(egg::lang::IExecution& execution, const egg::lang::Value&) override {
      return execution.raiseFormat("'", this->type->toString(), "' does not support indexing with '[]'");
    }
    virtual egg::lang::Value setIndex(egg::lang::IExecution& execution, const egg::lang::Value&, const egg::lang::Value&) override {
      return execution.raiseFormat("'", this->type->toString(), "' does not support indexing with '[]'");
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution& execution) override {
      return execution.raiseFormat("'", this->type->toString(), "' does not support iteration");
    }
  };

  class VanillaGenerator : public VanillaFunction {
    EGG_NO_COPY(VanillaGenerator);
  private:
    egg::lang::ITypeRef rettype;
    std::unique_ptr<egg::yolk::FunctionCoroutine> coroutine;
    bool completed;
  public:
    VanillaGenerator(egg::yolk::EggProgramContext& program, const egg::lang::ITypeRef& type, const egg::lang::ITypeRef& rettype, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : VanillaFunction(program, type, block),
      rettype(rettype),
      coroutine(),
      completed(false) {
    }
    virtual egg::lang::Value call(egg::lang::IExecution&, const egg::lang::IParameters& parameters) override {
      // This actually calls a generator via a coroutine
      if ((parameters.getPositionalCount() > 0) || (parameters.getNamedCount() > 0)) {
        return this->program->raiseFormat("Parameters in generator iterator calls are not supported");
      }
      auto retval = this->iterateNext();
      if (retval.stripFlowControl(egg::lang::Discriminator::Return)) {
        // The sequence has ended
        if (!retval.is(egg::lang::Discriminator::Void)) {
          return this->program->raiseFormat("Expected 'return' statement without a value in generator, but got '", retval.getRuntimeType()->toString(), "' instead");
        }
      }
      return retval;
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution&) override;
    egg::lang::Value iterateNext() {
      if (this->coroutine == nullptr) {
        // Don't re-create if we've already completed
        if (this->completed) {
          return egg::lang::Value::ReturnVoid;
        }
        this->coroutine.reset(egg::yolk::FunctionCoroutine::create(this->block));
      }
      assert(this->coroutine != nullptr);
      auto retval = this->coroutine->resume(*this->program);
      if (retval.stripFlowControl(egg::lang::Discriminator::Yield)) {
        // We yielded a value
        return retval;
      }
      // We either completed or failed
      this->completed = true;
      this->coroutine.reset(nullptr);
      if (retval.stripFlowControl(egg::lang::Discriminator::Return)) {
        // We explicitly terminated
        return retval;
      }
      return retval;
    }
  };

  class VanillaGeneratorIterator : public VanillaIteratorBase {
    EGG_NO_COPY(VanillaGeneratorIterator);
  private:
    egg::gc::SoftRef<VanillaGenerator> generator;
  public:
    explicit VanillaGeneratorIterator(VanillaGenerator& generator)
      : VanillaIteratorBase(), generator() {
      this->linkSoft(this->generator, &generator);
    }
    virtual egg::lang::Value iterate(egg::lang::IExecution&) override {
      return this->generator->iterateNext();
    }
  };

  egg::lang::Value VanillaGenerator::iterate(egg::lang::IExecution&) {
    // Create an ad hod iterator
    return egg::lang::Value{ egg::gc::HardRef<VanillaGeneratorIterator>::make(*this) };
  }
}

// Vanilla types
const egg::lang::IType& egg::yolk::EggProgram::VanillaArray = VanillaArrayType::instance;
const egg::lang::IType& egg::yolk::EggProgram::VanillaObject = VanillaObjectType::instance;

egg::lang::Value egg::yolk::EggProgramContext::raise(const egg::lang::String& message) {
  auto exception = egg::lang::Value::make<VanillaException>(this->location, message);
  exception.addFlowControl(egg::lang::Discriminator::Exception);
  return exception;
}

egg::lang::Value egg::yolk::EggProgramContext::createVanillaArray() {
  return egg::lang::Value::make<VanillaArray>();
}

egg::lang::Value egg::yolk::EggProgramContext::createVanillaObject() {
  return egg::lang::Value::make<VanillaObject>();
}

egg::lang::Value egg::yolk::EggProgramContext::createVanillaFunction(const egg::lang::ITypeRef& type, const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  return egg::lang::Value::make<VanillaFunction>(*this, type, block);
}

egg::lang::Value egg::yolk::EggProgramContext::createVanillaGenerator(const egg::lang::ITypeRef& itertype, const egg::lang::ITypeRef& rettype, const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  return egg::lang::Value::make<VanillaGenerator>(*this, itertype, rettype, block);
}
