#include "ovum/ovum.h"

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

const egg::ovum::IParameters& egg::ovum::Object::ParametersNone{ parametersNone };

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaObject(IAllocator& allocator) {
  // TODO
  return ObjectFactory::create<VanillaObject>(allocator);
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaArray(IAllocator& allocator, size_t fixed) {
  // TODO
  return ObjectFactory::create<VanillaArray>(allocator, fixed);
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaError(IAllocator& allocator, const LocationSource& location, const String& message) {
  // TODO add location and message;
  return ObjectFactory::create<VanillaError>(allocator, location, message);
}
