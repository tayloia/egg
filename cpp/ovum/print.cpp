#include "ovum/ovum.h"
#include "ovum/utf.h"

#include <iomanip>

namespace {
  using namespace egg::ovum;

  constexpr Print::Options constructDefault() {
    Print::Options options{};
    options.quote = '\0';
    return options;
  }

  void escapeCodepoint(std::ostream& stream, char quote, char32_t codepoint) {
    switch (codepoint) {
    case '\0':
      stream << '\\' << '0';
      break;
    case '\\':
      stream << '\\' << '\\';
      break;
    case '\b':
      stream << '\\' << 'b';
      break;
    case '\f':
      stream << '\\' << 'f';
      break;
    case '\n':
      stream << '\\' << 'n';
      break;
    case '\r':
      stream << '\\' << 'r';
      break;
    case '\t':
      stream << '\\' << 't';
      break;
    case '\v':
      stream << '\\' << 'v';
      break;
    default:
      if (codepoint == char32_t(quote)) {
        stream << '\\' << quote;
      } else {
        stream << char(codepoint); // TODO proper unicode escapes
      }
    }
  }
}

const egg::ovum::Print::Options egg::ovum::Print::Options::DEFAULT = constructDefault();

void egg::ovum::Print::write(std::ostream& stream, std::nullptr_t, const Options&) {
  stream << "null";
}

void egg::ovum::Print::write(std::ostream& stream, bool value, const Options&) {
  stream << (value ? "true" : "false");
}

void egg::ovum::Print::write(std::ostream& stream, int32_t value, const Options&) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, int64_t value, const Options&) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, uint32_t value, const Options&) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, uint64_t value, const Options&) {
  stream << value;
}

void egg::ovum::Print::write(std::ostream& stream, float value, const Options&) {
  Arithmetic::print(stream, value);
}

void egg::ovum::Print::write(std::ostream& stream, double value, const Options&) {
  Arithmetic::print(stream, value);
}

void egg::ovum::Print::write(std::ostream& stream, const std::string& value, const Options& options) {
  if (options.quote == '\0') {
    stream << value;
  } else {
    stream << options.quote;
    Print::escape(stream, value, options.quote);
    stream << options.quote;
  }
}

void egg::ovum::Print::write(std::ostream& stream, const String& value, const Options& options) {
  Print::write(stream, value.toUTF8(), options);
}

void egg::ovum::Print::write(std::ostream& stream, const Type& value, const Options&) {
  if (value == nullptr) {
    stream << "null";
  } else {
    stream << value->toStringPrecedence().first;
  }
}

void egg::ovum::Print::write(std::ostream& stream, ValueFlags value, const Options&) {
  size_t found = 0;
#define EGG_OVUM_VARIANT_PRINT(name, text) if (Bits::hasAnySet(value, ValueFlags::name)) { if (++found > 1) { stream << '|'; } stream << text; }
  EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VARIANT_PRINT)
#undef EGG_OVUM_VARIANT_PRINT
  if (found == 0) {
    stream << '(' << int(value) << ')';
  }
}

void egg::ovum::Print::write(std::ostream& stream, ILogger::Source value, const Options&) {
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

void egg::ovum::Print::write(std::ostream& stream, ILogger::Severity value, const Options&) {
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

void egg::ovum::Print::write(std::ostream& stream, const Value& value, const Options& options) {
  if (options.quote != '\0') {
    Printer printer(stream, options);
    value->print(printer);
  } else {
    Options pretty{ options };
    pretty.quote = '"';
    Printer printer(stream, pretty);
    value->print(printer);
  }
}

void egg::ovum::Print::ascii(std::ostream& stream, const std::string& value, char quote) {
  UTF8 reader{ value, 0 };
  char32_t codepoint;
  while (reader.forward(codepoint)) {
    escapeCodepoint(stream, quote, codepoint);
  }
}

void egg::ovum::Print::escape(std::ostream& stream, const std::string& value, char quote) {
  // Through the magic of UTF8 we don't have to decode-recode all codeunits!
  for (auto codeunit : value) {
    if ((codeunit >= 32) && (codeunit <= 126)) {
      stream << codeunit;
    } else {
      escapeCodepoint(stream, quote, char32_t(codeunit));
    }
  }
}

egg::ovum::Printer& egg::ovum::Printer::operator<<(const String& value) {
  this->os << value.toUTF8();
  return *this;
}
