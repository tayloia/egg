#include "yolk/yolk.h"
#include "yolk/lexers.h"
#include "yolk/egg-tokenizer.h"
#include "yolk/egg-syntax.h"
#include "yolk/egg-parser.h"
#include "yolk/egg-engine.h"
#include "yolk/egg-program.h"

namespace {
  class VanillaBase : public egg::ovum::SoftReferenceCounted<egg::ovum::IObject> {
    EGG_NO_COPY(VanillaBase);
  protected:
    std::string kind;
    egg::ovum::Type type;
  public:
    VanillaBase(egg::ovum::IAllocator& allocator, const std::string& kind, const egg::ovum::IType& type)
      : SoftReferenceCounted(allocator), kind(kind), type(&type) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // No soft links
    }
    virtual egg::ovum::Type getRuntimeType() const override {
      return this->type;
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution& execution, const egg::ovum::IParameters&) override {
      return execution.raiseFormat(this->kind, "s do not support calling with '()'");
    }
    virtual egg::ovum::Variant getIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant& index) override {
      if (!index.isString()) {
        return execution.raiseFormat(this->kind, " index (property name) was expected to be 'string', not '", index.getRuntimeType().toString(), "'");
      }
      return this->getProperty(execution, index.getString());
    }
    virtual egg::ovum::Variant setIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant& index, const egg::ovum::Variant& value) override {
      if (!index.isString()) {
        return execution.raiseFormat(this->kind, " index (property name) was expected to be 'string', not '", index.getRuntimeType().toString(), "'");
      }
      return this->setProperty(execution, index.getString(), value);
    }
  };

  class VanillaIteratorType : public egg::ovum::NotReferenceCounted<egg::ovum::TypeBase> {
    EGG_NO_COPY(VanillaIteratorType);
  public:
    VanillaIteratorType() {}
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("<iterator>", 0);
    }
    // TODO iterable() for forEachRemaining() like Java?
    virtual egg::ovum::Variant tryAssign(egg::ovum::Variant&, const egg::ovum::Variant&) const override {
      egg::ovum::Variant exception{ "Cannot re-assign iterators" };
      exception.addFlowControl(egg::ovum::VariantBits::Throw);
      return exception;
    }
    static const VanillaIteratorType instance;
  };
  const VanillaIteratorType VanillaIteratorType::instance{};

  class VanillaIteratorBase : public VanillaBase {
    EGG_NO_COPY(VanillaIteratorBase);
  public:
    explicit VanillaIteratorBase(egg::ovum::IAllocator& allocator)
      : VanillaBase(allocator, "Iterator", VanillaIteratorType::instance) {
    }
    virtual egg::ovum::Variant toString() const override {
      return egg::ovum::Variant{ this->type.toString() };
    }
    virtual egg::ovum::Variant getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      return execution.raiseFormat("Iterators do not support properties: '.", property, "'");
    }
    virtual egg::ovum::Variant setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const egg::ovum::Variant&) override {
      return execution.raiseFormat("Iterators do not support properties: '.", property, "'");
    }
  };

  class VanillaKeyValueType : public egg::ovum::NotReferenceCounted<egg::ovum::TypeBase> {
    VanillaKeyValueType(const VanillaKeyValueType&) = delete;
    VanillaKeyValueType& operator=(const VanillaKeyValueType&) = delete;
  public:
    VanillaKeyValueType() = default;
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("<keyvalue>", 0);
    }
    virtual egg::ovum::Type iterable() const override {
      // A keyvalue is a dictionary of two elements, so it is itself iterable
      return egg::ovum::Type(&VanillaKeyValueType::instance);
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
    egg::ovum::Variant key;
    egg::ovum::Variant value;
  public:
    VanillaKeyValue(egg::ovum::IAllocator& allocator, const egg::ovum::Variant& key, const egg::ovum::Variant& value)
      : VanillaBase(allocator, "Key-value", VanillaKeyValueType::instance), key(key), value(value) {
    }
    VanillaKeyValue(egg::ovum::IAllocator& allocator, const std::pair<egg::ovum::String, egg::ovum::Variant>& keyvalue)
      : VanillaKeyValue(allocator, egg::ovum::Variant{ keyvalue.first }, keyvalue.second) {
    }
    virtual egg::ovum::Variant toString() const override {
      return egg::ovum::Variant{ egg::ovum::StringBuilder::concat("{key:", this->key.toString(), ",value:", this->value.toString(), "}") };
    }
    virtual egg::ovum::Variant getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      auto name = property.toUTF8();
      if (name == "key") {
        return this->key;
      }
      if (name == "value") {
        return this->value;
      }
      return execution.raiseFormat("Key-values do not support property: '.", property, "'");
    }
    virtual egg::ovum::Variant setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const egg::ovum::Variant&) override {
      return execution.raiseFormat("Key-values do not support addition or modification of properties: '.", property, "'");
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution& execution) override {
      return execution.raiseFormat("Key-values do not support iteration");
    }
  };

  class VanillaArrayIndexSignature : public egg::ovum::IIndexSignature {
  public:
    static const VanillaArrayIndexSignature instance;
    virtual egg::ovum::Type getResultType() const override {
      return egg::ovum::Type::AnyQ;
    }
    virtual egg::ovum::Type getIndexType() const override {
      return egg::ovum::Type::Int;
    }
  };
  const VanillaArrayIndexSignature VanillaArrayIndexSignature::instance{};

  class VanillaArrayType : public egg::ovum::NotReferenceCounted<egg::ovum::TypeBase> {
    VanillaArrayType(const VanillaArrayType&) = delete;
    VanillaArrayType& operator=(const VanillaArrayType&) = delete;
  public:
    VanillaArrayType() = default;
    static const egg::ovum::IType* getPropertyType(const std::string& property) {
      if (property == "length") {
        return egg::ovum::Type::Int.get();
      }
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("any?[]", 0); // TODO
    }
    virtual const egg::ovum::IIndexSignature* indexable() const override {
      // Indexing an array returns an element
      return &VanillaArrayIndexSignature::instance;
    }
    virtual egg::ovum::Type dotable(const egg::ovum::String* property, egg::ovum::String& error) const override {
      // Arrays support limited properties
      if (property == nullptr) {
        return egg::ovum::Type::AnyQ;
      }
      auto* retval = VanillaArrayType::getPropertyType(property->toUTF8());
      if (retval == nullptr) {
        error = egg::ovum::StringBuilder::concat("Arrays do not support property '.", *property, "'");
        return nullptr;
      }
      return egg::ovum::Type(retval);
    }
    virtual egg::ovum::Type iterable() const override {
      // Iterating an array returns the elements
      return egg::ovum::Type::AnyQ;
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
    std::vector<egg::ovum::Variant> values;
  public:
    explicit VanillaArray(egg::ovum::IAllocator& allocator)
      : VanillaBase(allocator, "Array", VanillaArrayType::instance) {
    }
    virtual egg::ovum::Variant toString() const override {
      if (this->values.empty()) {
        return egg::ovum::Variant(egg::ovum::String("[]"));
      }
      egg::ovum::StringBuilder sb;
      char between = '[';
      for (auto& value : this->values) {
        sb.add(between, value.toString());
        between = ',';
      }
      sb.add(']');
      return egg::ovum::Variant{ sb.str() };
    }
    virtual egg::ovum::Variant getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      auto name = property.toUTF8();
      auto retval = this->getPropertyInternal(execution, name);
      if (retval.hasFlowControl()) {
        assert(VanillaArrayType::getPropertyType(name) == nullptr);
      } else {
        assert(VanillaArrayType::getPropertyType(name) != nullptr);
      }
      return retval;
    }
    virtual egg::ovum::Variant setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const egg::ovum::Variant& value) override {
      auto name = property.toUTF8();
      if (name == "length") {
        return this->setLength(execution, value);
      }
      return execution.raiseFormat("Arrays do not support property '.", property, "'");
    }
    virtual egg::ovum::Variant getIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant& index) override {
      if (!index.isInt()) {
        return execution.raiseFormat("Array index was expected to be 'int', not '", index.getRuntimeType().toString(), "'");
      }
      auto i = index.getInt();
      if ((i < 0) || (uint64_t(i) >= uint64_t(this->values.size()))) {
        return execution.raiseFormat("Invalid array index for an array with ", this->values.size(), " element(s): ", i);
      }
      auto& element = this->values.at(size_t(i));
      assert(!element.isVoid());
      return element;
    }
    virtual egg::ovum::Variant setIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant& index, const egg::ovum::Variant& value) override {
      if (!index.isInt()) {
        return execution.raiseFormat("Array index was expected to be 'int', not '", index.getRuntimeType().toString(), "'");
      }
      auto i = index.getInt();
      if ((i < 0) || (i >= 0x7FFFFFFF)) {
        return execution.raiseFormat("Invalid array index: ", i);
      }
      auto u = size_t(i);
      if (u >= this->values.size()) {
        this->values.resize(u + 1, egg::ovum::Variant::Null);
      }
      this->values[u] = value;
      return egg::ovum::Variant::Void;
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution& execution) override;
    egg::ovum::Variant iterateNext(size_t& index) const {
      // Used by VanillaArrayIterator
      // TODO What if the array has been modified?
      if (index < this->values.size()) {
        return this->values.at(index++);
      }
      return egg::ovum::Variant::Void;
    }
  private:
    egg::ovum::Variant getPropertyInternal(egg::ovum::IExecution& execution, const std::string& property) {
      if (property == "length") {
        return egg::ovum::Variant{ int64_t(this->values.size()) };
      }
      return execution.raiseFormat("Arrays do not support property '.", property, "'");
    }
    egg::ovum::Variant setPropertyInternal(egg::ovum::IExecution& execution, const std::string& property, const egg::ovum::Variant& value) {
      if (property == "length") {
        return this->setLength(execution, value);
      }
      return execution.raiseFormat("Arrays do not support property '.", property, "'");
    }
    egg::ovum::Variant setLength(egg::ovum::IExecution& execution, const egg::ovum::Variant& value) {
      if (!value.isInt()) {
        return execution.raiseFormat("Array length was expected to be set to an 'int', not '", value.getRuntimeType().toString(), "'");
      }
      auto n = value.getInt();
      if ((n < 0) || (n >= 0x7FFFFFFF)) {
        return execution.raiseFormat("Invalid array length: ", n);
      }
      auto u = size_t(n);
      this->values.resize(u, egg::ovum::Variant::Null);
      return egg::ovum::Variant::Void;
    }
  };

  class VanillaArrayIterator : public VanillaIteratorBase {
    EGG_NO_COPY(VanillaArrayIterator);
  private:
    egg::ovum::HardPtr<VanillaArray> array;
    size_t next;
  public:
    VanillaArrayIterator(egg::ovum::IAllocator& allocator, const VanillaArray& array)
      : VanillaIteratorBase(allocator), array(&array), next(0) {
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution&) override {
      return this->array->iterateNext(this->next);
    }
  };

  egg::ovum::Variant VanillaArray::iterate(egg::ovum::IExecution& execution) {
    return egg::ovum::VariantFactory::createObject<VanillaArrayIterator>(execution.getAllocator(), *this);
  }

  class VanillaDictionaryIterator : public VanillaIteratorBase {
    EGG_NO_COPY(VanillaDictionaryIterator);
  private:
    typedef egg::yolk::Dictionary<egg::ovum::String, egg::ovum::Variant> Dictionary;
    Dictionary::KeyValues keyvalues;
    size_t next;
  public:
    VanillaDictionaryIterator(egg::ovum::IAllocator& allocator, const Dictionary& dictionary)
      : VanillaIteratorBase(allocator), next(0) {
      (void)dictionary.getKeyValues(this->keyvalues);
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution& execution) override {
      if (this->next < this->keyvalues.size()) {
        return egg::ovum::VariantFactory::createObject<VanillaKeyValue>(execution.getAllocator(), keyvalues[this->next++]);
      }
      return egg::ovum::Variant::Void;
    }
  };

  class VanillaDictionary : public VanillaBase {
    EGG_NO_COPY(VanillaDictionary);
  protected:
    typedef egg::yolk::Dictionary<egg::ovum::String, egg::ovum::Variant> Dictionary;
    Dictionary dictionary;
  public:
    VanillaDictionary(egg::ovum::IAllocator& allocator, const std::string& kind, const egg::ovum::IType& type)
      : VanillaBase(allocator, kind, type) {
    }
    virtual egg::ovum::Variant toString() const override {
      Dictionary::KeyValues keyvalues;
      if (this->dictionary.getKeyValues(keyvalues) == 0) {
        return egg::ovum::Variant(egg::ovum::String("{}"));
      }
      egg::ovum::StringBuilder sb;
      char between = '{';
      for (auto& keyvalue : keyvalues) {
        sb.add(between, keyvalue.first, ':', keyvalue.second.toString());
        between = ',';
      }
      sb.add('}');
      return egg::ovum::Variant{ sb.str() };
    }
    virtual egg::ovum::Variant getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      egg::ovum::Variant value;
      if (this->dictionary.tryGet(property, value)) {
        return value;
      }
      return execution.raiseFormat(this->kind, " does not support property '", property, "'");
    }
    virtual egg::ovum::Variant setProperty(egg::ovum::IExecution&, const egg::ovum::String& property, const egg::ovum::Variant& value) override {
      (void)this->dictionary.addOrUpdate(property, value);
      return egg::ovum::Variant::Void;
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution& execution) override {
      return egg::ovum::VariantFactory::createObject<VanillaDictionaryIterator>(execution.getAllocator(), this->dictionary);
    }
  };

  class VanillaObjectIndexSignature : public egg::ovum::IIndexSignature {
  public:
    static const VanillaObjectIndexSignature instance;
    virtual egg::ovum::Type getResultType() const override {
      return egg::ovum::Type::AnyQ;
    }
    virtual egg::ovum::Type getIndexType() const override {
      return egg::ovum::Type::String;
    }
  };
  const VanillaObjectIndexSignature VanillaObjectIndexSignature::instance{};

  class VanillaObjectType : public egg::ovum::NotReferenceCounted<egg::ovum::TypeBase> {
    VanillaObjectType(const VanillaObjectType&) = delete;
    VanillaObjectType& operator=(const VanillaObjectType&) = delete;
  public:
    VanillaObjectType() = default;
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair("any?{string}", 0); // TODO
    }
    virtual const egg::ovum::IIndexSignature* indexable() const override {
      // Indexing an object returns a property
      return &VanillaObjectIndexSignature::instance;
    }
    virtual egg::ovum::Type dotable(const egg::ovum::String*, egg::ovum::String&) const override {
      // Objects support properties
      return egg::ovum::Type::AnyQ;
    }
    virtual egg::ovum::Type iterable() const override {
      // Iterating an object, returns the dictionary keyvalue pairs
      return egg::ovum::Type(&VanillaKeyValueType::instance);
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
    explicit VanillaObject(egg::ovum::IAllocator& allocator)
      : VanillaDictionary(allocator, "Object", VanillaObjectType::instance) {
    }
  };

  class VanillaException : public VanillaDictionary {
    EGG_NO_COPY(VanillaException);
  private:
    static const egg::ovum::String keyMessage;
    static const egg::ovum::String keyLocation;
  public:
    VanillaException(egg::ovum::IAllocator& allocator, const egg::ovum::LocationRuntime& location, const egg::ovum::String& message)
      : VanillaDictionary(allocator, "Exception", VanillaObjectType::instance) {
      this->dictionary.addUnique(keyMessage, egg::ovum::Variant{ message });
      this->dictionary.addUnique(keyLocation, egg::ovum::Variant{ location.toSourceString() }); // TODO use toRuntimeString
    }
    virtual egg::ovum::Variant toString() const override {
      egg::ovum::StringBuilder sb;
      egg::ovum::Variant value;
      if (this->dictionary.tryGet(keyLocation, value)) {
        sb.add(value.toString(), ": ");
      }
      if (this->dictionary.tryGet(keyMessage, value)) {
        sb.add(value.toString());
      } else {
        sb.add("Exception (no message)");
      }
      return egg::ovum::Variant{ sb.str() };
    }
  };
  const egg::ovum::String VanillaException::keyMessage = "message";
  const egg::ovum::String VanillaException::keyLocation = "location";

  class VanillaFunction : public egg::ovum::SoftReferenceCounted<egg::ovum::IObject> {
    EGG_NO_COPY(VanillaFunction);
  protected:
    egg::ovum::SoftPtr<egg::yolk::EggProgramContext> program;
    egg::ovum::Type type;
    std::shared_ptr<egg::yolk::IEggProgramNode> block;
  public:
    VanillaFunction(egg::ovum::IAllocator& allocator, egg::yolk::EggProgramContext& program, const egg::ovum::Type& type, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : SoftReferenceCounted(allocator),
        type(type),
        block(block) {
      assert(block != nullptr);
      this->program.set(*this, &program);
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->program.visit(visitor);
    }
    virtual egg::ovum::Variant toString() const override {
      return egg::ovum::Variant(egg::ovum::StringBuilder::concat("<", this->type.toString(), ">"));
    }
    virtual egg::ovum::Type getRuntimeType() const override {
      return this->type;
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution&, const egg::ovum::IParameters& parameters) override {
      return this->program->executeFunctionCall(this->type, parameters, this->block);
    }
    virtual egg::ovum::Variant getProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property) override {
      return execution.raiseFormat("'", this->type.toString(), "' does not support properties such as '.", property, "'");
    }
    virtual egg::ovum::Variant setProperty(egg::ovum::IExecution& execution, const egg::ovum::String& property, const egg::ovum::Variant&) override {
      return execution.raiseFormat("'", this->type.toString(), "' does not support properties such as '.", property, "'");
    }
    virtual egg::ovum::Variant getIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant&) override {
      return execution.raiseFormat("'", this->type.toString(), "' does not support indexing with '[]'");
    }
    virtual egg::ovum::Variant setIndex(egg::ovum::IExecution& execution, const egg::ovum::Variant&, const egg::ovum::Variant&) override {
      return execution.raiseFormat("'", this->type.toString(), "' does not support indexing with '[]'");
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution& execution) override {
      return execution.raiseFormat("'", this->type.toString(), "' does not support iteration");
    }
  };

  class VanillaGenerator : public VanillaFunction {
    EGG_NO_COPY(VanillaGenerator);
  private:
    egg::ovum::Type rettype;
    egg::ovum::HardPtr<egg::yolk::FunctionCoroutine> coroutine;
    bool completed;
  public:
    VanillaGenerator(egg::ovum::IAllocator& allocator, egg::yolk::EggProgramContext& program, const egg::ovum::Type& type, const egg::ovum::Type& rettype, const std::shared_ptr<egg::yolk::IEggProgramNode>& block)
      : VanillaFunction(allocator, program, type, block),
      rettype(rettype),
      coroutine(),
      completed(false) {
    }
    virtual egg::ovum::Variant call(egg::ovum::IExecution&, const egg::ovum::IParameters& parameters) override {
      // This actually calls a generator via a coroutine
      if ((parameters.getPositionalCount() > 0) || (parameters.getNamedCount() > 0)) {
        return this->program->raiseFormat("Parameters in generator iterator calls are not supported");
      }
      auto retval = this->iterateNext();
      if (retval.stripFlowControl(egg::ovum::VariantBits::Return)) {
        // The sequence has ended
        if (!retval.isVoid()) {
          return this->program->raiseFormat("Expected 'return' statement without a value in generator, but got '", retval.getRuntimeType().toString(), "' instead");
        }
      }
      return retval;
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution&) override;
    egg::ovum::Variant iterateNext() {
      if (this->coroutine == nullptr) {
        // Don't re-create if we've already completed
        if (this->completed) {
          return egg::ovum::Variant::ReturnVoid;
        }
        this->coroutine = egg::yolk::FunctionCoroutine::create(this->allocator, this->block);
      }
      assert(this->coroutine != nullptr);
      auto retval = this->coroutine->resume(*this->program);
      if (retval.stripFlowControl(egg::ovum::VariantBits::Yield)) {
        // We yielded a value
        return retval;
      }
      // We either completed or failed
      this->completed = true;
      this->coroutine = nullptr;
      if (retval.stripFlowControl(egg::ovum::VariantBits::Return)) {
        // We explicitly terminated
        return retval;
      }
      return retval;
    }
  };

  class VanillaGeneratorIterator : public VanillaIteratorBase {
    EGG_NO_COPY(VanillaGeneratorIterator);
  private:
    egg::ovum::SoftPtr<VanillaGenerator> generator;
  public:
    VanillaGeneratorIterator(egg::ovum::IAllocator& allocator, VanillaGenerator& generator)
      : VanillaIteratorBase(allocator) {
      this->generator.set(*this, &generator);
    }
    virtual egg::ovum::Variant iterate(egg::ovum::IExecution&) override {
      return this->generator->iterateNext();
    }
  };

  egg::ovum::Variant VanillaGenerator::iterate(egg::ovum::IExecution& execution) {
    // Create an ad hod iterator
    return egg::ovum::VariantFactory::createObject<VanillaGeneratorIterator>(execution.getAllocator(), *this);
  }
}

