#include <optional>

namespace egg::ovum {
  class FunctionSignature {
  public:
    enum class Parts {
      ReturnType = 0x01,
      FunctionName = 0x02,
      ParameterList = 0x04,
      ParameterNames = 0x08,
      NoNames = ReturnType | ParameterList,
      All = ~0
    };
    static void print(Printer& printer, const IFunctionSignature& signature, Parts parts = Parts::All) {
      // TODO better formatting of named/variadic etc.
      if (Bits::hasAnySet(parts, Parts::ReturnType)) {
        // Use precedence zero to get any necessary parentheses
        printer << signature.getReturnType().toString(0);
      }
      if (Bits::hasAnySet(parts, Parts::FunctionName)) {
        auto name = signature.getFunctionName();
        if (!name.empty()) {
          printer << ' ' << name;
        }
      }
      if (Bits::hasAnySet(parts, Parts::ParameterList)) {
        printer << '(';
        auto n = signature.getParameterCount();
        for (size_t i = 0; i < n; ++i) {
          if (i > 0) {
            printer << ", ";
          }
          auto& parameter = signature.getParameter(i);
          assert(parameter.getPosition() != SIZE_MAX);
          if (Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
            printer << "...";
          } else {
            printer << parameter.getType().toString();
            if (Bits::hasAnySet(parts, Parts::ParameterNames)) {
              auto pname = parameter.getName();
              if (!pname.empty()) {
                printer << ' ' << pname;
              }
            }
            if (!Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Required)) {
              printer << " = null";
            }
          }
        }
        printer << ')';
      }
    }

    static String toString(const IFunctionSignature& signature, Parts parts = Parts::All) {
      StringBuilder sb;
      FunctionSignature::print(sb, signature, parts);
      return sb.str();
    }

    static std::pair<std::string, int> toStringPrecedence(const IFunctionSignature& signature) {
      StringBuilder sb;
      FunctionSignature::print(sb, signature, Parts::NoNames);
      return { sb.toUTF8(), 0 };
    }
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
