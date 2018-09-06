#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/dictionary.h"

#include <stdexcept>

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
    virtual void softVisitLinks(const Visitor& visitor) const override {
      for (auto& value : this->values) {
        value.softVisitLink(visitor);
      }
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
      if (property.equals("length")) {
        return Variant(Int(this->values.size()));
      }
      return execution.raiseFormat("Arrays do not support property '", property, "'");
    }
    virtual Variant setProperty(IExecution& execution, const String& property, const Variant& value) override {
      if (property.equals("length")) {
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
    virtual Variant iterate(IExecution& execution) override;
  };

  class VanillaKeyValue : public VanillaBase {
    VanillaKeyValue(const VanillaKeyValue&) = delete;
    VanillaKeyValue& operator=(const VanillaKeyValue&) = delete;
    friend class VanillaKeyValueIterator;
  private:
    Variant key;
    Variant value;
  public:
    explicit VanillaKeyValue(IAllocator& allocator, IBasket& basket, const Variant& key, const Variant& value)
      : VanillaBase(allocator),
        key(key),
        value(value) {
      basket.take(*this);
      this->key.soften(basket);
      this->value.soften(basket);
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->key.softVisitLink(visitor);
      this->value.softVisitLink(visitor);
    }
    virtual Variant toString() const override {
      return StringBuilder::concat("{key:", this->key.toString(), ",value:", this->value.toString(), '}');
    }
    virtual Type getRuntimeType() const override {
      return Type::Object;
    }
    virtual Variant call(IExecution& execution, const IParameters&) override {
      return execution.raiseFormat("Object cannot be called like a function");
    }
    virtual Variant getProperty(IExecution&, const String& property) override {
      if (property.equals("key")) {
        return this->key;
      }
      if (property.equals("value")) {
        return this->value;
      }
      return Variant::Void;
    }
    virtual Variant setProperty(IExecution& execution, const String& property, const Variant&) override {
      return execution.raiseFormat("Cannot set property '", property, "' of key-value object");
    }
    virtual Variant getIndex(IExecution& execution, const Variant& index) override {
      if (!index.isString()) {
        return execution.raiseFormat("Key-value object index was expected to be a 'string', not '", index.getRuntimeType().toString(), "'");
      }
      return this->getProperty(execution, index.getString());
    }
    virtual Variant setIndex(IExecution& execution, const Variant&, const Variant&) override {
      return execution.raiseFormat("Key-value object does not support setting properties with '[]'");
    }
    virtual Variant iterate(IExecution& execution) override;
  };

  class VanillaObject : public VanillaBase {
    VanillaObject(const VanillaObject&) = delete;
    VanillaObject& operator=(const VanillaObject&) = delete;
    friend class VanillaObjectIterator;
  protected:
    Dictionary<String, Variant> values;
  public:
    explicit VanillaObject(IAllocator& allocator)
      : VanillaBase(allocator) {
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->values.foreach([visitor](const String&, const Variant& value) {
        value.softVisitLink(visitor);
      });
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
    virtual Variant setIndex(IExecution& execution, const Variant& index, const Variant& value) override {
      if (!index.isString()) {
        return execution.raiseFormat("Object index was expected to be a 'string', not '", index.getRuntimeType().toString(), "'");
      }
      auto property = index.getString();
      return this->values.addOrUpdate(property, value);
    }
    virtual Variant iterate(IExecution& execution) override;
  };

  class VanillaException : public VanillaObject {
    VanillaException(const VanillaException&) = delete;
    VanillaException& operator=(const VanillaException&) = delete;
  public:
    VanillaException(IAllocator& allocator, const LocationSource& location, const String& message)
      : VanillaObject(allocator) {
      this->values.addUnique("message", message);
      if (!location.file.empty()) {
        this->values.addUnique("file", location.file);
      }
      if ((location.line > 0) || (location.column > 0)) {
        this->values.addUnique("line", Int(location.line));
      }
      if (location.column > 0) {
        this->values.addUnique("column", Int(location.column));
      }
    }
    virtual Variant toString() const override {
      StringBuilder sb;
      Variant part;
      if (this->values.tryGet("file", part)) {
        sb.add(part.toString());
      }
      if (this->values.tryGet("line", part)) {
        sb.add('(', part.toString());
        if (this->values.tryGet("column", part)) {
          sb.add(',', part.toString());
        }
        sb.add(')');
      }
      if (!sb.empty()) {
        sb.add(':', ' ');
      }
      if (this->values.tryGet("message", part)) {
        sb.add(part.toString());
      } else {
        sb.add("Unknown exception");
      }
      return sb.str();
    }
  };

  class VanillaIteratorBase : public VanillaBase {
    VanillaIteratorBase(const VanillaIteratorBase&) = delete;
    VanillaIteratorBase& operator=(const VanillaIteratorBase&) = delete;
  public:
    explicit VanillaIteratorBase(IAllocator& allocator)
      : VanillaBase(allocator) {
    }
    virtual Variant toString() const override {
      return "<iterator>"; // TODO
    }
    virtual Type getRuntimeType() const override {
      return Type::Object; // WIBBLE
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override {
      if ((parameters.getPositionalCount() > 0) || (parameters.getNamedCount() > 0)) {
        return execution.raiseFormat("Iterator functions do not accept parameters");
      }
      return this->next(execution);
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
    virtual Variant next(IExecution& execution) = 0;
  };

  class VanillaArrayIterator : public VanillaIteratorBase {
    VanillaArrayIterator(const VanillaArrayIterator&) = delete;
    VanillaArrayIterator& operator=(const VanillaArrayIterator&) = delete;
  private:
    SoftPtr<VanillaArray> container;
    size_t index;
  public:
    VanillaArrayIterator(IAllocator& allocator, VanillaArray& container)
      : VanillaIteratorBase(allocator),
        index(0) {
      this->container.set(*this, &container);
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->container.visit(visitor);
    }
    virtual Variant next(IExecution&) override {
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
  };

  class VanillaKeyValueIterator : public VanillaIteratorBase {
    VanillaKeyValueIterator(const VanillaKeyValueIterator&) = delete;
    VanillaKeyValueIterator& operator=(const VanillaKeyValueIterator&) = delete;
  private:
    SoftPtr<VanillaKeyValue> container;
    size_t index;
  public:
    VanillaKeyValueIterator(IAllocator& allocator, VanillaKeyValue& container)
      : VanillaIteratorBase(allocator),
        index(0) {
      this->container.set(*this, &container);
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->container.visit(visitor);
    }
    virtual Variant next(IExecution&) override {
      if (this->index == SIZE_MAX) {
        // Already completed
        return Variant::Void;
      }
      switch (this->index++) {
      case 0:
        return this->container->key;
      case 1:
        return this->container->value;
      }
      this->index = SIZE_MAX;
      return Variant::Void;
    }
  };

  class VanillaObjectIterator : public VanillaIteratorBase {
    VanillaObjectIterator(const VanillaObjectIterator&) = delete;
    VanillaObjectIterator& operator=(const VanillaObjectIterator&) = delete;
  private:
    SoftPtr<VanillaObject> container;
    size_t index;
  public:
    VanillaObjectIterator(IAllocator& allocator, VanillaObject& container)
      : VanillaIteratorBase(allocator),
      index(0) {
      this->container.set(*this, &container);
    }
    virtual void softVisitLinks(const Visitor& visitor) const override {
      this->container.visit(visitor);
    }
    virtual Variant next(IExecution& execution) override {
      if (this->index == SIZE_MAX) {
        // Already completed
        return Variant::Void;
      }
      auto& values = this->container->values;
      auto i = this->index++;
      if (i >= values.length()) {
        // Just completed
        this->index = SIZE_MAX;
        return Variant::Void;
      }
      auto kv = values.getByIndex(i);
      return ObjectFactory::createVanillaKeyValue(this->allocator, execution.getBasket(), kv.first, kv.second);
    }
  };
}

egg::ovum::Variant VanillaArray::iterate(IExecution&) {
  return VariantFactory::createObject<VanillaArrayIterator>(this->allocator, *this);
}

egg::ovum::Variant VanillaKeyValue::iterate(IExecution&) {
  return VariantFactory::createObject<VanillaKeyValueIterator>(this->allocator, *this);
}

egg::ovum::Variant VanillaObject::iterate(IExecution&) {
  return VariantFactory::createObject<VanillaObjectIterator>(this->allocator, *this);
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaArray(IAllocator& allocator, size_t size) {
  return ObjectFactory::create<VanillaArray>(allocator, size);
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaException(IAllocator& allocator, const LocationSource& location, const String& message) {
  return ObjectFactory::create<VanillaException>(allocator, location, message);
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaKeyValue(IAllocator& allocator, IBasket& basket, const Variant& key, const Variant& value) {
  return ObjectFactory::create<VanillaKeyValue>(allocator, basket, key, value);
}

egg::ovum::Object egg::ovum::ObjectFactory::createVanillaObject(IAllocator& allocator) {
  return ObjectFactory::create<VanillaObject>(allocator);
}
