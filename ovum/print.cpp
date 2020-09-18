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

void egg::ovum::Print::write(std::ostream& stream, ValueFlags flags) {
  size_t found = 0;
#define EGG_OVUM_VARIANT_PRINT(name, text) if (Bits::hasAnySet(flags, ValueFlags::name)) { if (++found > 1) { stream << '|'; } stream << text; }
  EGG_OVUM_VALUE_FLAGS(EGG_OVUM_VARIANT_PRINT)
#undef EGG_OVUM_VARIANT_PRINT
  if (found == 0) {
    stream << '(' << int(flags) << ')';
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
  case egg::ovum::ValueFlags::Pointer: {
    egg::ovum::Value v;
    if (value->getChild(v)) {
      auto* p = &v.get();
      stream << "[pointer 0x" << std::hex << std::setw(sizeof(p) * 2) << std::setfill('0') << p << ']';
      return;
    }
    break;
  }
  }
  EGG_WARNING_SUPPRESS_SWITCH_END();
  Print::write(stream, value->toString());
}
