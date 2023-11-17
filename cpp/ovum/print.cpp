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

void egg::ovum::Print::write(std::ostream& stream, const ICollectable* value, const Options& options) {
  if (value == nullptr) {
    stream << "null";
  } else {
    // TODO change options?
    Printer printer{ stream, options };
    value->print(printer);
  }
}

void egg::ovum::Print::write(std::ostream& stream, const IObject* value, const Options& options) {
  Print::write(stream, static_cast<const ICollectable*>(value), options);
}

void egg::ovum::Print::write(std::ostream& stream, const IValue* value, const Options& options) {
  if (value == nullptr) {
    stream << "null";
  } else {
    Printer printer(stream, options);
    value->print(printer);
  }
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

void egg::ovum::Print::write(std::ostream& stream, UnaryOp value, const Options&) {
  switch (value) {
  case UnaryOp::Negate:
    stream << "-";
    break;
  case UnaryOp::BitwiseNot:
    stream << "~";
    break;
  case UnaryOp::LogicalNot:
    stream << "!";
    break;
  default:
    stream << "<UNKNOWN:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, BinaryOp value, const Options&) {
  switch (value) {
  case BinaryOp::Add:
    stream << "+";
    break;
  case BinaryOp::Subtract:
    stream << "-";
    break;
  case BinaryOp::Multiply:
    stream << "*";
    break;
  case BinaryOp::Divide:
    stream << "/";
    break;
  case BinaryOp::Remainder:
    stream << "%";
    break;
  case BinaryOp::LessThan:
    stream << "<";
    break;
  case BinaryOp::LessThanOrEqual:
    stream << "<=";
    break;
  case BinaryOp::Equal:
    stream << "==";
    break;
  case BinaryOp::NotEqual:
    stream << "!=";
    break;
  case BinaryOp::GreaterThanOrEqual:
    stream << ">=";
    break;
  case BinaryOp::GreaterThan:
    stream << ">";
    break;
  case BinaryOp::BitwiseAnd:
    stream << "&";
    break;
  case BinaryOp::BitwiseOr:
    stream << "|";
    break;
  case BinaryOp::BitwiseXor:
    stream << "^";
    break;
  case BinaryOp::ShiftLeft:
    stream << "<<";
    break;
  case BinaryOp::ShiftRight:
    stream << ">>";
    break;
  case BinaryOp::ShiftRightUnsigned:
    stream << ">>>";
    break;
  case BinaryOp::IfNull:
    stream << "??";
    break;
  case BinaryOp::IfFalse:
    stream << "&&";
    break;
  case BinaryOp::IfTrue:
    stream << "||";
    break;
  default:
    stream << "<UNKNOWN:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, TernaryOp value, const Options&) {
  switch (value) {
  case TernaryOp::IfThenElse:
    stream << "?:";
    break;
  default:
    stream << "<UNKNOWN:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, MutationOp value, const Options&) {
  switch (value) {
  case MutationOp::Assign:
    stream << "=";
    break;
  case MutationOp::Decrement:
    stream << "--";
    break;
  case MutationOp::Increment:
    stream << "++";
    break;
  case MutationOp::Add:
    stream << "+=";
    break;
  case MutationOp::Subtract:
    stream << "-=";
    break;
  case MutationOp::Multiply:
    stream << "*=";
    break;
  case MutationOp::Divide:
    stream << "/=";
    break;
  case MutationOp::Remainder:
    stream << "%=";
    break;
  case MutationOp::BitwiseAnd:
    stream << "&=";
    break;
  case MutationOp::BitwiseOr:
    stream << "|=";
    break;
  case MutationOp::BitwiseXor:
    stream << "^=";
    break;
  case MutationOp::ShiftLeft:
    stream << "<<=";
    break;
  case MutationOp::ShiftRight:
    stream << ">>=";
    break;
  case MutationOp::ShiftRightUnsigned:
    stream << ">>>=";
    break;
  case MutationOp::IfNull:
    stream << "??=";
    break;
  case MutationOp::IfFalse:
    stream << "&&=";
    break;
  case MutationOp::IfTrue:
    stream << "||=";
    break;
  case MutationOp::Noop:
    stream << "<NOOP>";
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

void egg::ovum::Print::write(std::ostream& stream, const HardObject& value, const Options& options) {
  Print::write(stream, const_cast<const IObject*>(value.get()), options);
}

void egg::ovum::Print::write(std::ostream& stream, const HardValue& value, const Options& options) {
  Print::write(stream, const_cast<const IValue*>(&value.get()), options);
}

void egg::ovum::Print::write(std::ostream& stream, const SoftObject& value, const Options& options) {
  Print::write(stream, const_cast<const IObject*>(value.get()), options);
}

void egg::ovum::Print::write(std::ostream& stream, const SoftKey& value, const Options& options) {
  Print::write(stream, const_cast<const IValue*>(&value.get()), options);
}

void egg::ovum::Print::write(std::ostream& stream, const SoftValue& value, const Options& options) {
  Print::write(stream, const_cast<const IValue*>(&value.get()), options);
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
