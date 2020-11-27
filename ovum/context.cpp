#include "ovum/ovum.h"

bool egg::ovum::LocationSource::printSource(Printer& printer) const {
  // Return 'true' iff something is written
  printer << this->file;
  if (this->column > 0) {
    printer << '(' << this->line << ',' << this->column << ')';
    return true;
  }
  if (this->line > 0) {
    printer << '(' << this->line << ')';
    return true;
  }
  return !this->file.empty();
}

egg::ovum::String egg::ovum::LocationSource::toSourceString() const {
  StringBuilder sb;
  (void)this->printSource(sb);
  return sb.str();
}

bool egg::ovum::LocationRuntime::printRuntime(Printer& printer) const {
  // Return 'true' iff something is written
  auto written = this->printSource(printer);
  if (!this->function.empty()) {
    if (written) {
      printer << ' ';
    }
    printer << '<' << this->function << '>';
    return true;
  }
  return written;
}

egg::ovum::String egg::ovum::LocationRuntime::toRuntimeString() const {
  StringBuilder sb;
  (void)this->printRuntime(sb);
  return sb.str();
}
