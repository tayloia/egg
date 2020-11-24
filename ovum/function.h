namespace egg::ovum {
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
    static bool validateParameter(const IParameters& parameters, size_t position, String& value, String& error);
    static size_t getMinimumCount(const IFunctionSignature& signature);
    static size_t getMaximumCount(const IFunctionSignature& signature);
  };
}
