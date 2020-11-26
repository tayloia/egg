#include "ovum/ovum.h"
#include "ovum/dictionary.h"
#include "ovum/node.h"
#include "ovum/slot.h"
#include "ovum/builtin.h"
#include "ovum/vanilla.h"

namespace {
  using namespace egg::ovum;

  class TypeBase : public NotSoftReferenceCounted<IType> {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  protected:
    static const Modifiability ReadWriteMutate = Modifiability::Read | Modifiability::Write | Modifiability::Mutate;
    static const Modifiability ReadWriteMutateDelete = Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
  private:
    ObjectShape shape;
  public:
    TypeBase(const IFunctionSignature* callable, const IPropertySignature* dotable, const IIndexSignature* indexable, const IIteratorSignature* iterable)
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

  class Type_Iterator final : public TypeBase {
    Type_Iterator(const Type_Iterator&) = delete;
    Type_Iterator& operator=(const Type_Iterator&) = delete;
  private:
    TypeBuilderCallable callable;
  public:
    explicit Type_Iterator(const Type& rettype)
      : TypeBase(&callable, nullptr, nullptr, nullptr),
        callable(rettype, {}) {
      assert(rettype.hasAnyFlags(ValueFlags::Void));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto retval = this->callable.getReturnType()->toStringPrecedence();
      return { '(' + retval.first + ')' + '(' + ')' , 0 };
    }
    virtual String describeValue() const override {
      // TODO i18n
      return "Iterator";
    }
  };

  class Type_Array final : public TypeBase {
    Type_Array(const Type_Array&) = delete;
    Type_Array& operator=(const Type_Array&) = delete;
  private:
    TypeBuilderProperties dotable;
    TypeBuilderIndexable indexable;
    TypeBuilderIterable iterable;
  public:
    Type_Array()
      : TypeBase(nullptr, &dotable, &indexable, &iterable),
        dotable(),
        indexable(Type::AnyQ, Type::Int, ReadWriteMutate),
        iterable(Type::AnyQ) {
      this->dotable.add(Type::Int, "length", ReadWriteMutate);
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "any?[]", 1 };
    }
    virtual String describeValue() const override {
      // TODO i18n
      return "Array";
    }
  };

  class Type_DictionaryBase : public TypeBase {
    Type_DictionaryBase(const Type_DictionaryBase&) = delete;
    Type_DictionaryBase& operator=(const Type_DictionaryBase&) = delete;
  protected:
    class KeyValueIterable : public IIteratorSignature {
    public:
      virtual Type getType() const override {
        return Vanilla::getKeyValueType();
      }
    };
    TypeBuilderProperties dotable;
    TypeBuilderIndexable indexable;
    KeyValueIterable iterable;
  public:
    Type_DictionaryBase(Modifiability modifiability, const Type& unknownType, Modifiability unknownModifiability)
      : TypeBase(nullptr, &dotable, &indexable, &iterable),
        dotable(unknownType, unknownModifiability),
        indexable(Type::AnyQ, Type::String, modifiability),
        iterable() {
    }
  };

  class Type_Dictionary final : public Type_DictionaryBase {
    Type_Dictionary(const Type_Dictionary&) = delete;
    Type_Dictionary& operator=(const Type_Dictionary&) = delete;
  public:
    Type_Dictionary()
      : Type_DictionaryBase(ReadWriteMutateDelete, Type::AnyQ, ReadWriteMutateDelete) {
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return { "any?{string}", 0 };
    }
    virtual String describeValue() const override {
      // TODO i18n
      return "Object";
    }
  };

