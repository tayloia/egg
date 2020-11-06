#include "ovum/ovum.h"
#include "ovum/print.h"

#include <iomanip>

void egg::ovum::Print::write(std::ostream& stream, std::nullptr_t) {
  stream << "null";
}

void egg::ovum::Print::write(std::ostream& stream, bool value) {
  stream << (value ? "true" : "false");
}

void egg::ovum::Print::write(std::ostream& stream, int32_t value) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, int64_t value) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, uint32_t value) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, uint64_t value) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, float value) {
  Arithmetic::print(stream, value);
}

void egg::ovum::Print::write(std::ostream& stream, double value) {
  Arithmetic::print(stream, value);
}

void egg::ovum::Print::write(std::ostream& stream, const char* value) {
  stream << value; // ((value == nullptr) ? "null" : value);
}

void egg::ovum::Print::write(std::ostream& stream, const std::string& value) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, const String& value) {
  stream << value.toUTF8();
}

void egg::ovum::Print::write(std::ostream& stream, ValueFlags value) {
  size_t found = 0;
#define EGG_OVUM_VARIANT_PRINT(name, text) if (Bits::hasAnySet(value, ValueFlags::name)) { if (++found > 1) { stream << '|'; } stream << text; }
  EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VARIANT_PRINT)
#undef EGG_OVUM_VARIANT_PRINT
  if (found == 0) {
    stream << '(' << int(value) << ')';
  }
}

void egg::ovum::Print::write(std::ostream& stream, ILogger::Source value) {
  switch (value) {
  case ILogger::Source::Compiler:
    stream << "<COMPILER>";
    break;
  case ILogger::Source::Runtime:
    stream << "<RUNTIME>";
    break;
  case ILogger::Source::User:
    stream << "<USER>";
    break;
  default:
    stream << "<UNKNOWN:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, ILogger::Severity value) {
  switch (value) {
  case ILogger::Severity::None:
    stream << "<NONE>";
    break;
  case ILogger::Severity::Debug:
    stream << "<DEBUG>";
    break;
  case ILogger::Severity::Verbose:
    stream << "<VERBOSE>";
    break;
  case ILogger::Severity::Information:
    stream << "<INFORMATION>";
    break;
  case ILogger::Severity::Warning:
    stream << "<WARNING>";
    break;
  case ILogger::Severity::Error:
    stream << "<ERROR>";
    break;
  default:
    stream << "<UNKNOWN:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, const Value& value) {
  auto flags = value->getFlags();
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN();
  switch (flags) {
  case egg::ovum::ValueFlags::String: {
    egg::ovum::String s;
    if (value->getString(s)) {
      // TODO: escape
      stream << '"' << s.toUTF8() << '"';
      return;
    }
    break;
  }
  case egg::ovum::ValueFlags::Object: {
    egg::ovum::Object o;
    if (value->getObject(o)) {
      stream << o->toString().toUTF8();
      return;
    }
    break;
  }
  }
  EGG_WARNING_SUPPRESS_SWITCH_END();
  Print::write(stream, value->toString());
}
