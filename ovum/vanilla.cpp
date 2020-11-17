#include "ovum/ovum.h"
#include "ovum/dictionary.h"
#include "ovum/node.h"
#include "ovum/slot.h"
#include "ovum/print.h"
#include "ovum/vanilla.h"

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
    virtual bool get(const String& key, Value& value) const override {
      auto ref = this->container.getOrNull(key);
      if (ref != nullptr) {
        value = Value(*ref->get());
        return true;
      }
      return false;
    }
    virtual void add(const String& key, const Value& value) override {
      auto added = this->container.addOrUpdate(this->allocator, key, value);
      assert(added);
      (void)added;
    }
    virtual void set(const String& key, const Value& value) override {
      (void)this->container.addOrUpdate(this->allocator, key, value);
    }
    virtual HardPtr<ISlot> slot(const String&) const override {
      // WIBBLE
      return nullptr;
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

  class TypeBase : public NotSoftReferenceCounted<IType> {
    TypeBase(const TypeBase&) = delete;
    TypeBase& operator=(const TypeBase&) = delete;
  protected:
    const char* name;
  public:
    explicit TypeBase(const char* name) : name(name) {
      assert(this->name != nullptr);
    }
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual Assignability queryAssignable(const IType& rhs) const override {
      auto rflags = rhs.getFlags();
      if (rflags == ValueFlags::Object) {
        return Assignability::Always;
      }
      if (Bits::hasAnySet(rflags, ValueFlags::Object)) {
        return Assignability::Sometimes;
      }
      return Assignability::Never;
    }
    virtual const IFunctionSignature* queryCallable() const override {
      return &TypeFactory::OmniFunctionSignature;
    }
    virtual const IPropertySignature* queryDotable() const override {
      return nullptr;
    }
    virtual const IIteratorSignature* queryIterable() const override {
      return nullptr;
    }
    virtual const IPointerSignature* queryPointable() const override {
      return nullptr;
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair(this->name, 0);
    }
    virtual String describeValue() const override {
      return StringBuilder::concat("Value of type '", this->name, "'");
    }
  };

  class TypeArray : public TypeBase {
    TypeArray(const TypeArray&) = delete;
    TypeArray& operator=(const TypeArray&) = delete;
  private:
    class IndexSignature : public IIndexSignature {
    public:
      virtual Type getResultType() const override {
        return Type::AnyQ;
      }
      virtual Type getIndexType() const override {
        return Type::Int;
      }
      virtual Modifiability getModifiability() const override {
        return Modifiability::Read | Modifiability::Write | Modifiability::Mutate;
      }
    };
    class PropertySignature : public IPropertySignature {
    public:
      virtual Type getType(const String& property) const override {
        if (property == "length") {
          return Type::Int;
        }
        return nullptr;
      }
      virtual Modifiability getModifiability(const String& property) const override {
        if (property == "length") {
          return Modifiability::Read | Modifiability::Write | Modifiability::Mutate;
        }
        return Modifiability::None;
      }
      virtual String getName(size_t index) const override {
        if (index == 0) {
          return "length";
        }
        return {};
      }
    };
    static inline const IndexSignature indexSignature{};
    static inline const PropertySignature propertySignature{};
  public:
    TypeArray() : TypeBase("any?[]") {
    }
    virtual const IIndexSignature* queryIndexable() const override {
      return &indexSignature;
    }
    virtual const IPropertySignature* queryDotable() const override {
      return &propertySignature;
    }
  };
  const TypeArray typeArray{};

  class TypeDictionary : public TypeBase {
    TypeDictionary(const TypeDictionary&) = delete;
    TypeDictionary& operator=(const TypeDictionary&) = delete;
  private:
    class IndexSignature : public IIndexSignature {
    public:
      virtual Type getResultType() const override {
        return Type::AnyQ;
      }
      virtual Type getIndexType() const override {
        return Type::String;
      }
      virtual Modifiability getModifiability() const override {
        return Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
      }
    };
    class PropertySignature : public IPropertySignature {
    public:
      virtual Type getType(const String&) const override {
        return Type::AnyQ;
      }
      virtual Modifiability getModifiability(const String&) const override {
        return Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
      }
      virtual String getName(size_t) const override {
        return {};
      }
    };
    static inline const IndexSignature indexSignature{};
    static inline const PropertySignature propertySignature{};
  public:
    TypeDictionary() : TypeBase("any?{string}") {
    }
    virtual const IIndexSignature* queryIndexable() const override {
      return &indexSignature;
    }
    virtual const IPropertySignature* queryDotable() const override {
      return &propertySignature;
    }
  };
  const TypeDictionary typeDictionary{};

  class TypeObject : public TypeBase {
    TypeObject(const TypeObject&) = delete;
    TypeObject& operator=(const TypeObject&) = delete;
  private:
    class IndexSignature : public IIndexSignature {
    public:
      virtual Type getResultType() const override {
        return Type::AnyQ;
      }
      virtual Type getIndexType() const override {
        return Type::AnyQ;
      }
      virtual Modifiability getModifiability() const override {
        return Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
      }
    };
    class PropertySignature : public IPropertySignature {
    public:
      virtual Type getType(const String&) const override {
        return Type::AnyQ;
      }
      virtual Modifiability getModifiability(const String&) const override {
        return Modifiability::Read | Modifiability::Write | Modifiability::Mutate | Modifiability::Delete;
      }
      virtual String getName(size_t) const override {
        return {};
      }
    };
    static inline const IndexSignature indexSignature{};
    static inline const PropertySignature propertySignature{};
  public:
    TypeObject() : TypeBase("object") {
    }
    virtual const IIndexSignature* queryIndexable() const override {
      return &indexSignature;
    }
    virtual const IPropertySignature* queryDotable() const override {
      return &propertySignature;
    }
  };
  const TypeObject typeObject{};

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
      return Vanilla::Array;
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
      return Vanilla::Dictionary;
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
      return Vanilla::Object;
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
      return Vanilla::Object;
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

const egg::ovum::Type egg::ovum::Vanilla::Array{ &typeArray };
const egg::ovum::Type egg::ovum::Vanilla::Dictionary{ &typeDictionary };
const egg::ovum::Type egg::ovum::Vanilla::Object{ &typeObject };

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

egg::ovum::HardPtr<egg::ovum::IVanillaMap<egg::ovum::String, egg::ovum::Value>> egg::ovum::VanillaFactory::createStringValueMap(IAllocator& allocator) {
  return allocator.make<VanillaStringValueMap>();
}