// Vanilla types
const egg::ovum::IType& egg::yolk::EggProgram::VanillaArray = VanillaArrayType::instance;
const egg::ovum::IType& egg::yolk::EggProgram::VanillaObject = VanillaObjectType::instance;

egg::ovum::Variant egg::yolk::EggProgramContext::createVanillaArray() {
  return egg::ovum::VariantFactory::createObject<VanillaArray>(this->allocator);
}

egg::ovum::Variant egg::yolk::EggProgramContext::createVanillaObject() {
  return egg::ovum::VariantFactory::createObject<VanillaObject>(this->allocator);
}

egg::ovum::Variant egg::yolk::EggProgramContext::createVanillaFunction(const egg::ovum::Type& type, const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  return egg::ovum::VariantFactory::createObject<VanillaFunction>(this->allocator, *this, type, block);
}

egg::ovum::Variant egg::yolk::EggProgramContext::createVanillaGenerator(const egg::ovum::Type& itertype, const egg::ovum::Type& rettype, const std::shared_ptr<egg::yolk::IEggProgramNode>& block) {
  return egg::ovum::VariantFactory::createObject<VanillaGenerator>(this->allocator, *this, itertype, rettype, block);
}

egg::ovum::Variant egg::yolk::EggProgramContext::createVanillaException(const egg::ovum::String& message) {
  auto exception = egg::ovum::VariantFactory::createObject<VanillaException>(this->allocator, this->location, message);
  exception.addFlowControl(egg::ovum::VariantBits::Throw);
  return exception;
}
