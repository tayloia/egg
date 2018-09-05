#include "ovum/ovum.h"

void egg::ovum::LocationSource::formatSourceString(StringBuilder& sb) const {
  sb.add(this->file);
  if (this->column > 0) {
    sb.add('(', this->line, ',', this->column, ')');
  } else if (this->line > 0) {
    sb.add('(', this->line, ')');
  }
}

egg::ovum::String egg::ovum::LocationSource::toSourceString() const {
  StringBuilder sb;
  this->formatSourceString(sb);
  return sb.str();
}

void egg::ovum::LocationRuntime::formatRuntimeString(StringBuilder& sb) const {
  this->formatSourceString(sb);
  if (!this->function.empty()) {
    if (sb.empty()) {
      sb.add(' ');
    }
    sb.add('<', this->function, '>');
  }
}

egg::ovum::String egg::ovum::LocationRuntime::toRuntimeString() const {
  StringBuilder sb;
  this->formatRuntimeString(sb);
  return sb.str();
}
