#include "ovum/ovum.h"

namespace {
  using namespace egg::ovum;

  class ObjectVanilla : public SoftReferenceCounted<IObject> {
    ObjectVanilla(const ObjectVanilla&) = delete;
    ObjectVanilla& operator=(const ObjectVanilla&) = delete;
  public:
    explicit ObjectVanilla(IAllocator& allocator) : SoftReferenceCounted(allocator) {}
    // Inherited via HardReferenceCounted
    virtual void softVisitLinks(const Visitor&) const override {
      // WIBBLE
    }
    virtual bool validate() const override {
      return true;
    }
    virtual Value toString() const override {
      // WIBBLE
      return ValueFactory::createString(this->allocator, "<object>");
    }
    virtual Type getRuntimeType() const override {
      // WIBBLE
      return Type::Object;
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
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaObject(IAllocator& allocator) {
  return ObjectFactory::create<ObjectVanilla>(allocator);
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaArray(IAllocator& allocator, size_t) {
  return ObjectFactory::create<ObjectVanilla>(allocator);
}
