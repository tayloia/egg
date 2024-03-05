#include "ovum/ovum.h"
#include "ovum/exception.h"
#include "ovum/file.h"

namespace {
  std::string formatWhere(const std::string& resource, const egg::ovum::SourceLocation& location) {
    std::stringstream oss;
    oss << resource << '(' << location.line;
    if (location.column > 0) {
      oss << ',' << ' ' << location.column;
    }
    oss << ')';
    return oss.str();
  }
}

egg::ovum::Exception::Exception(const std::string& what, const std::string& reason, const std::string& where)
  : std::runtime_error(what), reason_value(reason), where_value(where) {
}

egg::ovum::Exception::Exception(const std::string& reason, const std::string& where)
  : std::runtime_error(where + ": " + reason), reason_value(reason), where_value(where) {
}

egg::ovum::Exception::Exception(const std::string& reason, const std::string& file, size_t line)
  : Exception(reason, File::normalizePath(file) + "(" + std::to_string(line) + ")") {
}

egg::ovum::SyntaxException::SyntaxException(const std::string& reason, const std::string& resource, const SourceLocation& location, const std::string& token)
  : Exception(reason, formatWhere(resource, location)),
    token_value(token),
    resource_value(resource),
    range_value({ location, location }) {
}

egg::ovum::SyntaxException::SyntaxException(const std::string& reason, const std::string& resource, const SourceRange& range, const std::string& token)
  : Exception(reason, formatWhere(resource, range.begin)),
    token_value(token),
    resource_value(resource),
    range_value({ range.begin, range.end }) {
}
