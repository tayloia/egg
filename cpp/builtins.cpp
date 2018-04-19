#include "yolk.h"

namespace {
  using namespace egg::lang;

  class BuiltinType : public egg::gc::NotReferenceCounted<IType> {
  private:
    String name;
  public:
    inline explicit BuiltinType(const std::string& name)
      : name(String::fromUTF8(name)) {
    }
    virtual String toString() const override {
      return this->name;
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

  class Builtin : public egg::gc::NotReferenceCounted<IObject> {
  private:
    String name;
    BuiltinType type;
  public:
    inline Builtin(const std::string& name, const std::string& type)
      : name(String::fromUTF8(name)), type(type) {
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
    inline Print() : Builtin("[print]", "void print(...)") {}
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
