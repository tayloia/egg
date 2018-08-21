#include "ovum/ovum.h"

namespace {
  void formatSourceLocation(egg::ovum::StringBuilder& sb, const egg::ovum::LocationSource& location) {
    sb.add(location.file);
    if (location.column > 0) {
      sb.add('(', location.line, ',', location.column, ')');
    } else if (location.line > 0) {
      sb.add('(', location.line, ')');
    }
  }
}

egg::ovum::String egg::ovum::LocationSource::toSourceString() const {
  StringBuilder sb;
  formatSourceLocation(sb, *this);
  return sb.str();
}

egg::ovum::String egg::ovum::LocationRuntime::toRuntimeString() const {
  StringBuilder sb;
  formatSourceLocation(sb, *this);
  if (!this->function.empty()) {
    if (sb.empty()) {
      sb.add(' ');
    }
    sb.add('<', this->function, '>');
  }
  return sb.str();
}
