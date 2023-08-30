#include <optional>

namespace egg::ovum {
  class FunctionSignatureParameter : public IFunctionSignatureParameter {
  private:
    String name; // may be empty
    Type type;
    size_t position; // may be SIZE_MAX
    Flags flags;
  public:
    FunctionSignatureParameter(const String& name, const Type& type, size_t position, Flags flags)
      : name(name), type(type), position(position), flags(flags) {
    }
    virtual String getName() const override {
      return this->name;
    }
    virtual Type getType() const override {
      return this->type;
    }
    virtual size_t getPosition() const override {
      return this->position;
    }
    virtual Flags getFlags() const override {
      return this->flags;
    }
  };

  class FunctionSignature {
  public:
    // Helpers
    enum class Parts {
      ReturnType = 0x01,
      FunctionName = 0x02,
      ParameterList = 0x04,
      ParameterNames = 0x08,
      NoNames = ReturnType | ParameterList,
      All = ReturnType | FunctionName | ParameterList | ParameterNames
    };
    static void print(Printer& printer, const IFunctionSignature& signature, Parts parts = Parts::All);
    static String toString(const IFunctionSignature& signature, Parts parts = Parts::All);
    static std::pair<std::string, int> toStringPrecedence(const IFunctionSignature& signature);
  };

  class ParameterChecker {
    ParameterChecker(const ParameterChecker&) = delete;
    ParameterChecker& operator=(const ParameterChecker&) = delete;
  public:
    const IParameters* parameters;
    const IFunctionSignature* signature;
    String error;
  public:
    ParameterChecker()
      : parameters(nullptr),
        signature(nullptr) {
    }
    ParameterChecker(const IParameters& parameters, const IFunctionSignature* signature = nullptr)
      : parameters(&parameters),
        signature(signature) {
    }
    bool validateCount() {
      assert(this->parameters != nullptr);
      assert(this->signature != nullptr);
      auto minimum = getMinimumCount(*this->signature);
      auto maximum = getMaximumCount(*this->signature);
      return ParameterChecker::validateCount(*this->parameters, minimum, maximum, this->error);
    }
    bool validateCount(size_t expected) {
      assert(this->parameters != nullptr);
      return ParameterChecker::validateCount(*this->parameters, expected, expected, this->error);
    }
    bool validateCount(size_t minimum, size_t maximum) {
      assert(this->parameters != nullptr);
      return ParameterChecker::validateCount(*this->parameters, minimum, maximum, this->error);
    }
    static bool validateCount(const IParameters& parameters, size_t minimum, size_t maximum, String& error);
    template<typename T>
    bool validateParameter(size_t position, T& value) {
      assert(this->parameters != nullptr);
      return this->validateParameter(*this->parameters, position, value, this->error);
    }
    static bool validateParameter(const IParameters& parameters, size_t position, Value& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, Bool& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, Int& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, Float& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, String& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, Object& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, std::optional<Value>& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, std::optional<Bool>& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, std::optional<Int>& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, std::optional<Float>& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, std::optional<String>& value, String& error);
    static bool validateParameter(const IParameters& parameters, size_t position, std::optional<Object>& value, String& error);
    static size_t getMinimumCount(const IFunctionSignature& signature);
    static size_t getMaximumCount(const IFunctionSignature& signature);
  };
}
