#include "ovum/ovum.h"
#include "ovum/node.h"
#include "ovum/module.h"
#include "ovum/program.h"

// WIBBLE
#if defined(_MSC_VER)
#define WIBBLE __FUNCSIG__
#else
#define WIBBLE + std::string(__PRETTY_FUNCTION__)
#endif

namespace {
  using namespace egg::ovum;

  class BuiltinBase : public SoftReferenceCounted<IObject> {
    BuiltinBase(const BuiltinBase&) = delete;
    BuiltinBase& operator=(const BuiltinBase&) = delete;
  protected:
    String name;
  public:
    explicit BuiltinBase(IAllocator& allocator)
      : SoftReferenceCounted(allocator) {
    }
    virtual Variant getProperty(IExecution& execution, const String& property) override {
      return this->raiseBuiltin(execution, "does not support properties such as '.", property, "'");
    }
    virtual Variant setProperty(IExecution& execution, const String& property, const Variant&) override {
      return this->raiseBuiltin(execution, "does not support properties such as '.", property, "'");
    }
    virtual Variant getIndex(IExecution& execution, const Variant&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Variant setIndex(IExecution& execution, const Variant&, const Variant&) override {
      return this->raiseBuiltin(execution, "does not support indexing with '[]'");
    }
    virtual Variant iterate(IExecution& execution) override {
      return this->raiseBuiltin(execution, "does not support iteration");
    }
    template<typename... ARGS>
    Variant raiseBuiltin(IExecution& execution, ARGS&&... args) {
      auto message = StringBuilder::concat("Built-in '", this->name, "' ", std::forward<ARGS>(args)...);
      return execution.raise(message);
    }
  };

  class BuiltinObject : public BuiltinBase {
    BuiltinObject(const BuiltinObject&) = delete;
    BuiltinObject& operator=(const BuiltinObject&) = delete;
  private:
  public:
    explicit BuiltinObject(IAllocator& allocator)
      : BuiltinBase(allocator) {
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
  };

  class BuiltinFunction : public BuiltinBase {
    BuiltinFunction(const BuiltinFunction&) = delete;
    BuiltinFunction& operator=(const BuiltinFunction&) = delete;
  private:
    Type type;
    String name;
  public:
    BuiltinFunction(IAllocator& allocator, const Type& type, const String& name)
      : BuiltinBase(allocator),
      type(type),
      name(name) {
      assert(type != nullptr);
      assert(!name.empty());
    }
    virtual void softVisitLinks(const Visitor&) const override {
      // TODO
      assert(false);
    }
    virtual Variant toString() const override {
      throw std::runtime_error("TODO: " WIBBLE);
    }
    virtual Type getRuntimeType() const override {
      return this->type;
    }
  };

  class BuiltinAssert : public BuiltinFunction {
    BuiltinAssert(const BuiltinAssert&) = delete;
    BuiltinAssert& operator=(const BuiltinAssert&) = delete;
  public:
    explicit BuiltinAssert(IAllocator& allocator)
      : BuiltinFunction(allocator, Type::Object, StringFactory::fromUTF8(allocator, "assert", 6)) {
    }
    virtual Variant call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return this->raiseBuiltin(execution, "does not accept named parameters");
      }
      if (parameters.getPositionalCount() != 1) {
        return this->raiseBuiltin(execution, "accepts only 1 parameter, not ", parameters.getPositionalCount());
      }
      return execution.assertion(parameters.getPositional(0).direct());
    }
  };
}

egg::ovum::Variant egg::ovum::VariantFactory::createBuiltinAssert(IAllocator& allocator) {
  Object object(*allocator.create<BuiltinAssert>(0, allocator));
  return Variant(object);
}