  class Type_KeyValue final : public Type_DictionaryBase {
    Type_KeyValue(const Type_KeyValue&) = delete;
    Type_KeyValue& operator=(const Type_KeyValue&) = delete;
  public:
    Type_KeyValue()
      : Type_DictionaryBase(Modifiability::Read, nullptr, Modifiability::None) {
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

  class VanillaValueArray final : public SoftReferenceCounted<IVanillaArray<Value>> {
    VanillaValueArray(const VanillaValueArray&) = delete;
    VanillaValueArray& operator=(const VanillaValueArray&) = delete;
  private:
    using Container = SlotArray;
    Container container;
  public:
    VanillaValueArray(IAllocator& allocator, size_t length)
      : SoftReferenceCounted(allocator),
        container(length) {
      assert(this->validate());
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->container.softVisitLinks(visitor);
    }
    virtual bool get(size_t index, Value& value) const override {
      auto* slot = this->container.get(index);
      if (slot != nullptr) {
        auto underlying = slot->get();
        value = (underlying == nullptr) ? Value::Void : Value(*underlying);
        return true;
      }
      return false;
    }
    virtual bool set(size_t index, const Value& value) override {
      return this->container.set(this->allocator, index, value) != nullptr;
    }
    virtual Value mut(size_t index, Mutation mutation, const Value& value) override {
      auto slot = this->container.get(index);
      if (slot == nullptr) {
        return ValueFactory::createThrowString(this->allocator, "Array does not have element ", index);
      }
      Value before;
      auto retval = slot->mutate(Type::AnyQ, mutation, value, before);
      if (retval != Type::Assignment::Success) {
        return ValueFactory::createThrowString(this->allocator, "Cannot mutate array value: WOBBLE");
      }
      return before;
    }
    virtual size_t length() const override {
      return this->container.length();
    }
    virtual bool resize(size_t size) override {
      this->container.resize(this->allocator, size);
      return true;
    }
    void print(Printer& printer, char& separator) const {
      this->container.foreach([&](const Slot& slot) {
        printer << separator;
        auto* value = slot.get();
        if (value != nullptr) {
          printer.write(Value(*value));
        }
        separator = ',';
      });
    }
  };

  class VanillaStringValueMap final : public SoftReferenceCounted<IVanillaMap<String, Value>> {
    VanillaStringValueMap(const VanillaStringValueMap&) = delete;
    VanillaStringValueMap& operator=(const VanillaStringValueMap&) = delete;
  private:
    using Container = SlotMap<String>;
    Container container;
  public:
    VanillaStringValueMap(IAllocator& allocator)
      : SoftReferenceCounted(allocator) {
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->container.softVisitLinks(visitor);
    }
    virtual bool add(const String& key, const Value& value) override {
      return this->container.add(this->allocator, key, value);
    }
    virtual bool get(const String& key, Value& value) const override {
      auto slot = this->container.getOrNull(key);
      if (slot != nullptr) {
        auto underlying = slot->get();
        value = (underlying == nullptr) ? Value::Void : Value(*underlying);
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
      Value before;
      auto retval = slot->mutate(Type::AnyQ, mutation, value, before);
      if (retval != Type::Assignment::Success) {
        return ValueFactory::createThrowString(this->allocator, "Cannot mutate map value: WOBBLE");
      }
      return before;
    }
    virtual bool del(const String& key) override {
      return this->container.remove(key);
    }
    size_t length() const {
      return this->container.length();
    }
    bool getKeyValue(size_t index, String& key, Value& value) const {
      auto slot = this->container.getByIndex(index, key);
      if (slot != nullptr) {
        auto underlying = slot->get();
        value = (underlying == nullptr) ? Value::Void : Value(*underlying);
        return true;
      }
      return false;
    }
    void print(Printer& printer, char& separator) const {
      this->container.foreach([&](const String& key, const Slot& slot) {
        auto* value = slot.get();
        printer << separator << key.toUTF8() << ':';
        if (value != nullptr) {
          printer.write(Value(*value));
        }
        separator = ',';
      });
    }
  };

  class VanillaBase : public SoftReferenceCounted<IObject> {
    VanillaBase(const VanillaBase&) = delete;
    VanillaBase& operator=(const VanillaBase&) = delete;
  public:
    explicit VanillaBase(IAllocator& allocator) : SoftReferenceCounted(allocator) {}
    virtual bool validate() const override {
      return true;
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
    virtual void print(Printer& printer) const override {
      printer << '<' << this->getRuntimeType().toString() << '>';
    }
  protected:
    virtual String trailing(const char* suffix) const {
      return StringBuilder::concat(this->getRuntimeType().describeValue(), " ", suffix);
    }
  };

  class VanillaIterable : public VanillaBase {
    VanillaIterable(const VanillaIterable&) = delete;
    VanillaIterable& operator=(const VanillaIterable&) = delete;
  public:
    explicit VanillaIterable(IAllocator& allocator) : VanillaBase(allocator) {}
    struct State {
      Int a;
      Int b;
    };
    virtual State iterateStart(IExecution& execution) const = 0;
    virtual Value iterateNext(IExecution& execution, State& state) const = 0;
    Value createIterator(IExecution& execution, const Type& elementType);
  };

  class VanillaIterator final : public VanillaBase {
    VanillaIterator(const VanillaIterator&) = delete;
    VanillaIterator& operator=(const VanillaIterator&) = delete;
  private:
    HardPtr<VanillaIterable> container; // WIBBLE SoftPtr
    Type_Iterator type;
    VanillaIterable::State state;
  public:
    VanillaIterator(IExecution& execution, VanillaIterable& container, const Type& rettype)
      : VanillaBase(execution.getAllocator()),
        container(&container),
        type(rettype),
        state(container.iterateStart(execution)) {
      assert(this->validate());
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->container->softVisitLinks(visitor);
    }
    virtual Type getRuntimeType() const override {
      return Type(&this->type);
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if ((parameters.getPositionalCount() > 0) || (parameters.getNamedCount() > 0)) {
        return execution.raise("Iterator function does not expect any parameters");
      }
      return this->container->iterateNext(execution, this->state);
    }
  };

  Value VanillaIterable::createIterator(IExecution& execution, const Type& elementType) {
    assert(elementType != nullptr);
    auto rettype = TypeFactory::createUnion(execution.getAllocator(), *Type::Void, *elementType);
    Object iterator(*allocator.create<VanillaIterator>(0, execution, *this, rettype));
    return ValueFactory::create(allocator, iterator);
  }

  class VanillaArray final : public VanillaIterable {
    VanillaArray(const VanillaArray&) = delete;
    VanillaArray& operator=(const VanillaArray&) = delete;
  private:
    VanillaValueArray array;
  public:
    VanillaArray(IAllocator& allocator, size_t length)
      : VanillaIterable(allocator),
        array(allocator, length) {
      assert(this->validate());
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->array.softVisitLinks(visitor);
    }
    virtual Type getRuntimeType() const override {
      return Vanilla::getArrayType();
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (property == "length") {
        return ValueFactory::createInt(this->allocator, Int(this->array.length()));
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
        if (!this->array.resize(size_t(length))) {
          return execution.raiseFormat("Unable to resize array to length ", length);
        }
        return Value::Void;
      }
      return execution.raiseFormat("Array does not have property: '", property, "'");
    }
    virtual Value getIndex(IExecution& execution, const Value& index) override {
      Int i;
      if (!index->getInt(i)) {
        return execution.raiseFormat("Array index was expected to be an 'int', not '", index->getRuntimeType().toString(), "'");
      }
      Value value;
      if (!this->array.get(size_t(i), value)) {
        return execution.raiseFormat("Invalid array index for an array with ", this->array.length(), " element(s): ", i);
      }
      return value;
    }
    virtual Value setIndex(IExecution& execution, const Value& index, const Value& value) override {
      Int i;
      if (!index->getInt(i)) {
        return execution.raiseFormat("Array index was expected to be an 'int', not '", index->getRuntimeType().toString(), "'");
      }
      if (!this->array.set(size_t(i), value)) {
        return execution.raiseFormat("Invalid array index for an array with ", this->array.length(), " element(s): ", i);
      }
      return Value::Void;
    }
    virtual Value iterate(IExecution& execution) override {
      return this->createIterator(execution, Type::AnyQ);
    }
    virtual State iterateStart(IExecution&) const override {
      // state.a is the next index into the array
      // state.b is the size of the array
      return { 0, Int(this->array.length()) };
    }
    virtual Value iterateNext(IExecution& execution, State& state) const override {
      // state.a is the next index into the array (negative when finished)
      // state.b is the size of the array
      if (state.a < 0) {
        return Value::Void;
      }
      auto length = Int(this->array.length());
      if (state.b != length) {
        state.a = -1;
        return execution.raise("Array iterator has detected that the underlying array has changed size");
      }
      Value value;
      if (!this->array.get(size_t(state.a), value)) {
        state.a = -1;
        return Value::Void;
      }
      state.a++;
      return value;
    }
    virtual void print(Printer& printer) const override {
      char separator = '[';
      this->array.print(printer, separator);
      if (separator == '[') {
        printer << separator;
      }
      printer << ']';
    }
  };

  class VanillaDictionary final : public VanillaIterable {
    VanillaDictionary(const VanillaDictionary&) = delete;
    VanillaDictionary& operator=(const VanillaDictionary&) = delete;
  protected:
    VanillaStringValueMap map;
  public:
    explicit VanillaDictionary(IAllocator& allocator)
      : VanillaIterable(allocator),
        map(allocator) {
      assert(this->validate());
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->map.softVisitLinks(visitor);
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
          result = execution.raise(thrown.readable());
        }
      }
      return result;
    }
    virtual Value iterate(IExecution& execution) override {
      return this->createIterator(execution, Vanilla::getKeyValueType());
    }
    virtual State iterateStart(IExecution&) const override {
      // state.a is the next index into the key vector
      // state.b is the size of the key vector
      return { 0, Int(this->map.length()) };
    }
    virtual Value iterateNext(IExecution& execution, State& state) const override {
      // state.a is the next index into the key vector
      // state.b is the size of the key vector
      if (state.a < 0) {
        return Value::Void;
      }
      auto length = Int(this->map.length());
      if (state.b != length) {
        state.a = -1;
        return execution.raise("Array iterator has detected that the underlying object has changed size");
      }
      String key;
      Value value;
      if (!this->map.getKeyValue(size_t(state.a), key, value)) {
        state.a = -1;
        return Value::Void;
      }
      state.a++;
      return execution.makeValue(VanillaFactory::createKeyValue(execution.getAllocator(), key, value));
    }
    virtual void print(Printer& printer) const override {
      char separator = '{';
      this->map.print(printer, separator);
      if (separator == '{') {
        printer << separator;
      }
      printer << '}';
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
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->map.softVisitLinks(visitor);
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
          result = execution.raise(thrown.readable());
        }
      }
      return result;
    }
  };

