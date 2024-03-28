#include "ovum/ovum.h"
#include "ovum/exception.h"
#include "ovum/file.h"

namespace {
  const std::string& formatWhat(const char* format, egg::ovum::Exception& exception) {
    auto& value = exception["what"];
    if (value.empty()) {
      value = format;
    }
    return value;
  }

  std::string formatWhere(const std::string& file, size_t line, size_t column = 0) {
    std::stringstream oss;
    oss << file << '(' << line;
    if (column > 0) {
      oss << ',' << column;
    }
    oss << ')';
    return oss.str();
  }
}

egg::ovum::Exception::Exception(const std::string& what, const std::string& reason, const std::string& where)
  : std::runtime_error(what) {
  this->emplace("reason", reason);
  this->emplace("where", where);
}

egg::ovum::Exception::Exception(const std::string& reason, const std::source_location location)
  : Exception(reason, formatWhere(File::normalizePath(location.file_name()), location.line())) {
}

egg::ovum::Exception::Exception(const std::string& reason, const std::string& where)
  : Exception(where + ": " + reason, reason, where) {
}

const char* egg::ovum::Exception::what() const noexcept {
  const char* format = std::runtime_error::what();
  try {
    return formatWhat(format, *const_cast<Exception*>(this)).c_str();
  } catch (...) {
    return format;
  }
}

egg::ovum::SyntaxException::SyntaxException(const std::string& reason, const std::string& resource, const SourceLocation& location, const std::string& token)
  : Exception(reason, formatWhere(resource, location.line, location.column)),
    token_value(token),
    resource_value(resource),
    range_value({ location, location }) {
}

egg::ovum::SyntaxException::SyntaxException(const std::string& reason, const std::string& resource, const SourceRange& range, const std::string& token)
  : Exception(reason, formatWhere(resource, range.begin.line, range.begin.column)),
    token_value(token),
    resource_value(resource),
    range_value({ range.begin, range.end }) {
}
