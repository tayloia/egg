#include "ovum/ovum.h"

// WIBBLE
#include "yolk/macros.h"
#include "yolk/dictionaries.h"
#include "yolk/exceptions.h"
#include "yolk/files.h"
#include "yolk/lang.h"

namespace {
  using namespace egg::ovum;

  class VanillaBase : public SoftReferenceCounted<IObject> {
    VanillaBase(const VanillaBase&) = delete;
    VanillaBase& operator=(const VanillaBase&) = delete;
  public:
    using Value = egg::lang::ValueLegacy; // WIBBLE
    VanillaBase(IAllocator& allocator)
      : SoftReferenceCounted(allocator) {
    }
  };

  class VanillaObject : public VanillaBase {
    VanillaObject(const VanillaObject&) = delete;
    VanillaObject& operator=(const VanillaObject&) = delete;
  private:
  public:
    VanillaObject(IAllocator& allocator)
      : VanillaBase(allocator) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
      assert(false);
    }
    virtual Value toString() const override {
      throw std::runtime_error("WIBBLE");
    }
    virtual ITypeRef getRuntimeType() const override {
      throw std::runtime_error("WIBBLE");
    }
    virtual Value call(IExecution&, const egg::lang::IParameters&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual Value getProperty(IExecution&, const String&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual Value setProperty(IExecution&, const String&, const Value&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual Value getIndex(IExecution&, const Value&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual Value setIndex(IExecution&, const Value&, const Value&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual Value iterate(IExecution&) override {
      throw std::runtime_error("WIBBLE");
    }
  };
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaObject(egg::ovum::IAllocator& allocator) {
  return Object(*allocator.create<VanillaObject>(0, allocator));
}