  class VanillaKeyValue final : public VanillaIterable {
    VanillaKeyValue(const VanillaKeyValue&) = delete;
    VanillaKeyValue& operator=(const VanillaKeyValue&) = delete;
  protected:
    String key;
    Value value; // TODO SoftPtr
  public:
    explicit VanillaKeyValue(IAllocator& allocator, const String& key, const Value& value)
      : VanillaIterable(allocator),
        key(key),
        value(value) {
      assert(this->validate());
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
    }
    virtual Type getRuntimeType() const override {
      return Vanilla::getKeyValueType();
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      if (property == "key") {
        return execution.makeValue(this->key);
      }
      if (property == "value") {
        return this->value;
      }
      return execution.raiseFormat("Key-value object does support property '", property, "'");
    }
    virtual Value setProperty(IExecution& execution, const String& property, const Value&) override {
      if ((property == "key") || (property == "value")) {
        return execution.raiseFormat("Key-value object does not support the modification of property '", property, "'");
      }
      return execution.raiseFormat("Key-value object does not support property '", property, "'");
    }
    virtual Value mutProperty(IExecution& execution, const String& property, Mutation, const Value&) override {
      if ((property == "key") || (property == "value")) {
        return execution.raiseFormat("Key-value object does not support the modification of property '", property, "'");
      }
      return execution.raiseFormat("Key-value object does not support property '", property, "'");
    }
    virtual Value iterate(IExecution& execution) override {
      return this->createIterator(execution, Vanilla::getKeyValueType());
    }
    virtual State iterateStart(IExecution&) const override {
      // state.a is the next index into the key vector
      return { 0, 0 };
    }
    virtual Value iterateNext(IExecution& execution, State& state) const override {
      // state.a is the next index into the key vector
      switch (state.a) {
      case 0:
        state.a = 1;
        return execution.makeValue(VanillaFactory::createKeyValue(execution.getAllocator(), "key", execution.makeValue(this->key)));
      case 1:
        state.a = -1;
        return execution.makeValue(VanillaFactory::createKeyValue(execution.getAllocator(), "value", this->value));
      }
      state.a = -1;
      return Value::Void;
    }
    virtual void print(Printer& printer) const override {
      printer << "{key:" << this->key << ",value:";
      printer.write(this->value);
      printer << '}';
    }
    virtual bool validate() const override {
      return VanillaBase::validate() && this->value.validate();
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
      // There are no soft links to visit
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
    virtual void print(Printer& printer) const override {
      printer << "<predicate>";
    }
  };

