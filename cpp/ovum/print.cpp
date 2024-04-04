#include "ovum/ovum.h"
#include "ovum/utf.h"

#include <iomanip>

namespace {
  using namespace egg::ovum;

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

const egg::ovum::Print::Options egg::ovum::Print::Options::DEFAULT = {};

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

void egg::ovum::Print::write(std::ostream& stream, ValueFlags value, const Options&) {
  size_t found = 0;
#define EGG_OVUM_VARIANT_PRINT(name, text) if (Bits::hasAnySet(value, ValueFlags::name)) { if (++found > 1) { stream << '|'; } stream << text; }
  EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VARIANT_PRINT)
#undef EGG_OVUM_VARIANT_PRINT
  if (found == 0) {
    stream << "<VALUEFLAGS:" << int(value) << '>';
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
  case ILogger::Source::Command:
    stream << "<COMMAND>";
    break;
  case ILogger::Source::User:
    stream << "<USER>";
    break;
  default:
    stream << "<UNKNOWN:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, const SourceRange& value, const Options&) {
  // See https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-diagnostic-format-for-tasks
  // Also https://sarifweb.azurewebsites.net/
  if (!value.empty()) {
    // (line
    stream << '(' << value.begin.line;
    if (value.begin.column > 0) {
      // (line,column
      stream << ',' << value.begin.column;
      if ((value.end.line > 0) && (value.end.column > 0)) {
        if (value.end.line > value.begin.line) {
          // (line,column,line,column
          stream << ',' << value.end.line << ',' << value.end.column;
        } else if (value.end.column > (value.begin.column + 1)) {
          // (line,column-column
          stream << '-' << (value.end.column - 1);
        }
      }
    }
    stream << ')';
  }
}

void egg::ovum::Print::write(std::ostream& stream, ValueUnaryOp value, const Options&) {
  switch (value) {
  case ValueUnaryOp::Negate:
    stream << "-";
    break;
  case ValueUnaryOp::BitwiseNot:
    stream << "~";
    break;
  case ValueUnaryOp::LogicalNot:
    stream << "!";
    break;
  default:
    stream << "<VALUEUNARYOP:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, ValueBinaryOp value, const Options&) {
  switch (value) {
  case ValueBinaryOp::Add:
    stream << "+";
    break;
  case ValueBinaryOp::Subtract:
    stream << "-";
    break;
  case ValueBinaryOp::Multiply:
    stream << "*";
    break;
  case ValueBinaryOp::Divide:
    stream << "/";
    break;
  case ValueBinaryOp::Remainder:
    stream << "%";
    break;
  case ValueBinaryOp::LessThan:
    stream << "<";
    break;
  case ValueBinaryOp::LessThanOrEqual:
    stream << "<=";
    break;
  case ValueBinaryOp::Equal:
    stream << "==";
    break;
  case ValueBinaryOp::NotEqual:
    stream << "!=";
    break;
  case ValueBinaryOp::GreaterThanOrEqual:
    stream << ">=";
    break;
  case ValueBinaryOp::GreaterThan:
    stream << ">";
    break;
  case ValueBinaryOp::BitwiseAnd:
    stream << "&";
    break;
  case ValueBinaryOp::BitwiseOr:
    stream << "|";
    break;
  case ValueBinaryOp::BitwiseXor:
    stream << "^";
    break;
  case ValueBinaryOp::ShiftLeft:
    stream << "<<";
    break;
  case ValueBinaryOp::ShiftRight:
    stream << ">>";
    break;
  case ValueBinaryOp::ShiftRightUnsigned:
    stream << ">>>";
    break;
  case ValueBinaryOp::Minimum:
    stream << "<|";
    break;
  case ValueBinaryOp::Maximum:
    stream << ">|";
    break;
  case ValueBinaryOp::IfVoid:
    stream << "!!";
    break;
  case ValueBinaryOp::IfNull:
    stream << "??";
    break;
  case ValueBinaryOp::IfFalse:
    stream << "||";
    break;
  case ValueBinaryOp::IfTrue:
    stream << "&&";
    break;
  default:
    stream << "<VALUEBINARYOP:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, ValueTernaryOp value, const Options&) {
  switch (value) {
  case ValueTernaryOp::IfThenElse:
    stream << "?:";
    break;
  default:
    stream << "<VALUETERNARYOP:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, ValueMutationOp value, const Options&) {
  switch (value) {
  case ValueMutationOp::Assign:
    stream << "=";
    break;
  case ValueMutationOp::Decrement:
    stream << "--";
    break;
  case ValueMutationOp::Increment:
    stream << "++";
    break;
  case ValueMutationOp::Add:
    stream << "+=";
    break;
  case ValueMutationOp::Subtract:
    stream << "-=";
    break;
  case ValueMutationOp::Multiply:
    stream << "*=";
    break;
  case ValueMutationOp::Divide:
    stream << "/=";
    break;
  case ValueMutationOp::Remainder:
    stream << "%=";
    break;
  case ValueMutationOp::BitwiseAnd:
    stream << "&=";
    break;
  case ValueMutationOp::BitwiseOr:
    stream << "|=";
    break;
  case ValueMutationOp::BitwiseXor:
    stream << "^=";
    break;
  case ValueMutationOp::ShiftLeft:
    stream << "<<=";
    break;
  case ValueMutationOp::ShiftRight:
    stream << ">>=";
    break;
  case ValueMutationOp::ShiftRightUnsigned:
    stream << ">>>=";
    break;
  case ValueMutationOp::Minimum:
    stream << "<|=";
    break;
  case ValueMutationOp::Maximum:
    stream << ">|=";
    break;
  case ValueMutationOp::IfVoid:
    stream << "!!=";
    break;
  case ValueMutationOp::IfNull:
    stream << "??=";
    break;
  case ValueMutationOp::IfFalse:
    stream << "&&=";
    break;
  case ValueMutationOp::IfTrue:
    stream << "||=";
    break;
  case ValueMutationOp::Noop:
    stream << "<NOOP>";
    break;
  default:
    stream << "<VALUEMUTATIONOP:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, TypeUnaryOp value, const Options&) {
  switch (value) {
  case TypeUnaryOp::Pointer:
    stream << "*";
    break;
  case TypeUnaryOp::Iterator:
    stream << "!";
    break;
  case TypeUnaryOp::Array:
    stream << "[]";
    break;
  case TypeUnaryOp::Nullable:
    stream << "?";
    break;
  default:
    stream << "<TYPEUNARYOP:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, TypeBinaryOp value, const Options&) {
  switch (value) {
  case TypeBinaryOp::Map:
    stream << "[]";
    break;
  case TypeBinaryOp::Union:
    stream << "|";
    break;
  default:
    stream << "<TYPEBINARYOP:" << int(value) << ">";
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
    stream << "<SEVERITY:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, const Exception& value, const Options&) {
  stream << value.what();
  for (auto& field : value) {
    if (!field.first.empty()) {
      stream << "\n  " << field.first << '=' << field.second;
    }
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

void egg::ovum::Printer::write(const HardValue& value) {
  this->write(value.get());
}

void egg::ovum::Printer::write(const HardObject& value) {
  this->write(value.get());
}

void egg::ovum::Printer::write(const Type& value) {
  this->write(value.get());
}

void egg::ovum::Printer::write(const IValue& value) {
  if (Bits::hasAnySet(value.getPrimitiveFlag(), ValueFlags::Object)) {
    this->anticycle(value);
  } else {
    value.print(*this);
  }
}

void egg::ovum::Printer::write(const IObject& value) {
  this->anticycle(value);
}

void egg::ovum::Printer::write(const IType& value) {
  this->anticycle(value);
}

void egg::ovum::Printer::anticycle(const ICollectable& value) {
  // Prevent cycles causing stack overflows
  if (this->options.visited == nullptr) {
    // Create a 'visited' context
    Print::Options voptions{ this->options };
    std::set<const ICollectable*> visited;
    visited.emplace(&value);
    voptions.visited = &visited;
    Printer vprinter{ this->stream, voptions };
    value.print(vprinter);
  } else if (this->options.visited->emplace(&value).second) {
    // First time visited
    value.print(*this);
  } else {
    // Cycle found
    this->stream << "<cycle>";
  }
}

int egg::ovum::Printer::describe(ValueFlags value) {
  if (value == ValueFlags::None) {
    this->stream << "<none>";
    return 0;
  }
  this->quote();
  auto precedence = Type::print(*this, value);
  this->quote();
  return precedence;
}

void egg::ovum::Printer::describe(const IType& value) {
  this->quote();
  value.print(*this);
  this->quote();
}

void egg::ovum::Printer::describe(const IValue& value) {
  // TODO complex types
  Print::Options quoted{ this->options };
  if (quoted.quote == '\0') {
    quoted.quote = '\'';
  }
  Bool bvalue;
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN
  switch (value.getPrimitiveFlag()) {
  case ValueFlags::None:
    this->stream << "nothing";
    return;
  case ValueFlags::Void:
    this->stream << quoted.quote << "void" << quoted.quote;
    return;
  case ValueFlags::Null:
    this->stream << quoted.quote << "null" << quoted.quote;
    return;
  case ValueFlags::Bool:
    if (value.getBool(bvalue)) {
      this->stream << quoted.quote << (bvalue ? "true" : "false") << quoted.quote;
      return;
    }
    break;
  }
  EGG_WARNING_SUPPRESS_SWITCH_END
  if (value.getPrimitiveFlag() == ValueFlags::Type) {
    this->stream << "type ";
  } else {
    this->stream << "a value of type ";
  }
  Printer printer{ this->stream, quoted };
  printer.describe(*value.getRuntimeType());
}
