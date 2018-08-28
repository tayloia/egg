#include "ovum/ovum.h"

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

  class VanillaArray : public VanillaBase {
    VanillaArray(const VanillaArray&) = delete;
    VanillaArray& operator=(const VanillaArray&) = delete;
  private:
    std::vector<Variant> values;
  public:
    VanillaArray(IAllocator& allocator, size_t size)
      : VanillaBase(allocator) {
      this->values.resize(size);
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
      assert(false);
    }
    virtual  egg::ovum::Variant toString() const override {
      throw std::runtime_error("TODO");
    }
    virtual Type getRuntimeType() const override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant call(IExecution&, const IParameters&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant getProperty(IExecution&, const String&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant setProperty(IExecution&, const String&, const  egg::ovum::Variant&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant getIndex(IExecution&, const  egg::ovum::Variant&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant setIndex(IExecution&, const  egg::ovum::Variant&, const  egg::ovum::Variant&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant iterate(IExecution&) override {
      throw std::runtime_error("TODO");
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
    virtual  egg::ovum::Variant toString() const override {
      throw std::runtime_error("TODO");
    }
    virtual Type getRuntimeType() const override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant call(IExecution&, const IParameters&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant getProperty(IExecution&, const String&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant setProperty(IExecution&, const String&, const  egg::ovum::Variant&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant getIndex(IExecution&, const  egg::ovum::Variant&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant setIndex(IExecution&, const  egg::ovum::Variant&, const  egg::ovum::Variant&) override {
      throw std::runtime_error("TODO");
    }
    virtual  egg::ovum::Variant iterate(IExecution&) override {
      throw std::runtime_error("TODO");
    }
  };
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaArray(egg::ovum::IAllocator& allocator, size_t size) {
  return Object(*allocator.create<VanillaArray>(0, allocator, size));
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaObject(egg::ovum::IAllocator& allocator) {
  return Object(*allocator.create<VanillaObject>(0, allocator));
}
