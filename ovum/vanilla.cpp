#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/vanilla.h"

namespace {
  using namespace egg::ovum;

  // An 'omni' function looks like this: 'any?(...any?[])'
  class VanillaFunctionSignature : public IFunctionSignature {
    VanillaFunctionSignature(const VanillaFunctionSignature&) = delete;
    VanillaFunctionSignature& operator=(const VanillaFunctionSignature&) = delete;
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
    VanillaFunctionSignature() = default;
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
  const VanillaFunctionSignature functionSignature{};

  class VanillaIndexSignature : public IIndexSignature {
    virtual Type getResultType() const override {
      return Type::AnyQ;
    }
    virtual Type getIndexType() const override {
      return Type::AnyQ;
    }
  };
  const VanillaIndexSignature indexSignature{};

  class TypeObject : public NotReferenceCounted<IType> {
    TypeObject(const TypeObject&) = delete;
    TypeObject& operator=(const TypeObject&) = delete;
  private:
  public:
    TypeObject() = default;
    virtual ValueFlags getFlags() const override {
      return ValueFlags::Object;
    }
    virtual Type makeUnion(IAllocator& allocator, const IType& rhs) const override {
      return TypeFactory::createUnionJoin(allocator, *this, rhs);
    }
    virtual Error tryMutate(IAllocator&, Slot&, Mutation, const Value&) const override {
      return Error("TypeObject::tryMutate not implemented");
    }
    virtual Erratic<bool> queryAssignableAlways(const IType& rhs) const override {
      auto rflags = rhs.getFlags();
      if (rflags == ValueFlags::Object) {
        return true; // always
      }
      if (Bits::hasAnySet(rflags, ValueFlags::Object)) {
        return false; // sometimes
      }
      return Erratic<bool>::fail("Cannot assign values of type '", Type::toString(rhs), "' to targets of type 'object'");
    }
    virtual Erratic<Type> queryPropertyType(const String&) const {
      return Type::AnyQ;
    }
    virtual const IFunctionSignature* queryCallable() const override {
      return &functionSignature;
    }
    virtual const IIndexSignature* queryIndexable() const override {
      return &indexSignature;
    }
    virtual Erratic<Type> queryIterable() const override {
      return Erratic<Type>::fail("WIBBLE: Object not iterable");
    }
    virtual Erratic<Type> queryPointeeType() const override {
      return Erratic<Type>::fail("WIBBLE: Object not pointable");
    }
    virtual std::pair<std::string, int> toStringPrecedence() const override {
      return std::pair<std::string, int>();
    }
  };
  const TypeObject typeObject{};

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
      // WIBBLE
      return "<object>";
    }
    virtual Type getRuntimeType() const override {
      // WIBBLE
      return Vanilla::Object;
    }
    virtual Value call(IExecution& execution, const IParameters&) override {
      // WIBBLE
      return execution.raiseFormat("WIBBLE: Objects do not support calling with '()'");
    }
    virtual Value getProperty(IExecution& execution, const String&) override {
      // WIBBLE
      return execution.raiseFormat("WIBBLE: Objects do not support properties");
    }
    virtual Value setProperty(IExecution& execution, const String&, const Value&) override {
      // WIBBLE
      return execution.raiseFormat("WIBBLE: Objects do not support properties");
    }
    virtual Value getIndex(IExecution& execution, const Value&) override {
      // WIBBLE
      return execution.raiseFormat("WIBBLE: Objects do not support indexing with '[]'");
    }
    virtual Value setIndex(IExecution& execution, const Value&, const Value&) override {
      // WIBBLE
      return execution.raiseFormat("WIBBLE: Objects do not support indexing with '[]'");
    }
    virtual Value iterate(IExecution& execution) override {
      // WIBBLE
      return execution.raiseFormat("WIBBLE: Objects do not support iteration");
    }
  };

  class VanillaObject : public VanillaBase {
    VanillaObject(const VanillaObject&) = delete;
    VanillaObject& operator=(const VanillaObject&) = delete;
  public:
    explicit VanillaObject(IAllocator& allocator)
      : VanillaBase(allocator) {
      assert(this->validate());
    }
    virtual String toString() const override {
      // WIBBLE
      return "<vanilla-object>";
    }
    void addProperty(const String&, const String&) {
      // WIBBLE
    }
  };

  class VanillaArray : public VanillaBase {
    VanillaArray(const VanillaArray&) = delete;
    VanillaObject& operator=(const VanillaArray&) = delete;
  public:
    VanillaArray(IAllocator& allocator, size_t)
      : VanillaBase(allocator) {
      assert(this->validate());
    }
    virtual String toString() const override {
      // WIBBLE
      return "<vanilla-array>";
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
      this->addProperty("message", message);
      StringBuilder sb;
      location.formatSourceString(sb);
      this->addProperty("location", sb.str());
      if (!sb.empty()) {
        sb.add(':', ' ');
      }
      sb.add(message);
      this->readable = sb.str();
    }
    virtual String toString() const override {
      // WIBBLE
      return this->readable;
    }
  };
}

const egg::ovum::Type egg::ovum::Vanilla::Object{ &typeObject };
const egg::ovum::Type egg::ovum::Vanilla::Array{ &typeObject }; // WIBBLE WOBBLE
const egg::ovum::IFunctionSignature& egg::ovum::Vanilla::FunctionSignature{ functionSignature };

egg::ovum::Object egg::ovum::VanillaFactory::createObject(IAllocator& allocator) {
  // TODO
  return ObjectFactory::create<VanillaObject>(allocator);
}

egg::ovum::Object egg::ovum::VanillaFactory::createArray(IAllocator& allocator, size_t fixed) {
  // TODO
  return ObjectFactory::create<VanillaArray>(allocator, fixed);
}

egg::ovum::Object egg::ovum::VanillaFactory::createError(IAllocator& allocator, const LocationSource& location, const String& message) {
  return ObjectFactory::create<VanillaError>(allocator, location, message);
}

egg::ovum::Object egg::ovum::VanillaFactory::createPredicate(IAllocator& allocator, Vanilla::IPredicateCallback& callback, const INode& node) {
  return ObjectFactory::create<VanillaPredicate>(allocator, callback, node);
}
