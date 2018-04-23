#include "yolk.h"

namespace {
  using namespace egg::lang;

  class BuiltinType : public egg::gc::NotReferenceCounted<IType> {
  private:
    String name;
    String signature;
  public:
    inline BuiltinType(const String& name, const String& signature)
      : name(name), signature(signature) {
    }
    virtual String toString() const override {
      return this->signature;
    }
    virtual Value canAlwaysAssignFrom(IExecution& execution, const IType&) const override {
      return execution.raiseFormat("Cannot re-assign built-in function '", this->name, "'");
    }
    virtual Value promoteAssignment(IExecution& execution, const Value&) const override {
      return execution.raiseFormat("Cannot re-assign built-in function '", this->name, "'");
    }
  };

  class Builtin : public egg::gc::NotReferenceCounted<IObject> {
  private:
    String name;
    BuiltinType type;
  public:
    inline Builtin(const String& name, const String& signature)
      : name(name), type(name, signature) {
    }
    virtual bool dispose() override {
      // We don't allow disposing of built-ins
      return false;
    }
    virtual Value toString() const override {
      return Value(this->name);
    }
    virtual Value getRuntimeType() const override {
      // Fetch the runtime type
      return Value(this->type);
    }
  };

  class Print : public Builtin {
  public:
    inline Print() : Builtin(String::fromUTF8("print"), String::fromUTF8("void print(...)")) {}
    virtual Value call(IExecution& execution, const IParameters& parameters) override {
      if (parameters.getNamedCount() > 0) {
        return execution.raiseFormat("print(): Named parameters are not supported");
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