  class VanillaError : public VanillaObject {
    VanillaError(const VanillaError&) = delete;
    VanillaError& operator=(const VanillaError&) = delete;
  private:
    std::string readable;
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
      this->readable = sb.toUTF8();
    }
    virtual void print(Printer& printer) const override {
      // We print the raw value here
      printer << this->readable;
    }
  };
}

egg::ovum::Object egg::ovum::VanillaFactory::createArray(IAllocator& allocator, size_t length) {
  return ObjectFactory::create<VanillaArray>(allocator, length);
}

egg::ovum::Object egg::ovum::VanillaFactory::createDictionary(IAllocator& allocator) {
  return ObjectFactory::create<VanillaDictionary>(allocator);
}

egg::ovum::Object egg::ovum::VanillaFactory::createObject(IAllocator& allocator) {
  return ObjectFactory::create<VanillaObject>(allocator);
}

egg::ovum::Object egg::ovum::VanillaFactory::createKeyValue(IAllocator& allocator, const String& key, const Value& value) {
  return ObjectFactory::create<VanillaKeyValue>(allocator, key, value);
}

egg::ovum::Object egg::ovum::VanillaFactory::createError(IAllocator& allocator, const LocationSource& location, const String& message) {
  return ObjectFactory::create<VanillaError>(allocator, location, message);
}

egg::ovum::Object egg::ovum::VanillaFactory::createPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node) {
  return ObjectFactory::create<VanillaPredicate>(allocator, callback, node);
}
