#include "ovum/ovum.h"
#include "ovum/function.h"

namespace {
  using namespace egg::ovum;

  std::string ordinal(size_t position) {
    // TODO i18n
    static const char lookup[][16] = {
      "first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth", "ninth", "tenth", "eleventh", "twelfth"
    };
    if (position < 12) {
      return lookup[position];
    }
    return std::to_string(position + 1) + "th";
  }
}

bool egg::ovum::ParameterChecker::validateCount(const egg::ovum::IParameters& parameters, size_t minimum, size_t maximum, String& error) {
  assert(minimum < SIZE_MAX);
  assert(minimum <= maximum);
  auto actual = parameters.getPositionalCount();
  if ((minimum == maximum) && (actual != minimum)) {
    // Expected a fixed number of parameters
    switch (minimum) {
    case 0:
      error = StringBuilder::concat("expects no parameters, but received ", actual);
      break;
    case 1:
      error = StringBuilder::concat("expects one parameter, but received ", actual);
      break;
    default:
      error = StringBuilder::concat("expects ", minimum, " parameters, but received ", actual);
      break;
    }
    return false;
  }
  if (actual < minimum) {
    switch (minimum) {
    case 1:
      error = StringBuilder::concat("expects at least one parameter, but received ", actual);
      break;
    default:
      error = StringBuilder::concat("expects at least ", minimum, " parameters, but received ", actual);
      break;
    }
    return false;
  }
  if (actual > maximum) {
    switch (maximum) {
    case 0:
      error = StringBuilder::concat("expects no parameters, but received ", actual);
      break;
    case 1:
      error = StringBuilder::concat("expects no more than one parameter, but received ", actual);
      break;
    default:
      error = StringBuilder::concat("expects no more than ", minimum, " parameters, but received ", actual);
      break;
    }
    return false;
  }
  return true;
}

bool egg::ovum::ParameterChecker::validateParameter(const IParameters& parameters, size_t position, Value& value, String& error) {
  auto expected = parameters.getPositionalCount();
  if (position >= expected) {
    error = StringBuilder::concat("queried for undeclared ", ordinal(position), " parameter");
    return false;
  }
  value = parameters.getPositional(position);
  return true;
}

bool egg::ovum::ParameterChecker::validateParameter(const IParameters& parameters, size_t position, Int& value, String& error) {
  Value parameter;
  if (ParameterChecker::validateParameter(parameters, position, parameter, error)) {
    if (parameter->getInt(value)) {
      return true;
    }
    error = StringBuilder::concat("expected ", ordinal(position), " parameter to be of type 'int', but got '", parameter->getRuntimeType().toString(), "'");
  }
  return false;
}

bool egg::ovum::ParameterChecker::validateParameter(const IParameters& parameters, size_t position, String& value, String& error) {
  Value parameter;
  if (ParameterChecker::validateParameter(parameters, position, parameter, error)) {
    if (parameter->getString(value)) {
      return true;
    }
    error = StringBuilder::concat("expected ", ordinal(position), " parameter to be of type 'string', but got '", parameter->getRuntimeType().toString(), "'");
  }
  return false;
}

bool egg::ovum::ParameterChecker::validateParameter(const IParameters& parameters, size_t position, Object& value, String& error) {
  Value parameter;
  if (ParameterChecker::validateParameter(parameters, position, parameter, error)) {
    if (parameter->getObject(value)) {
      return true;
    }
    error = StringBuilder::concat("expected ", ordinal(position), " parameter to be an object, but got '", parameter->getRuntimeType().toString(), "'");
  }
  return false;
}

bool egg::ovum::ParameterChecker::validateParameter(const IParameters& parameters, size_t position, std::optional<Value>& value, String&) {
  // Cannot fail
  auto expected = parameters.getPositionalCount();
  if (position >= expected) {
    value = std::nullopt;
    return true;
  }
  value = parameters.getPositional(position);
  return true;
}

bool egg::ovum::ParameterChecker::validateParameter(const IParameters& parameters, size_t position, std::optional<Int>& value, String& error) {
  auto expected = parameters.getPositionalCount();
  if (position >= expected) {
    value = std::nullopt;
    return true;
  }
  auto parameter = parameters.getPositional(position);
  Int ivalue;
  if (parameter->getInt(ivalue)) {
    value = ivalue;
    return true;
  }
  error = StringBuilder::concat("expected optional ", ordinal(position), " parameter to be of type 'int', but got '", parameter->getRuntimeType().toString(), "'");
  return false;
}

bool egg::ovum::ParameterChecker::validateParameter(const IParameters& parameters, size_t position, std::optional<String>& value, String& error) {
  auto expected = parameters.getPositionalCount();
  if (position >= expected) {
    value = std::nullopt;
    return true;
  }
  auto parameter = parameters.getPositional(position);
  String svalue;
  if (parameter->getString(svalue)) {
    value = svalue;
    return true;
  }
  error = StringBuilder::concat("expected optional ", ordinal(position), " parameter to be of type 'string'");
  return false;
}

size_t egg::ovum::ParameterChecker::getMinimumCount(const IFunctionSignature& signature) {
  // The minimum acceptable number of parameters is the number of "required" parameters
  size_t minimum = 0;
  size_t count = signature.getParameterCount();
  for (size_t index = 0; index < count; ++index) {
    auto& parameter = signature.getParameter(index);
    if (Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Required)) {
      ++minimum;
    }
  }
  return minimum;
}

size_t egg::ovum::ParameterChecker::getMaximumCount(const IFunctionSignature& signature) {
  // The maximum acceptable number of parameters is either the number of parameters or "infinity" for variadic functions
  size_t maximum = signature.getParameterCount();
  for (size_t index = 0; index < maximum; ++index) {
    auto& parameter = signature.getParameter(index);
    if (Bits::hasAnySet(parameter.getFlags(), IFunctionSignatureParameter::Flags::Variadic)) {
      return SIZE_MAX;
    }
  }
  return maximum;
}
