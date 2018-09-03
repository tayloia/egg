#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/dictionary.h"

#include <stdexcept>

// WIBBLE
#if defined(_MSC_VER)
#define WIBBLE __FUNCSIG__
#else
#define WIBBLE + std::string(__PRETTY_FUNCTION__)
#endif

namespace {
  using namespace egg::ovum;

  class VanillaBase : public SoftReferenceCounted<IObject> {
    VanillaBase(const VanillaBase&) = delete;
    VanillaBase& operator=(const VanillaBase&) = delete;
  public:
    explicit VanillaBase(IAllocator& allocator)
      : SoftReferenceCounted(allocator) {
    }
  };

  class VanillaObject : public VanillaBase {
    VanillaObject(const VanillaObject&) = delete;
    VanillaObject& operator=(const VanillaObject&) = delete;
  private:
    Dictionary<String, Variant> values;
  public:
    explicit VanillaObject(IAllocator& allocator)
      : VanillaBase(allocator) {
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
      assert(false);
    }
    virtual Variant toString() const override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Type getRuntimeType() const override {
      return Type::Object;
    }
    virtual Variant call(IExecution&, const IParameters&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant getProperty(IExecution&, const String& property) override {
      return this->values.getOrDefault(property, Variant::Void);
    }
    virtual Variant setProperty(IExecution&, const String& property, const Variant& value) override {
      (void)this->values.addOrUpdate(property, value);
      return Variant::Void;
    }
    virtual Variant getIndex(IExecution& execution, const Variant& index) override {
      if (!index.isString()) {
        return execution.raiseFormat("Object index was expected to be a 'string', not '", index.getRuntimeType().toString(), "'");
      }
      auto property = index.getString();
      return this->values.getOrDefault(property, Variant::Void);
    }
    virtual Variant setIndex(IExecution&, const Variant&, const Variant&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant iterate(IExecution&) override {
      throw std::runtime_error("TODO: " WIBBLE);
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
    virtual Variant toString() const override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Type getRuntimeType() const override {
      return Type::Object; // WIBBLE
    }
    virtual Variant call(IExecution&, const IParameters&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant getProperty(IExecution& execution, const String& property) override {
      if (property == "length") {
        return Variant(Int(this->values.size()));
      }
      return execution.raiseFormat("Arrays do not support property '", property, "'");
    }
    virtual Variant setProperty(IExecution&, const String&, const Variant&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant getIndex(IExecution& execution, const Variant& index) override {
      if (!index.isInt()) {
        return execution.raiseFormat("Array index was expected to be an 'int', not '", index.getRuntimeType().toString(), "'");
      }
      auto i = index.getInt();
      auto u = size_t(i);
      if (u >= this->values.size()) {
        return execution.raiseFormat("Invalid array index for an array with ", this->values.size(), " element(s): ", i);
      }
      return this->values[u];
    }
    virtual Variant setIndex(IExecution& execution, const Variant& index, const Variant& value) override {
      if (!index.isInt()) {
        return execution.raiseFormat("Array index was expected to be an 'int', not '", index.getRuntimeType().toString(), "'");
      }
      auto i = index.getInt();
      auto u = size_t(i);
      if (u >= this->values.size()) {
        return execution.raiseFormat("Invalid array index for an array with ", this->values.size(), " element(s): ", i);
      }
      this->values[u] = value;
      return Variant::Void;
    }
    virtual Variant iterate(IExecution&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
  };

  class VanillaFunction : public VanillaBase {
    VanillaFunction(const VanillaFunction&) = delete;
    VanillaFunction& operator=(const VanillaFunction&) = delete;
  private:
    Type type;
    String name;
    Node block;
  public:
    explicit VanillaFunction(IAllocator& allocator, const Type& type, const String& name, const Node& block)
      : VanillaBase(allocator),
        type(type),
        name(name),
        block(block) {
      assert(type != nullptr);
      assert(!name.empty());
      assert(block != nullptr);
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
      assert(false);
    }
    virtual Variant toString() const override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Type getRuntimeType() const override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant call(IExecution&, const IParameters&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant getProperty(IExecution&, const String&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant setProperty(IExecution&, const String&, const Variant&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant getIndex(IExecution&, const Variant&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant setIndex(IExecution&, const Variant&, const Variant&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Variant iterate(IExecution&) override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
  };
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaArray(IAllocator& allocator, size_t size) {
  return Object(*allocator.create<VanillaArray>(0, allocator, size));
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaObject(IAllocator& allocator) {
  return Object(*allocator.create<VanillaObject>(0, allocator));
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaFunction(IAllocator& allocator, const Type& type, const String& name, const Node& block) {
  return Object(*allocator.create<VanillaFunction>(0, allocator, type, name, block));
}
