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
      if (this->values.empty()) {
        return "{}";
      }
      StringBuilder sb;
      char separator = '{';
      this->values.foreach([&](const String& key, const Variant& value) {
        sb.add(separator, key, ':', value.toString());
        separator = ',';
      });
      sb.add('}');
      return sb.str();
    }
    virtual Type getRuntimeType() const override {
      return Type::Object;
    }
    virtual Variant call(IExecution& execution, const IParameters&) override {
      return execution.raiseFormat("Object cannot be called like a function");
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
    friend class VanillaArrayIterator;
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
      if (this->values.empty()) {
        return "[]";
      }
      StringBuilder sb;
      char separator = '[';
      for (auto& value : this->values) {
        sb.add(separator, value.toString());
        separator = ',';
      }
      sb.add(']');
      return sb.str();
    }
    virtual Type getRuntimeType() const override {
      return Type::Object; // WIBBLE
    }
    virtual Variant call(IExecution& execution, const IParameters&) override {
      return execution.raiseFormat("Arrays cannot be called like functions");
    }
    virtual Variant getProperty(IExecution& execution, const String& property) override {
      if (property == "length") {
        return Variant(Int(this->values.size()));
      }
      return execution.raiseFormat("Arrays do not support property '", property, "'");
    }
    virtual Variant setProperty(IExecution& execution, const String& property, const Variant& value) override {
      if (property == "length") {
        if (!value.isInt()) {
          return execution.raiseFormat("Array length was expected to be set to an 'int', not '", value.getRuntimeType().toString(), "'");
        }
        auto n = value.getInt();
        if ((n < 0) || (n >= 0x7FFFFFFF)) {
          return execution.raiseFormat("Invalid array length: ", n);
        }
        this->values.resize(size_t(n), egg::ovum::Variant::Null);
        return Variant::Void;
      }
      return execution.raiseFormat("Arrays do not support property '", property, "'");
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
      return this->values[u].direct();
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
      auto& e = this->values[u];
      e = value;
      e.soften(*this->softGetBasket());
      return Variant::Void;
    }
    virtual Variant iterate(IExecution&) override;
  };

  class VanillaArrayIterator : public VanillaBase {
    VanillaArrayIterator(const VanillaArrayIterator&) = delete;
    VanillaArrayIterator& operator=(const VanillaArrayIterator&) = delete;
  private:
    SoftPtr<VanillaArray> container;
    size_t index;
  public:
    VanillaArrayIterator(IAllocator& allocator, VanillaArray& container)
      : VanillaBase(allocator),
        index(0) {
      this->container.set(*this, &container);
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->container.visit(visitor);
    }
    virtual Variant toString() const override {
      return "<iterator>"; // TODO
    }
    virtual Type getRuntimeType() const override {
      return Type::Object; // WIBBLE
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override {
      if ((parameters.getPositionalCount() > 0) || (parameters.getNamedCount() > 0)) {
        return execution.raiseFormat("Array iterator functions do not accept parameters");
      }
      if (this->index == SIZE_MAX) {
        // Already completed
        return Variant::Void;
      }
      auto& values = this->container->values;
      auto i = this->index++;
      if (i >= values.size()) {
        // Just completed
        this->index = SIZE_MAX;
        return Variant::Void;
      }
      return values[i];
    }
    virtual Variant getProperty(IExecution& execution, const String& property) override {
      return execution.raiseFormat("Iterators do not support properties such as '", property, "'");
    }
    virtual Variant setProperty(IExecution& execution, const String& property, const Variant&) override {
      return execution.raiseFormat("Iterators do not support properties such as '", property, "'");
    }
    virtual Variant getIndex(IExecution& execution, const Variant&) override {
      return execution.raiseFormat("Iterators do not support indexing with '[]'");
    }
    virtual Variant setIndex(IExecution& execution, const Variant&, const Variant&) override {
      return execution.raiseFormat("Iterators do not support indexing with '[]'");
    }
    virtual Variant iterate(IExecution&) override {
      return Object(*this);
    }
  };
}

egg::ovum::Variant VanillaArray::iterate(IExecution&) {
  return VariantFactory::createObject<VanillaArrayIterator>(this->allocator, *this);
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaArray(IAllocator& allocator, size_t size) {
  return Object(*allocator.create<VanillaArray>(0, allocator, size));
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaObject(IAllocator& allocator) {
  return Object(*allocator.create<VanillaObject>(0, allocator));
}
