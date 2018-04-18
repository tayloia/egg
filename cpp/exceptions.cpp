#include "yolk.h"

namespace {
  std::string formatWhere(const std::string& resource, const egg::yolk::ExceptionLocation& location) {
    std::stringstream ss;
    ss << resource << '(' << location.line;
    if (location.column > 0) {
      ss << ',' << ' ' << location.column;
    }
    ss << ')';
    return ss.str();
  }
}

egg::yolk::Exception::Exception(const std::string& what, const std::string& reason, const std::string& where)
  : std::runtime_error(what), reason_value(reason), where_value(where) {
}

egg::yolk::Exception::Exception(const std::string& reason, const std::string& where)
  : std::runtime_error(where + ": " + reason), reason_value(reason), where_value(where) {
}

egg::yolk::Exception::Exception(const std::string& reason, const std::string& file, size_t line)
  : Exception(reason, File::normalizePath(file) + "(" + String::fromUnsigned(line) + ")") {
}

egg::yolk::SyntaxException::SyntaxException(const std::string& reason, const std::string& resource, const ExceptionLocation& location, const std::string& token)
  : Exception(reason, formatWhere(resource, location)),
    token_value(token),
    resource_value(resource),
    location_value({ location, {} }) {
}

egg::yolk::SyntaxException::SyntaxException(const std::string& reason, const std::string& resource, const ExceptionLocationRange& range, const std::string& token)
  : Exception(reason, formatWhere(resource, range.begin)),
    token_value(token),
    resource_value(resource),
    location_value({ range.begin, range.end }) {
}
