#include "yolk.h"

namespace {
  using namespace egg::lang;

  class BuiltinType : public IType {
  public:
    virtual IType* acquireHard() const override {
      // We don't actually reference count built-in types; they are never destroyed
      return const_cast<BuiltinType*>(this);
    }
    virtual void releaseHard() const override {
      // We don't actually reference count built-in types; they are never destroyed
    }
    virtual String toString() const override {
      return String::fromUTF8("WIBBLE" __FUNCTION__);
    }
    virtual bool hasSimpleType(Discriminator) const override {
      return false;
    }
    virtual Discriminator arithmeticTypes() const override {
      return Discriminator::None;
    }
    virtual egg::gc::HardRef<const IType> dereferencedType() const override {
      return egg::gc::HardRef<const IType>{nullptr};
    }
    virtual egg::gc::HardRef<const IType> nullableType(bool) const override {
      return egg::gc::HardRef<const IType>{nullptr};
    }
    virtual egg::gc::HardRef<const IType> unionWith(const IType&) const override {
      return egg::gc::HardRef<const IType>{nullptr};
    }
    virtual egg::gc::HardRef<const IType> unionWithSimple(Discriminator) const override {
      return egg::gc::HardRef<const IType>{nullptr};
    }
    virtual Value decantParameters(const IParameters&, Setter) const override {
      return Value::raise("WIBBLE" __FUNCTION__);
    }
    virtual bool canAssignFrom(const IType&, String& problem) const override {
      problem = String::fromUTF8("WIBBLE" __FUNCTION__);
      return false;
    }
    virtual bool tryAssignFrom(Value&, const Value&, String& problem) const override {
      problem = String::fromUTF8("WIBBLE" __FUNCTION__);
      return false;
    }
  };

  class Builtin : public IObject {
  private:
    BuiltinType type;
  public:
    virtual IObject* acquireHard() const override {
      // We don't actually reference count built-ins; they are never destroyed
      return const_cast<Builtin*>(this);
    }
    virtual void releaseHard() const override {
      // We don't actually reference count built-ins; they are never destroyed
    }
    virtual bool dispose() override {
      // We don't allow disposing of built-ins
      return false;
    }
    virtual Value getRuntimeType() const override {
      // Fetch the runtime type
      return Value(this->type);
    }
  };

  class Print : public Builtin {
  public:
    virtual Value toString() const override {
      return Value("[print]");
    }
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return Value::raise("print(): Named parameters are not supported");
      }
      auto n = parameters.getPositionalCount();
      std::string utf8;
      for (size_t i = 0; i < n; ++i) {
        auto parameter = parameters.getPositional(i);
        utf8 += parameter.toUTF8();
      }
      execution.print(utf8);
      return Value::Void;
    }
  };

  static Print BuiltinPrint;
}

egg::lang::Value egg::lang::Value::Print{ BuiltinPrint };
