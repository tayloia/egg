#include "ovum/ovum.h"
#include "ovum/dictionary.h"
#include "ovum/node.h"
#include "ovum/slot.h"
#include "ovum/print.h"
#include "ovum/builtin.h"
#include "ovum/vanilla.h"

namespace {
  using namespace egg::ovum;

  class Type_Shape : public NotSoftReferenceCounted<IType> {
    Type_Shape(const Type_Shape&) = delete;
    Type_Shape& operator=(const Type_Shape&) = delete;
  private:
    ObjectShape shape;
  public:
    Type_Shape(const IFunctionSignature* callable, const IPropertySignature* dotable, const IIndexSignature* indexable, const IIteratorSignature* iterable)
      : shape({ callable, dotable, indexable, iterable }) {
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual const IntShape* getIntShape() const override {
      // We are not shaped like an int
      return nullptr;
    }
    virtual const FloatShape* getFloatShape() const override {
      // We are not shaped like a float
      return nullptr;
    }
    virtual const ObjectShape* getStringShape() const override {
      // We are not shaped like a string
      return nullptr;
    }
    virtual const ObjectShape* getObjectShape(size_t index) const override {
      // We only have one object shape
      return (index == 0) ? &this->shape : nullptr;
    }
    virtual size_t getObjectShapeCount() const override {
      // We only have one object shape
      return 1;
    }
    virtual String describeValue() const override {
      // TODO i18n
      return StringBuilder::concat("Value of type '", this->toStringPrecedence().first, "'");
    }
  };

  class Type_Array final : public Type_Shape {
    Type_Array(const Type_Array&) = delete;
    Type_Array& operator=(const Type_Array&) = delete;
  private:
    TypeBuilderProperties dotable;
    TypeBuilderIndexable indexable;
    TypeBuilderIterable iterable;
  public:
    Type_Array()
      : Type_Shape(nullptr, &dotable, &indexable, &iterable),
        dotable(true),
        indexable(Type::AnyQ, Type::Int, Modifiability::Read | Modifiability::Write | Modifiability::Mutate),
        iterable(Type::AnyQ) {
      this->dotable.add(Type::Int, "length", Modifiability::Read);
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "any?[]", 1 };
    }
  };

  class Type_DictionaryBase : public Type_Shape {
    Type_DictionaryBase(const Type_DictionaryBase&) = delete;
    Type_DictionaryBase& operator=(const Type_DictionaryBase&) = delete;
  protected:
    class KeyValueIterable : public IIteratorSignature {
    public:
      virtual Type getType() const override {
        // This convolution is necessary because 'keyvalue' types are recursively defined
        return Vanilla::getKeyValueType();
      }
    };
    TypeBuilderProperties dotable;
    TypeBuilderIndexable indexable;
    KeyValueIterable iterable;
  public:
    Type_DictionaryBase(Modifiability modifiability, bool closed)
      : Type_Shape(nullptr, &dotable, &indexable, &iterable),
        dotable(closed),
        indexable(Type::AnyQ, Type::String, modifiability),
        iterable() {
    }
  };

  class Type_Dictionary final : public Type_DictionaryBase {
    Type_Dictionary(const Type_Dictionary&) = delete;
    Type_Dictionary& operator=(const Type_Dictionary&) = delete;
  public:
    Type_Dictionary()
      : Type_DictionaryBase(Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete, false) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "dictionary", 0 };
    }
  };

  class Type_KeyValue final : public Type_DictionaryBase {
    Type_KeyValue(const Type_KeyValue&) = delete;
    Type_KeyValue& operator=(const Type_KeyValue&) = delete;
  public:
    Type_KeyValue()
      : Type_DictionaryBase(Modifiability::Read, true) {
      this->dotable.add(Type::String, "key", Modifiability::Read);
      this->dotable.add(Type::AnyQ, "value", Modifiability::Read);
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "keyvalue", 0 };
    }
  };
}

// Vanilla types
egg::ovum::Type egg::ovum::Vanilla::getArrayType() {
  static const Type_Array instance;
  return Type(&instance);
}

egg::ovum::Type egg::ovum::Vanilla::getDictionaryType() {
  static const Type_Dictionary instance;
  return Type(&instance);
}

egg::ovum::Type egg::ovum::Vanilla::getKeyValueType() {
  static const Type_KeyValue instance;
  return Type(&instance);
}

namespace {
  using namespace egg::ovum;

