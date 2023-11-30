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

  const char* valueFlagsComponent(ValueFlags flags) {
    EGG_WARNING_SUPPRESS_SWITCH_BEGIN
    switch (flags) {
#define EGG_OVUM_VALUE_FLAGS_COMPONENT(name, text) case ValueFlags::name: return text;
      EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VALUE_FLAGS_COMPONENT)
#undef EGG_OVUM_VALUE_FLAGS_COMPONENT
    case ValueFlags::Any:
      return "any";
    }
  EGG_WARNING_SUPPRESS_SWITCH_END
    return nullptr;
  }

  int valueFlagsWrite(std::ostream& os, ValueFlags flags) {
    auto* component = valueFlagsComponent(flags);
    if (component != nullptr) {
      os << component;
      return 0;
    }
    if (Bits::hasAnySet(flags, ValueFlags::Null)) {
      auto nonnull = valueFlagsWrite(os, Bits::clear(flags, ValueFlags::Null));
      os << '?';
      return std::max(nonnull, 1);
    }
    if (Bits::hasAnySet(flags, ValueFlags::Void)) {
      os << "void|";
      (void)valueFlagsWrite(os, Bits::clear(flags, ValueFlags::Void));
      return 2;
    }
    auto head = Bits::topmost(flags);
    assert(head != ValueFlags::None);
    component = valueFlagsComponent(head);
    assert(component != nullptr);
    os << component << '|';
    (void)valueFlagsWrite(os, Bits::clear(flags, head));
    return 2;
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

void egg::ovum::Print::write(std::ostream& stream, const IType* value, const Options&) {
  if (value == nullptr) {
    stream << "null";
  } else {
    stream << value->toStringPrecedence().first;
  }
}

void egg::ovum::Print::write(std::ostream& stream, const Type& value, const Options& options) {
  Print::write(stream, &*value, options);
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
    stream << "<UNKNOWN:" << int(value) << ">";
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
    stream << "<UNKNOWN:" << int(value) << ">";
    break;
  }
}

void egg::ovum::Print::write(std::ostream& stream, ValueTernaryOp value, const Options&) {
  switch (value) {
  case ValueTernaryOp::IfThenElse:
    stream << "?:";
    break;
  default:
    stream << "<UNKNOWN:" << int(value) << ">";
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
    stream << "<UNKNOWN:" << int(value) << ">";
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
    stream << "<UNKNOWN:" << int(value) << ">";
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

int egg::ovum::Print::describe(std::ostream& stream, ValueFlags value, const Options& options) {
  if (options.quote != '\0') {
    stream << options.quote;
  }
  auto precedence = valueFlagsWrite(stream, value);
  if (options.quote != '\0') {
    stream << options.quote;
  }
  return precedence;
}

void egg::ovum::Print::describe(std::ostream& stream, const IType& value, const Options& options) {
  // TODO complex types
  (void)Print::describe(stream, value.getPrimitiveFlags(), options);
}

void egg::ovum::Print::describe(std::ostream& stream, const IValue& value, const Options& options) {
  // TODO complex types
  Options quoted = options;
  if (quoted.quote == '\0') {
    quoted.quote = '\'';
  }
  Bool bvalue;
  EGG_WARNING_SUPPRESS_SWITCH_BEGIN
  switch (value.getFlags()) {
  case ValueFlags::None:
    stream << "nothing";
    return;
  case ValueFlags::Void:
    stream << quoted.quote << "void" << quoted.quote;
    return;
  case ValueFlags::Null:
    stream << quoted.quote << "null" << quoted.quote;
    return;
  case ValueFlags::Bool:
    if (value.getBool(bvalue)) {
      stream << quoted.quote << (bvalue ? "true" : "false") << quoted.quote;
      return;
    }
    break;
  }
  EGG_WARNING_SUPPRESS_SWITCH_END
  stream << "a value of type ";
  Print::describe(stream, *value.getType(), quoted);
}

egg::ovum::Printer& egg::ovum::Printer::operator<<(const String& value) {
  this->os << value.toUTF8();
  return *this;
}
