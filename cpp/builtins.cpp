#include "yolk.h"

namespace {
  using namespace egg::lang;

  class Builtin : public IObject {
  public:
    virtual IObject* acquire() override {
      // We don't actually reference count built-ins; they are never destroyed
      return this;
    }
    virtual void release() override {
      // We don't actually reference count built-ins; they are never destroyed
    }
    virtual bool dispose() override {
      // We don't allow disposing of built-ins
      return false;
    }
  };

  class Print : public Builtin {
  public:
    virtual Value toString() override {
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
        utf8 += parameter.toString();
      }
      execution.print(utf8);
      return Value::Void;
    }
  };

  static Print BuiltinPrint;
}

egg::lang::Value egg::lang::Value::Print{ BuiltinPrint };
