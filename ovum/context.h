// WIBBLE retire
namespace egg::lang {
  class ValueLegacy;
}

namespace egg::ovum {
  class IPreparation {
  public:
    virtual ~IPreparation() {}
    virtual void raise(ILogger::Severity severity, const String& message) = 0;

    // Useful helpers
    template<typename... ARGS>
    void raiseWarning(ARGS... args);
    template<typename... ARGS>
    void raiseError(ARGS... args);
  };

  class IExecution {
  public:
    virtual ~IExecution() {}
    virtual IAllocator& getAllocator() const = 0;
    virtual egg::lang::ValueLegacy raise(const String& message) = 0;
    virtual egg::lang::ValueLegacy assertion(const egg::lang::ValueLegacy& predicate) = 0;
    virtual void print(const std::string& utf8) = 0;

    // Useful helper
    template<typename... ARGS>
    egg::lang::ValueLegacy raiseFormat(ARGS... args);
  };
}