  class VanillaStringValueMap : public SoftReferenceCounted<IVanillaMap<String, Value>> {
    VanillaStringValueMap(const VanillaStringValueMap&) = delete;
    VanillaStringValueMap& operator=(const VanillaStringValueMap&) = delete;
  private:
    using Container = SlotMap<String>;
    Container container;
  public:
    VanillaStringValueMap(IAllocator& allocator)
      : SoftReferenceCounted(allocator) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // WIBBLE
    }
    virtual bool add(const String& key, const Value& value) override {
      return this->container.add(this->allocator, key, value);
    }
    virtual bool get(const String& key, Value& value) const override {
      auto ref = this->container.getOrNull(key);
      if (ref != nullptr) {
        value = Value(*ref->get());
        return true;
      }
      return false;
    }
    virtual void set(const String& key, const Value& value) override {
      (void)this->container.set(this->allocator, key, value);
    }
    virtual Value mut(const String& key, Mutation mutation, const Value& value) override {
      auto slot = this->container.getOrNull(key);
      if (slot == nullptr) {
        return ValueFactory::createThrowString(this->allocator, "Object does not have a property named '", key, "'");
      }
      String error;
      auto result = slot->mutate(mutation, value, error);
      if (result->getFlags() == ValueFlags::Void) {
        return ValueFactory::createThrowString(this->allocator, error);
      }
      return result;
    }
    virtual bool del(const String& key) override {
      return this->container.remove(key);
    }
    void toString(StringBuilder& sb, char& separator) const {
      this->container.foreach([&](const String& key, const Slot& slot) {
        auto* value = slot.get();
        if (value != nullptr) {
          sb.add(separator, key, ':');
          Print::add(sb, Value(*value));
          separator = ',';
        }
      });
    }
  };

  class VanillaBase : public SoftReferenceCounted<IObject> {
    VanillaBase(const VanillaBase&) = delete;
    VanillaBase& operator=(const VanillaBase&) = delete;
  public:
    explicit VanillaBase(IAllocator& allocator) : SoftReferenceCounted(allocator) {}
    virtual void softVisitLinks(const Visitor&) const override {
      // WIBBLE
    }
    virtual bool validate() const override {
      return true;
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      sb.add('<', this->getRuntimeType().toString(), '>');
    }
    virtual Value call(IExecution& execution, const IParameters&) override {
      return execution.raise(this->trailing("does not support calling with '()'"));
    }
    virtual Value getProperty(IExecution& execution, const String&) override {
      return execution.raise(this->trailing("does not support properties [get]"));
    }
    virtual Value setProperty(IExecution& execution, const String&, const Value&) override {
      return execution.raise(this->trailing("does not support properties [set]"));
    }
    virtual Value mutProperty(IExecution& execution, const String&, Mutation, const Value&) override {
      return execution.raise(this->trailing("does not support properties [mut]"));
    }
    virtual Value delProperty(IExecution& execution, const String&) override {
      return execution.raise(this->trailing("does not support properties [del]"));
    }
    virtual Value refProperty(IExecution& execution, const String&) override {
      return execution.raise(this->trailing("does not support properties [ref]"));
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return execution.raise(this->trailing("does not support indexing with '[]' [get]"));
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return execution.raise(this->trailing("does not support indexing with '[]' [set]"));
    }
    virtual Value mutIndex(IExecution& execution, const Value&, Mutation, const Value&) override {
      return execution.raise(this->trailing("does not support indexing with '[]' [mut]"));
    }
    virtual Value delIndex(IExecution& execution, const Value&) override {
      return execution.raise(this->trailing("does not support indexing with '[]' [del]"));
    }
    virtual Value refIndex(IExecution& execution, const Value&) override {
      return execution.raise(this->trailing("does not support indexing with '[]' [ref]"));
    }
    virtual Value getPointee(IExecution& execution) override {
      return execution.raise(this->trailing("does not support pointer dereferencing with '*' [get]"));
    }
    virtual Value setPointee(IExecution& execution, const Value&) override {
      return execution.raise(this->trailing("does not support pointer dereferencing with '*' [set]"));
    }
    virtual Value mutPointee(IExecution& execution, Mutation, const Value&) override {
      return execution.raise(this->trailing("does not support pointer dereferencing with '*' [mut]"));
    }
    virtual Value iterate(IExecution& execution) override {
      return execution.raise(this->trailing("does not support iteration"));
    }
  protected:
    virtual String trailing(const char* suffix) const {
      return StringBuilder::concat(this->getRuntimeType().describeValue(), " ", suffix);
    }
  };

  class VanillaArray : public VanillaBase {
    VanillaArray(const VanillaArray&) = delete;
    VanillaArray& operator=(const VanillaArray&) = delete;
  private:
    std::vector<Slot> container;
  public:
    VanillaArray(IAllocator& allocator, size_t fixed)
      : VanillaBase(allocator) {
      this->resize(fixed);
      assert(this->validate());
    }
    virtual Type getRuntimeType() const override {
      return Vanilla::getArrayType();
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (property == "length") {
        return ValueFactory::createInt(this->allocator, Int(this->container.size()));
      }
      return execution.raiseFormat("Array does not have property: '", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value& value) override {
      if (property == "length") {
        Int length;
        if (!value->getInt(length)) {
          return execution.raiseFormat("Array length was expected to be set to an 'int', not '", value->getRuntimeType().toString(), "'");
        }
        if ((length < 0) || (length > 0x7FFFFFFF)) {
          return execution.raiseFormat("Invalid array length: ", length);
        }
        this->resize(size_t(length));
        return Value::Void;
      }
      return execution.raiseFormat("Array does not have property: '", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value& index) override {
      Int i;
      if (!index->getInt(i)) {
        return execution.raiseFormat("Array index was expected to be an 'int', not '", index->getRuntimeType().toString(), "'");
      }
      auto u = size_t(i);
      if (u >= this->container.size()) {
        return execution.raiseFormat("Invalid array index for an array with ", this->container.size(), " element(s): ", i);
      }
      auto* value = this->container[u].get();
      assert(value != nullptr);
      return Value(*value);
    }
    virtual Value setIndex(IExecution& execution, const Value& index, const Value& value) override {
      Int i;
      if (!index->getInt(i)) {
        return execution.raiseFormat("Array index was expected to be an 'int', not '", index->getRuntimeType().toString(), "'");
      }
      auto u = size_t(i);
      if (u >= this->container.size()) {
        return execution.raiseFormat("Invalid array index for an array with ", this->container.size(), " element(s): ", i);
      }
      this->container[u].set(value);
      return Value::Void;
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      char separator = '[';
      for (auto& slot : this->container) {
        sb.add(separator);
        auto* value = slot.get();
        if (value != nullptr) {
          sb.add(value->toString());
        }
        separator = ',';
      }
      if (separator == '[') {
        sb.add(separator);
      }
      sb.add(']');
    }
  private:
    void resize(size_t size) {
      // Cannot simply resize because slots have no default constructor
      auto before = this->container.size();
      if (size < before) {
        for (auto i = size; i < before; ++i) {
          this->container.pop_back();
        }
      } else if (size > before) {
        this->container.reserve(size);
        for (auto i = before; i < size; ++i) {
          this->container.emplace_back(allocator, Value::Null);
        }
      }
      assert(this->container.size() == size);
    }
  };

  class VanillaDictionary : public VanillaBase {
    VanillaDictionary(const VanillaDictionary&) = delete;
    VanillaDictionary& operator=(const VanillaDictionary&) = delete;
  protected:
    VanillaStringValueMap map;
  public:
    explicit VanillaDictionary(IAllocator& allocator)
      : VanillaBase(allocator),
        map(allocator) {
      assert(this->validate());
    }
    virtual Type getRuntimeType() const override {
      return Vanilla::getDictionaryType();
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (!this->map.get(property, value)) {
        return execution.raiseFormat("Object does not have property: '", property, "'");
      }
      return value;
    }
    virtual Value setProperty(IExecution&, const String& property, const Value& value) override {
      this->map.set(property, value);
      return Value::Void;
    }
    virtual Value mutProperty(IExecution& execution, const String& key, Mutation mutation, const Value& value) override {
      auto result = this->map.mut(key, mutation, value);
      if (result->getFlags() == (ValueFlags::Throw | ValueFlags::String)) {
        // The mutation failed with a thrown string: augment it with the location
        Value thrown;
        if (result->getInner(thrown)) {
          result = execution.raise(thrown->toString());
        }
      }
      return result;
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      char separator = '{';
      this->map.toString(sb, separator);
      if (separator == '{') {
        sb.add(separator);
      }
      sb.add('}');
    }
  };

  class VanillaObject : public VanillaBase {
    VanillaObject(const VanillaObject&) = delete;
    VanillaObject& operator=(const VanillaObject&) = delete;
  protected:
    VanillaStringValueMap map; // WIBBLE VanillaValueValueMap
  public:
    explicit VanillaObject(IAllocator& allocator)
      : VanillaBase(allocator),
        map(allocator) {
      assert(this->validate());
    }
    virtual Type getRuntimeType() const override {
      return Type::Object;
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (!this->map.get(property, value)) {
        return execution.raiseFormat("Object does not have property: '", property, "'");
      }
      return value;
    }
    virtual Value setProperty(IExecution&, const String& property, const Value& value) override {
      this->map.set(property, value);
      return Value::Void;
    }
    virtual Value mutProperty(IExecution& execution, const String& key, Mutation mutation, const Value& value) override {
      auto result = this->map.mut(key, mutation, value);
      if (result->getFlags() == (ValueFlags::Throw | ValueFlags::String)) {
        // The mutation failed with a thrown string: augment it with the location
        Value thrown;
        if (result->getInner(thrown)) {
          result = execution.raise(thrown->toString());
        }
      }
      return result;
    }
  };

  class VanillaPredicate : public VanillaBase {
    VanillaPredicate(const VanillaPredicate&) = delete;
    VanillaPredicate& operator=(const VanillaPredicate&) = delete;
  private:
    Vanilla::IPredicateCallback& callback; // Guaranteed to be long-lived
    Node node;
  public:
    VanillaPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node)
      : VanillaBase(allocator),
      callback(callback),
      node(&node) {
      assert(this->node != nullptr);
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // There are no soft link to visit
    }
    virtual Type getRuntimeType() const override {
      return Type::Object; // WIBBLE
    }
    virtual Value call(IExecution&, const IParameters& parameters) override {
      assert(parameters.getNamedCount() == 0);
      assert(parameters.getPositionalCount() == 0);
      (void)parameters; // ignore the parameters
      return this->callback.predicateCallback(*this->node);
    }
    virtual Value getProperty(IExecution& execution, const String&) override {
      return execution.raise("Internal runtime error: Predicates do not support properties");
    }
    virtual Value setProperty(IExecution& execution, const String&, const Value&) override {
      return execution.raise("Internal runtime error: Predicates do not support properties");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return execution.raise("Internal runtime error: Predicates do not support indexing");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return execution.raise("Internal runtime error: Predicates do not support indexing");
    }
    virtual Value iterate(IExecution& execution) override {
      return execution.raise("Internal runtime error: Predicates do not support iteration");
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      sb.add("<predicate>");
    }
  };

  class VanillaError : public VanillaObject {
    VanillaError(const VanillaError&) = delete;
    VanillaError& operator=(const VanillaError&) = delete;
  private:
    String readable;
  public:
    VanillaError(IAllocator& allocator, const LocationSource& location, const String& message)
      : VanillaObject(allocator) {
      assert(this->validate());
      this->map.add("message", ValueFactory::create(allocator, message));
      StringBuilder sb;
      location.formatSourceString(sb);
      this->map.add("location", ValueFactory::createUTF8(allocator, sb.toUTF8()));
      if (!sb.empty()) {
        sb.add(':', ' ');
      }
      sb.add(message);
      this->readable = sb.str();
    }
    virtual void toStringBuilder(StringBuilder& sb) const override {
      sb.add(this->readable);
    }
  };
}

egg::ovum::Object egg::ovum::VanillaFactory::createArray(IAllocator& allocator, size_t fixed) {
  return ObjectFactory::create<VanillaArray>(allocator, fixed);
}

egg::ovum::Object egg::ovum::VanillaFactory::createDictionary(IAllocator& allocator) {
  return ObjectFactory::create<VanillaDictionary>(allocator);
}

egg::ovum::Object egg::ovum::VanillaFactory::createObject(IAllocator& allocator) {
  return ObjectFactory::create<VanillaObject>(allocator);
}

egg::ovum::Object egg::ovum::VanillaFactory::createError(IAllocator& allocator, const LocationSource& location, const String& message) {
  return ObjectFactory::create<VanillaError>(allocator, location, message);
}

egg::ovum::Object egg::ovum::VanillaFactory::createPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node) {
  return ObjectFactory::create<VanillaPredicate>(allocator, callback, node);
}
