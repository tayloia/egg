#include "ovum/ovum.h"
#include "ovum/dictionary.h"
#include "ovum/node.h"
#include "ovum/slot.h"
#include "ovum/vanilla.h"

namespace {
  using namespace egg::ovum;

  class VanillaDictionary : public SoftReferenceCounted<IVanillaDictionary> {
    VanillaDictionary(const VanillaDictionary&) = delete;
    VanillaDictionary& operator=(const VanillaDictionary&) = delete;
  public:
    Dictionary<String, Slot> container;
  public:
    VanillaDictionary(IAllocator& allocator)
      : SoftReferenceCounted(allocator) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // WIBBLE
    }
    virtual bool get(const String&, Value&) const override {
      // WIBBLE
      return false;
    }
    virtual HardPtr<Slot> ref(const String&) const override {
      // WIBBLE
      return nullptr;
    }
    virtual void add(const String&, const Value&) override {
      // WIBBLE
    }
    virtual void set(const String&, const Value&) override {
      // WIBBLE
    }
  };

  // An vanilla function looks like this: 'any?(...any?[])'
  class FunctionSignature : public IFunctionSignature {
    FunctionSignature(const FunctionSignature&) = delete;
    FunctionSignature& operator=(const FunctionSignature&) = delete;
  private:
    class Parameter : public IFunctionSignatureParameter {
    public:
      virtual String getName() const override {
        return String();
      }
      virtual Type getType() const override {
        return Type::AnyQ;
      }
      virtual size_t getPosition() const override {
        return 0;
      }
      virtual Flags getFlags() const override {
        return Flags::Variadic;
      }
    };
    Parameter parameter;
  public:
    FunctionSignature() {}
    virtual String getFunctionName() const override {
      return String();
    }
    virtual Type getReturnType() const override {
      return Type::AnyQ;
    }
    virtual size_t getParameterCount() const override {
      return 1;
    }
    virtual const IFunctionSignatureParameter& getParameter(size_t) const override {
      return this->parameter;
    }
  };
  const FunctionSignature functionSignature{};

  class TypeBase : public NotReferenceCounted<IType> {
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
    virtual Type makeUnion(IAllocator& allocator, const IType& rhs) const override {
      return TypeFactory::createUnionJoin(allocator, *this, rhs);
    }
    virtual Error tryMutate(IAllocator& allocator, Slot& slot, Mutation mutation, const Value& value) const override {
      if (mutation == Mutation::Assign) {
        return this->tryAssign(allocator, slot, value);
      }
      return Error("Cannot modify values of type '", this->name, "'");
    }
    virtual Erratic<bool> queryAssignableAlways(const IType& rhs) const override {
      auto rflags = rhs.getFlags();
      if (rflags == ValueFlags::Object) {
        return true; // always
      }
      if (Bits::hasAnySet(rflags, ValueFlags::Object)) {
        return false; // sometimes
      }
      return Erratic<bool>::fail("Cannot assign values of type '", Type::toString(rhs), "' to targets of type '", this->name, "'");
    }
    virtual Erratic<Type> queryPropertyType(const String&) const {
      return Type::AnyQ;
    }
    virtual const IFunctionSignature* queryCallable() const override {
      return &functionSignature;
    }
    virtual Erratic<Type> queryIterable() const override {
      return Erratic<Type>::fail("WIBBLE: Object not iterable");
    }
    virtual Erratic<Type> queryPointeeType() const override {
      return Erratic<Type>::fail("WIBBLE: Object not pointable");
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::make_pair(this->name, 0);
    }
  protected:
    virtual Error tryAssign(IAllocator& allocator, Slot& slot, const Value& value) const = 0;
  };

  class TypeObject : public TypeBase {
    TypeObject(const TypeObject&) = delete;
    TypeObject& operator=(const TypeObject&) = delete;
  private:
    class IndexSignature : public IIndexSignature {
      virtual Type getResultType() const override {
        return Type::AnyQ;
      }
      virtual Type getIndexType() const override {
        return Type::AnyQ;
      }
    };
    static inline const IndexSignature indexSignature{};
  public:
    TypeObject() : TypeBase("object") {
    }
    virtual const IIndexSignature* queryIndexable() const override {
      return &indexSignature;
    }
  protected:
    virtual Error tryAssign(IAllocator&, Slot& slot, const Value& value) const override {
      slot.clobber(value);
      return {};
    }
  };
  const TypeObject typeObject{};

  class TypeArray : public TypeBase {
    TypeArray(const TypeArray&) = delete;
    TypeArray& operator=(const TypeArray&) = delete;
  private:
    class IndexSignature : public IIndexSignature {
      virtual Type getResultType() const override {
        return Type::AnyQ;
      }
      virtual Type getIndexType() const override {
        return Type::Int;
      }
    };
    static inline const IndexSignature indexSignature{};
  public:
    TypeArray() : TypeBase("any?[]") {
    }
    virtual const IIndexSignature* queryIndexable() const override {
      return &indexSignature;
    }
  protected:
    virtual Error tryAssign(IAllocator&, Slot& slot, const Value& value) const override {
      slot.clobber(value);
      return {};
    }
  };
  const TypeArray typeArray{};

  // WIBBLE WOBBLE move in class hierarchy
  class VanillaPredicate : public SoftReferenceCounted<IObject> {
    VanillaPredicate(const VanillaPredicate&) = delete;
    VanillaPredicate& operator=(const VanillaPredicate&) = delete;
  private:
    Vanilla::IPredicateCallback& callback; // Guaranteed to be long-lived
    Node node;
  public:
    VanillaPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node)
      : SoftReferenceCounted(allocator),
        callback(callback),
        node(&node) {
      assert(this->node != nullptr);
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // There are no soft link to visit
    }
    virtual String toString() const override {
      return "<predicate>";
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
    virtual String toString() const override {
      return StringBuilder::concat('<', this->getRuntimeType().toString(), '>');
    }
    virtual Value call(IExecution& execution, const IParameters&) override {
      return execution.raise(this->trailing("do not support calling with '()'"));
    }
    virtual Value getProperty(IExecution& execution, const String&) override {
      return execution.raise(this->trailing("do not support properties [get]"));
    }
    virtual Value setProperty(IExecution& execution, const String&, const Value&) override {
      return execution.raise(this->trailing("do not support properties [set]"));
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      return execution.raise(this->trailing("do not support indexing with '[]' [get]"));
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      return execution.raise(this->trailing("do not support indexing with '[]' [set]"));
    }
    virtual Value iterate(IExecution& execution) override {
      return execution.raise(this->trailing("do not support iteration"));
    }
  protected:
    virtual String trailing(const char* suffix) const {
      return StringBuilder::concat("Values of type '", this->getRuntimeType().toString(), "' ", suffix);
    }
  };

  class VanillaObject : public VanillaBase {
    VanillaObject(const VanillaObject&) = delete;
    VanillaObject& operator=(const VanillaObject&) = delete;
  protected:
    HardPtr<IVanillaDictionary> dictionary;
  public:
    explicit VanillaObject(IAllocator& allocator)
      : VanillaBase(allocator),
        dictionary(allocator.make<VanillaDictionary>()) {
      assert(this->validate());
    }
    virtual Type getRuntimeType() const override {
      return Vanilla::Object;
    }
    virtual Value getProperty(IExecution& execution, const String& property) override {
      Value value;
      if (!dictionary->get(property, value)) {
        return execution.raiseFormat("Object does not have property: '.", property, "'");
      }
      return value;
    }
  };

  class VanillaArray : public VanillaBase {
    VanillaArray(const VanillaArray&) = delete;
    VanillaObject& operator=(const VanillaArray&) = delete;
  private:
    std::vector<Slot> container;
  public:
    VanillaArray(IAllocator& allocator, size_t fixed)
      : VanillaBase(allocator) {
      this->container.reserve(fixed);
      for (size_t i = 0; i < fixed; ++i) {
        this->container.emplace_back(allocator);
      }
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
      return execution.raiseFormat("Array does not have property: '.", property, "'");
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
      return this->container[u].getValue();
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
      this->container[u].clobber(value);
      return Value::Void;
    }
    virtual String toString() const override {
      if (this->container.empty()) {
        return "[]";
      }
      StringBuilder sb;
      char separator = '[';
      for (auto& slot : this->container) {
        sb.add(separator);
        auto* value = slot.getReference();
        if (value != nullptr) {
          sb.add(value->toString());
        }
        separator = ',';
      }
      sb.add(']');
      return sb.str();
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
      this->dictionary->add("message", ValueFactory::create(allocator, message));
      StringBuilder sb;
      location.formatSourceString(sb);
      this->dictionary->add("location", ValueFactory::create(allocator, sb.toUTF8()));
      if (!sb.empty()) {
        sb.add(':', ' ');
      }
      sb.add(message);
      this->readable = sb.str();
    }
    virtual String toString() const override {
      return this->readable;
    }
  };
}

const egg::ovum::Type egg::ovum::Vanilla::Object{ &typeObject };
const egg::ovum::Type egg::ovum::Vanilla::Array{ &typeArray };
const egg::ovum::IFunctionSignature& egg::ovum::Vanilla::FunctionSignature{ functionSignature };

egg::ovum::Object egg::ovum::VanillaFactory::createObject(IAllocator& allocator) {
  // TODO
  return ObjectFactory::create<VanillaObject>(allocator);
}

egg::ovum::Object egg::ovum::VanillaFactory::createArray(IAllocator& allocator, size_t fixed) {
  return ObjectFactory::create<VanillaArray>(allocator, fixed);
}

egg::ovum::Object egg::ovum::VanillaFactory::createError(IAllocator& allocator, const LocationSource& location, const String& message) {
  return ObjectFactory::create<VanillaError>(allocator, location, message);
}

egg::ovum::Object egg::ovum::VanillaFactory::createPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node) {
  return ObjectFactory::create<VanillaPredicate>(allocator, callback, node);
}
