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
    virtual  egg::lang::ValueLegacy toString() const override {
      throw std::runtime_error("WIBBLE");
    }
    virtual ITypeRef getRuntimeType() const override {
      throw std::runtime_error("WIBBLE");
    }
    virtual  egg::lang::ValueLegacy call(IExecution&, const IParameters&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual  egg::lang::ValueLegacy getProperty(IExecution&, const String&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual  egg::lang::ValueLegacy setProperty(IExecution&, const String&, const  egg::lang::ValueLegacy&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual  egg::lang::ValueLegacy getIndex(IExecution&, const  egg::lang::ValueLegacy&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual  egg::lang::ValueLegacy setIndex(IExecution&, const  egg::lang::ValueLegacy&, const  egg::lang::ValueLegacy&) override {
      throw std::runtime_error("WIBBLE");
    }
    virtual  egg::lang::ValueLegacy iterate(IExecution&) override {
      throw std::runtime_error("WIBBLE");
    }
  };
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaObject(egg::ovum::IAllocator& allocator) {
  return Object(*allocator.create<VanillaObject>(0, allocator));
}
