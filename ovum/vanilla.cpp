#include "ovum/ovum.h"
#include "ovum/dictionary.h"
#include "ovum/node.h"
#include "ovum/slot.h"
#include "ovum/builtin.h"
#include "ovum/vanilla.h"

namespace {
  using namespace egg::ovum;

  template<typename T, typename... ARGS>
  static Object createVanillaObject(IAllocator& allocator, ARGS&&... args) {
    return Object(*allocator.makeRaw<T>(allocator, std::forward<ARGS>(args)...));
  }

  class Type_Base : public NotHardReferenceCounted<IType> {
    Type_Base(const Type_Base&) = delete;
    Type_Base& operator=(const Type_Base&) = delete;
  protected:
    static const Modifiability ReadWriteMutate = Modifiability::Read | Modifiability::Write | Modifiability::Mutate;
    static const Modifiability ReadWriteMutateDelete = Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
  private:
    ObjectShape shape;
  public:
    Type_Base(const IFunctionSignature* callable,
              const IPropertySignature* dotable,
              const IIndexSignature* indexable,
              const IIteratorSignature* iterable,
              const IPointerSignature* pointable)
      : shape{ callable, dotable, indexable, iterable, pointable } {
    }
    virtual ValueFlags getPrimitiveFlags() const override {
      return ValueFlags::None;
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

  class Type_Iterator final : public Type_Base {
    Type_Iterator(const Type_Iterator&) = delete;
    Type_Iterator& operator=(const Type_Iterator&) = delete;
  private:
    TypeBuilderCallable callable;
  public:
    explicit Type_Iterator(const Type& rettype)
      : Type_Base(&callable, nullptr, nullptr, nullptr, nullptr),
        callable(rettype, nullptr, {}) {
      assert(rettype.hasPrimitiveFlag(ValueFlags::Void));
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      auto retval = this->callable.getReturnType()->toStringPrecedence();
      assert(retval.second == 2);
      return { '(' + retval.first + ')' + '(' + ')', 0 };
    }
    virtual String describeValue() const override {
      // TODO i18n
      return "Iterator";
    }
  };

  class Type_Array final : public Type_Base {
    Type_Array(const Type_Array&) = delete;
    Type_Array& operator=(const Type_Array&) = delete;
  private:
    TypeBuilderProperties dotable;
    TypeBuilderIndexable indexable;
    TypeBuilderIterable iterable;
  public:
    Type_Array()
      : Type_Base(nullptr, &dotable, &indexable, &iterable, nullptr),
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

  class Type_DictionaryBase : public Type_Base {
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
      : Type_Base(nullptr, &dotable, &indexable, &iterable, nullptr),
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
      return { "any?[string]", 0 };
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

  template<typename... ARGS>
  Value raiseString(IAllocator& allocator, ARGS&&... args) {
    // TODO possibly conflate to ValueFlags::Throw|ValueFlags::String
    auto message = StringBuilder::concat(std::forward<ARGS>(args)...);
    auto value = ValueFactory::createString(allocator, message);
    return ValueFactory::createFlowControl(allocator, ValueFlags::Throw, value);
  }

  Value augment(IExecution& execution, const Value& value) {
    // Reverse of 'raiseString'
    if (value->getFlags() == (ValueFlags::Throw | ValueFlags::String)) {
      // The operation failed with a thrown string: augment it with the location
      Value thrown;
      if (value->getInner(thrown)) {
        return execution.raise(thrown.readable());
      }
    }
    return value;
  }

  class VanillaValueArray final : public SoftReferenceCounted<IVanillaArray<Value>> {
    VanillaValueArray(const VanillaValueArray&) = delete;
    VanillaValueArray& operator=(const VanillaValueArray&) = delete;
  private:
    using Container = SlotArray;
    Container container;
  public:
    VanillaValueArray(IAllocator& allocator, IBasket& basket, size_t length)
      : SoftReferenceCounted(allocator),
        container(length) {
      basket.take(*this);
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->container.softVisit(visitor);
    }
    virtual bool get(size_t index, Value& value) const override {
      auto* slot = this->container.get(index);
      if (slot != nullptr) {
        value = slot->value(Value::Void);
        return true;
      }
      return false;
    }
    virtual bool set(size_t index, const Value& value) override {
      assert(this->basket != nullptr);
      return this->container.set(this->allocator, *this->basket, index, value) != nullptr;
    }
    virtual Value mut(size_t index, Mutation mutation, const Value& value) override {
      auto slot = this->container.get(index);
      if (slot == nullptr) {
        return raiseString(this->allocator, "Array does not have element ", index);
      }
      Value before;
      auto retval = slot->mutate(Type::AnyQ, mutation, value, before);
      if (retval != Type::Assignment::Success) {
        return raiseString(this->allocator, "Cannot mutate array value: WOBBLE");
      }
      return before;
    }
    virtual size_t length() const override {
      return this->container.length();
    }
    virtual bool resize(size_t size) override {
      assert(this->basket != nullptr);
      this->container.resize(this->allocator, *this->basket, size, Value::Null);
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
    VanillaStringValueMap(IAllocator& allocator, IBasket& basket)
      : SoftReferenceCounted(allocator) {
      basket.take(*this);
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->container.visit(visitor);
    }
    virtual bool add(const String& key, const Value& value) override {
      assert(this->basket != nullptr);
      return this->container.add(this->allocator, *this->basket, key, value);
    }
    virtual bool get(const String& key, Value& value) const override {
      auto slot = this->container.find(key);
      if (slot != nullptr) {
        value = slot->value(Value::Void);
        return true;
      }
      return false;
    }
    virtual void set(const String& key, const Value& value) override {
      assert(this->basket != nullptr);
      (void)this->container.set(this->allocator, *this->basket, key, value);
    }
    virtual Value mut(const String& key, Mutation mutation, const Value& value) override {
      auto slot = this->container.find(key);
      if (slot == nullptr) {
        return raiseString(this->allocator, "Object does not have a property named '", key, "'");
      }
      Value before;
      auto retval = slot->mutate(Type::AnyQ, mutation, value, before);
      if (retval != Type::Assignment::Success) {
        return raiseString(this->allocator, "Cannot mutate map value: WOBBLE");
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
      auto slot = this->container.lookup(index, key);
      if (slot != nullptr) {
        value = slot->value(Value::Void);
        return true;
      }
      return false;
    }
    void print(Printer& printer, char& separator) const {
      this->container.foreach([&](const String& key, const Slot& slot) {
        printer << separator << key.toUTF8() << ':';
        auto* value = slot.get();
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
    SoftPtr<VanillaIterable> container;
    Type_Iterator type;
    VanillaIterable::State state;
  public:
    VanillaIterator(IExecution& execution, VanillaIterable& container, const Type& rettype)
      : VanillaBase(execution.getAllocator()),
        container(execution.getBasket(), &container),
        type(rettype),
        state(container.iterateStart(execution)) {
      execution.getBasket().take(*this);
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->container.visit(visitor);
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
    auto rettype = execution.getTypeFactory().addVoid(elementType);
    Object iterator(*allocator.create<VanillaIterator>(0, execution, *this, rettype));
    return ValueFactory::create(allocator, iterator);
  }

  class VanillaArray final : public VanillaIterable {
    VanillaArray(const VanillaArray&) = delete;
    VanillaArray& operator=(const VanillaArray&) = delete;
  private:
    SoftPtr<VanillaValueArray> array;
  public:
    VanillaArray(IAllocator& allocator, IBasket& basket, size_t length)
      : VanillaIterable(allocator),
        array(basket, allocator.makeRaw<VanillaValueArray>(allocator, basket, length)) {
      basket.take(*this);
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->array.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return Vanilla::getArrayType();
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (property == "length") {
        return ValueFactory::createInt(this->allocator, Int(this->array->length()));
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
        if (!this->array->resize(size_t(length))) {
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
      if (!this->array->get(size_t(i), value)) {
        return execution.raiseFormat("Invalid array index for an array with ", this->array->length(), " element(s): ", i);
      }
      return value;
    }
    virtual Value setIndex(IExecution& execution, const Value& index, const Value& value) override {
      Int i;
      if (!index->getInt(i)) {
        return execution.raiseFormat("Array index was expected to be an 'int', not '", index->getRuntimeType().toString(), "'");
      }
      if (!this->array->set(size_t(i), value)) {
        return execution.raiseFormat("Invalid array index for an array with ", this->array->length(), " element(s): ", i);
      }
      return Value::Void;
    }
    virtual Value mutIndex(IExecution& execution, const Value& index, Mutation mutation, const Value& value) override {
      Int i;
      if (!index->getInt(i)) {
        return execution.raiseFormat("Array index was expected to be an 'int', not '", index->getRuntimeType().toString(), "'");
      }
      return augment(execution, this->array->mut(size_t(i), mutation, value));
    }
    virtual Value iterate(IExecution& execution) override {
      return this->createIterator(execution, Type::AnyQ);
    }
    virtual State iterateStart(IExecution&) const override {
      // state.a is the next index into the array
      // state.b is the size of the array
      return { 0, Int(this->array->length()) };
    }
    virtual Value iterateNext(IExecution& execution, State& state) const override {
      // state.a is the next index into the array (negative when finished)
      // state.b is the size of the array
      if (state.a < 0) {
        return Value::Void;
      }
      auto length = Int(this->array->length());
      if (state.b != length) {
        state.a = -1;
        return execution.raise("Array iterator has detected that the underlying array has changed size");
      }
      Value value;
      if (!this->array->get(size_t(state.a), value)) {
        state.a = -1;
        return Value::Void;
      }
      state.a++;
      return value;
    }
    virtual void print(Printer& printer) const override {
      char separator = '[';
      this->array->print(printer, separator);
      if (separator == '[') {
        printer << separator;
      }
      printer << ']';
    }
  };

  class VanillaDictionary : public VanillaIterable {
    VanillaDictionary(const VanillaDictionary&) = delete;
    VanillaDictionary& operator=(const VanillaDictionary&) = delete;
  protected:
    SoftPtr<VanillaStringValueMap> map;
  public:
    VanillaDictionary(IAllocator& allocator, IBasket& basket)
      : VanillaIterable(allocator),
        map(basket, allocator.makeRaw<VanillaStringValueMap>(allocator, basket)) {
      basket.take(*this);
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->map.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return Vanilla::getDictionaryType();
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (!this->map->get(property, value)) {
        return execution.raiseFormat("Object does not have property: '", property, "'");
      }
      return value;
    }
    virtual Value setProperty(IExecution&, const String& property, const Value& value) override {
      this->map->set(property, value);
      return Value::Void;
    }
    virtual Value mutProperty(IExecution& execution, const String& key, Mutation mutation, const Value& value) override {
      return augment(execution, this->map->mut(key, mutation, value));
    }
    virtual Value getIndex(IExecution& execution, const Value& index) override {
      String property;
      if (!index->getString(property)) {
        return execution.raiseFormat("Object index was expected to be a 'string', not '", index->getRuntimeType().toString(), "'");
      }
      Value value;
      if (!this->map->get(property, value)) {
        return execution.raiseFormat("Object does not have property index '", property, "'");
      }
      return value;
    }
    virtual Value setIndex(IExecution& execution, const Value& index, const Value& value) override {
      String property;
      if (!index->getString(property)) {
        return execution.raiseFormat("Object index was expected to be a 'string', not '", index->getRuntimeType().toString(), "'");
      }
      this->map->set(property, value);
      return Value::Void;
    }
    virtual Value mutIndex(IExecution& execution, const Value& index, Mutation mutation, const Value& value) override {
      String property;
      if (!index->getString(property)) {
        return execution.raiseFormat("Object index was expected to be a 'string', not '", index->getRuntimeType().toString(), "'");
      }
      return augment(execution, this->map->mut(property, mutation, value));
    }
    virtual Value iterate(IExecution& execution) override {
      return this->createIterator(execution, Vanilla::getKeyValueType());
    }
    virtual State iterateStart(IExecution&) const override {
      // state.a is the next index into the key vector
      // state.b is the size of the key vector
      return { 0, Int(this->map->length()) };
    }
    virtual Value iterateNext(IExecution& execution, State& state) const override {
      // state.a is the next index into the key vector
      // state.b is the size of the key vector
      if (state.a < 0) {
        return Value::Void;
      }
      auto length = Int(this->map->length());
      if (state.b != length) {
        state.a = -1;
        return execution.raise("Dictionary iterator has detected that the underlying object has changed size");
      }
      String key;
      Value value;
      if (!this->map->getKeyValue(size_t(state.a), key, value)) {
        state.a = -1;
        return Value::Void;
      }
      state.a++;
      return execution.makeValue(VanillaFactory::createKeyValue(execution.getAllocator(), execution.getBasket(), key, value));
    }
    virtual void print(Printer& printer) const override {
      char separator = '{';
      this->map->print(printer, separator);
      if (separator == '{') {
        printer << separator;
      }
      printer << '}';
    }
  };

  class VanillaObject final : public VanillaBase {
    VanillaObject(const VanillaObject&) = delete;
    VanillaObject& operator=(const VanillaObject&) = delete;
  private:
    SoftPtr<VanillaStringValueMap> map; // TODO: VanillaValueValueMap
  public:
    VanillaObject(IAllocator& allocator, IBasket& basket)
      : VanillaBase(allocator),
        map(basket, allocator.makeRaw<VanillaStringValueMap>(allocator, basket)) {
      basket.take(*this);
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->map.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return Type::Object;
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (!this->map->get(property, value)) {
        return execution.raiseFormat("Object does not have property '", property, "'");
      }
      return value;
    }
    virtual Value setProperty(IExecution&, const String& property, const Value& value) override {
      this->map->set(property, value);
      return Value::Void;
    }
    virtual Value mutProperty(IExecution& execution, const String& key, Mutation mutation, const Value& value) override {
      return augment(execution, this->map->mut(key, mutation, value));
    }
  };

  class VanillaKeyValue final : public VanillaIterable {
    VanillaKeyValue(const VanillaKeyValue&) = delete;
    VanillaKeyValue& operator=(const VanillaKeyValue&) = delete;
  protected:
    String key;
    SoftPtr<Slot> value;
  public:
    explicit VanillaKeyValue(IAllocator& allocator, IBasket& basket, const String& key, const Value& value)
      : VanillaIterable(allocator),
        key(key),
        value(basket, allocator.makeRaw<Slot>(allocator, basket, value)) {
      basket.take(*this); // WOBBLE place in base class throughout?
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->value.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return Vanilla::getKeyValueType();
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      if (property == "key") {
        return execution.makeValue(this->key);
      }
      if (property == "value") {
        return this->getValue();
      }
      return execution.raiseFormat("Key-value object does not support property '", property, "'");
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
        return execution.makeValue(VanillaFactory::createKeyValue(execution.getAllocator(), execution.getBasket(), "key", execution.makeValue(this->key)));
      case 1:
        state.a = -1;
        return execution.makeValue(VanillaFactory::createKeyValue(execution.getAllocator(), execution.getBasket(), "value", this->getValue()));
      }
      state.a = -1;
      return Value::Void;
    }
    virtual void print(Printer& printer) const override {
      printer << "{key:" << this->key << ",value:";
      printer.write(this->getValue());
      printer << '}';
    }
    virtual bool validate() const override {
      return VanillaBase::validate() && (this->value != nullptr) && this->value->validate();
    }
  private:
    Value getValue() const {
      return Value(*this->value->get());
    }
  };

  class VanillaPointer : public VanillaBase {
    VanillaPointer(const VanillaPointer&) = delete;
    VanillaPointer& operator=(const VanillaPointer&) = delete;
  private:
    SoftPtr<ISlot> slot;
    Type type;
    Modifiability modifiability;
  public:
    VanillaPointer(IAllocator& allocator, IBasket& basket, const ISlot& slot, const Type& type, Modifiability modifiability)
      : VanillaBase(allocator),
        slot(),
        type(type),
        modifiability(modifiability) {
      assert(type != nullptr);
      this->slot.set(basket, &slot);
      basket.take(*this);
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->slot.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return this->type;
    }
    virtual Value getPointee(IExecution& execution) override {
      if (Bits::hasAllSet(this->modifiability, Modifiability::Read)) {
        auto* value = this->slot->get();
        if (value == nullptr) {
          return Value::Void;
        }
        return Value(*value);
      }
      return execution.raise(this->trailing("does not support reading via pointer operator '*'"));
    }
    virtual Value setPointee(IExecution& execution, const Value& value) override {
      if (Bits::hasAllSet(this->modifiability, Modifiability::Write)) {
        this->slot->set(value);
        return Value::Void;
      }
      return execution.raise(this->trailing("does not support writing via pointer operator '*'"));
    }
    virtual Value mutPointee(IExecution& execution, Mutation mutation, const Value& value) override {
      if (Bits::hasAllSet(this->modifiability, Modifiability::Mutate)) {
        Value before;
        switch (Slot::mutate(*this->slot, this->allocator, Type::AnyQ, mutation, value, before)) {
        case Type::Assignment::Success:
          break;
        case Type::Assignment::Uninitialized:
        case Type::Assignment::Incompatible:
        case Type::Assignment::BadIntToFloat:
          return execution.raise(this->trailing("failed to mutate via pointer operator '*' [WOBBLE]"));
        }
        return before;
      }
      return execution.raise(this->trailing("does not support mutating via pointer operator '*'"));
    }
  };

  class VanillaError : public VanillaDictionary {
    VanillaError(const VanillaError&) = delete;
    VanillaError& operator=(const VanillaError&) = delete;
  private:
    std::string readable;
  public:
    VanillaError(IAllocator& allocator, IBasket& basket, const LocationSource& location, const String& message)
      : VanillaDictionary(allocator, basket) {
      assert(this->validate());
      this->map->add("message", ValueFactory::create(allocator, message));
      if (!location.file.empty()) {
        this->map->add("file", ValueFactory::create(allocator, location.file));
      }
      if (location.line > 0) {
        this->map->add("line", ValueFactory::create(allocator, Int(location.line)));
      }
      if (location.column > 0) {
        this->map->add("column", ValueFactory::create(allocator, Int(location.column)));
      }
      StringBuilder sb;
      if (location.printSource(sb)) {
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

  class VanillaPredicate : public VanillaBase {
    VanillaPredicate(const VanillaPredicate&) = delete;
    VanillaPredicate& operator=(const VanillaPredicate&) = delete;
  private:
    Vanilla::IPredicateCallback& callback; // Guaranteed to be long-lived
    Node node;
  public:
    VanillaPredicate(IAllocator& allocator, IBasket& basket, Vanilla::IPredicateCallback& callback, const INode& node)
      : VanillaBase(allocator),
      callback(callback),
      node(&node) {
      assert(this->node != nullptr);
      basket.take(*this);
    }
    virtual void softVisit(const Visitor&) const override {
      // There are no soft links to visit
    }
    virtual Type getRuntimeType() const override {
      return Type::Object; // TODO
    }
    virtual Value call(IExecution&, const IParameters& parameters) override {
      assert(parameters.getNamedCount() == 0);
      assert(parameters.getPositionalCount() == 0);
      (void)parameters; // ignore the parameters
      return this->callback.predicateCallback(*this->node);
    }
    virtual void print(Printer& printer) const override {
      printer << "<predicate>";
    }
  protected:
    virtual String trailing(const char* suffix) const {
      return StringBuilder::concat("Internal runtime error: Predicate ", suffix);
    }
  };
}

egg::ovum::Object egg::ovum::VanillaFactory::createArray(IAllocator& allocator, IBasket& basket, size_t length) {
  return createVanillaObject<VanillaArray>(allocator, basket, length);
}

egg::ovum::Object egg::ovum::VanillaFactory::createDictionary(IAllocator& allocator, IBasket& basket) {
  return createVanillaObject<VanillaDictionary>(allocator, basket);
}

egg::ovum::Object egg::ovum::VanillaFactory::createObject(IAllocator& allocator, IBasket& basket) {
  return createVanillaObject<VanillaObject>(allocator, basket);
}

egg::ovum::Object egg::ovum::VanillaFactory::createKeyValue(IAllocator& allocator, IBasket& basket, const String& key, const Value& value) {
  return createVanillaObject<VanillaKeyValue>(allocator, basket, key, value);
}

egg::ovum::Object egg::ovum::VanillaFactory::createError(IAllocator& allocator, IBasket& basket, const LocationSource& location, const String& message) {
  return createVanillaObject<VanillaError>(allocator, basket, location, message);
}

egg::ovum::Object egg::ovum::VanillaFactory::createPredicate(IAllocator& allocator, IBasket& basket, Vanilla::IPredicateCallback& callback, const INode& node) {
  return createVanillaObject<VanillaPredicate>(allocator, basket, callback, node);
}

egg::ovum::Object egg::ovum::VanillaFactory::createPointer(IAllocator& allocator, IBasket& basket, ISlot& slot, const Type& pointer, Modifiability modifiability) {
  return createVanillaObject<VanillaPointer>(allocator, basket, slot, pointer, modifiability);
}
