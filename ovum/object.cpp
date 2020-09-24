#include "ovum/ovum.h"
#include "ovum/vanilla.h"

namespace {
  using namespace egg::ovum;

  class ParametersNone : public IParameters {
  public:
    virtual size_t getPositionalCount() const override {
      return 0;
    }
    virtual Value getPositional(size_t) const override {
      return Value::Void;
    }
    virtual const LocationSource* getPositionalLocation(size_t) const override {
      return nullptr;
    }
    virtual size_t getNamedCount() const override {
      return 0;
    }
    virtual String getName(size_t) const override {
      return String();
    }
    virtual Value getNamed(const String&) const override {
      return Value::Void;
    }
    virtual const LocationSource* getNamedLocation(const String&) const override {
      return nullptr;
    }
  };
  const ParametersNone parametersNone{};
}

const egg::ovum::IParameters& egg::ovum::Object::ParametersNone{ parametersNone };

egg::ovum::Object egg::ovum::ObjectFactory::createEmpty(IAllocator& allocator) {
  // TODO optimize
  return VanillaFactory::createObject(allocator);
}
