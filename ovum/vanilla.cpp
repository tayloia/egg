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
      this->container.foreach([&](const Slot* slot) {
        printer << separator;
        if (slot != nullptr) {
          auto* value = slot->get();
          if (value != nullptr) {
            printer.write(Value(*value));
          }
        }
        separator = ',';
      });
    }
    void print(Printer& printer) const override {
      char separator = '[';
      this->print(printer, separator);
      if (separator == '[') {
        printer << separator;
      }
      printer << ']';
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
    void print(Printer& printer) const override {
      char separator = '{';
      this->print(printer, separator);
      if (separator == '{') {
        printer << separator;
      }
      printer << '}';
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
    VanillaIterable::State state;
    Type runtimeType;
  public:
    VanillaIterator(IExecution& execution, VanillaIterable& container, const Type& runtimeType)
      : VanillaBase(execution.getAllocator()),
        container(execution.getBasket(), &container),
        state(container.iterateStart(execution)),
        runtimeType(runtimeType) {
      execution.getBasket().take(*this);
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->container.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return this->runtimeType;
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
    auto runtimeType = execution.getTypeFactory().createIterator(elementType);
    Object iterator(*allocator.create<VanillaIterator>(0, execution, *this, runtimeType));
    return ValueFactory::create(allocator, iterator);
  }

  class VanillaArray final : public VanillaIterable {
    VanillaArray(const VanillaArray&) = delete;
    VanillaArray& operator=(const VanillaArray&) = delete;
  private:
    SoftPtr<VanillaValueArray> array;
    Type runtimeType;
  public:
    VanillaArray(IAllocator& allocator, IBasket& basket, size_t length, const Type& runtimeType)
      : VanillaIterable(allocator),
        array(basket, allocator.makeRaw<VanillaValueArray>(allocator, basket, length)),
        runtimeType(runtimeType) {
      basket.take(*this);
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->array.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return this->runtimeType;
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
    const IType& runtimeType;
    const IType& keyValueType;
  public:
    VanillaDictionary(IAllocator& allocator, IBasket& basket, const Type& runtimeType, const Type& keyValueType)
      : VanillaIterable(allocator),
        map(basket, allocator.makeRaw<VanillaStringValueMap>(allocator, basket)),
        runtimeType(*runtimeType),
        keyValueType(*keyValueType) {
      basket.take(*this);
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->map.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return Type(&this->runtimeType);
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
      return this->createIterator(execution, Type(&this->keyValueType));
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
        return execution.raise("Object iterator has detected that the underlying object has changed size");
      }
      String key;
      Value value;
      if (!this->map->getKeyValue(size_t(state.a), key, value)) {
        state.a = -1;
        return Value::Void;
      }
      state.a++;
      return execution.makeValue(VanillaFactory::createKeyValue(execution.getTypeFactory(), execution.getBasket(), key, value));
    }
    virtual void print(Printer& printer) const override {
      this->map->print(printer);
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
    Type type;
  public:
    explicit VanillaKeyValue(IAllocator& allocator, IBasket& basket, const String& key, const Value& value, const Type& type)
      : VanillaIterable(allocator),
        key(key),
        value(basket, allocator.makeRaw<Slot>(allocator, basket, value)),
        type(type) {
      basket.take(*this); // WOBBLE place in base class throughout?
      assert(this->validate());
    }
    virtual void softVisit(const Visitor& visitor) const override {
      this->value.visit(visitor);
    }
    virtual Type getRuntimeType() const override {
      return this->type;
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
      return this->createIterator(execution, this->type);
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
        return execution.makeValue(VanillaFactory::createKeyValue(execution.getTypeFactory(), execution.getBasket(), "key", execution.makeValue(this->key)));
      case 1:
        state.a = -1;
        return execution.makeValue(VanillaFactory::createKeyValue(execution.getTypeFactory(), execution.getBasket(), "value", this->getValue()));
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
      if (Bits::hasAllSet(this->modifiability, Modifiability::READ)) {
        auto* value = this->slot->get();
        if (value == nullptr) {
          return Value::Void;
        }
        return Value(*value);
      }
      return execution.raise(this->trailing("does not support reading via pointer operator '*'"));
    }
    virtual Value setPointee(IExecution& execution, const Value& value) override {
      if (Bits::hasAllSet(this->modifiability, Modifiability::WRITE)) {
        this->slot->set(value);
        return Value::Void;
      }
      return execution.raise(this->trailing("does not support writing via pointer operator '*'"));
    }
    virtual Value mutPointee(IExecution& execution, Mutation mutation, const Value& value) override {
      if (Bits::hasAllSet(this->modifiability, Modifiability::MUTATE)) {
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
    VanillaError(IAllocator& allocator, IBasket& basket, const LocationSource& location, const String& message, const Type& runtimeType, const Type& keyValueType)
      : VanillaDictionary(allocator, basket, runtimeType, keyValueType) {
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
    IVanillaPredicateCallback& callback; // Guaranteed to be long-lived
    Node node;
    Type runtimeType;
  public:
    VanillaPredicate(IAllocator& allocator, IBasket& basket, IVanillaPredicateCallback& callback, const INode& node, const Type& runtimeType)
      : VanillaBase(allocator),
      callback(callback),
      node(&node),
      runtimeType(runtimeType) {
      assert(this->node != nullptr);
      basket.take(*this);
    }
    virtual void softVisit(const Visitor&) const override {
      // There are no soft links to visit
    }
    virtual Type getRuntimeType() const override {
      return this->runtimeType;
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if ((parameters.getNamedCount() > 0) || (parameters.getPositionalCount() > 0)) {
        return execution.raise(this->trailing("does not support calling with parameters"));
      }
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

egg::ovum::Object egg::ovum::VanillaFactory::createArray(ITypeFactory& factory, IBasket& basket, size_t length) {
  auto runtimeType = factory.getVanillaArray();
  return createVanillaObject<VanillaArray>(factory.getAllocator(), basket, length, runtimeType);
}

egg::ovum::Object egg::ovum::VanillaFactory::createDictionary(ITypeFactory& factory, IBasket& basket) {
  auto runtimeType = factory.getVanillaDictionary();
  auto keyValueType = factory.getVanillaKeyValue();
  return createVanillaObject<VanillaDictionary>(factory.getAllocator(), basket, runtimeType, keyValueType);
}

egg::ovum::Object egg::ovum::VanillaFactory::createObject(ITypeFactory& factory, IBasket& basket) {
  return createVanillaObject<VanillaObject>(factory.getAllocator(), basket);
}

egg::ovum::Object egg::ovum::VanillaFactory::createKeyValue(ITypeFactory& factory, IBasket& basket, const String& key, const Value& value) {
  auto keyValueType = factory.getVanillaKeyValue();
  return createVanillaObject<VanillaKeyValue>(factory.getAllocator(), basket, key, value, keyValueType);
}

egg::ovum::Object egg::ovum::VanillaFactory::createError(ITypeFactory& factory, IBasket& basket, const LocationSource& location, const String& message) {
  auto runtimeType = factory.getVanillaError();
  auto keyValueType = factory.getVanillaKeyValue();
  return createVanillaObject<VanillaError>(factory.getAllocator(), basket, location, message, runtimeType, keyValueType);
}

egg::ovum::Object egg::ovum::VanillaFactory::createPredicate(ITypeFactory& factory, IBasket& basket, IVanillaPredicateCallback& callback, const INode& node) {
  auto runtimeType = factory.getVanillaPredicate();
  return createVanillaObject<VanillaPredicate>(factory.getAllocator(), basket, callback, node, runtimeType);
}

egg::ovum::Object egg::ovum::VanillaFactory::createPointer(ITypeFactory& factory, IBasket& basket, ISlot& slot, const Type& pointer, Modifiability modifiability) {
  return createVanillaObject<VanillaPointer>(factory.getAllocator(), basket, slot, pointer, modifiability);
}
