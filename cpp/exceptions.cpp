#include "yolk.h"

egg::yolk::Exception::Exception(const std::string& reason, const std::string& where)
  : std::runtime_error(where + ": " + reason), reason_value(reason), where_value(where) {
}

egg::yolk::Exception::Exception(const std::string& what, const std::string& file, size_t line)
  : Exception(what, File::normalizePath(file) + "(" + std::to_string(line) + ")") {
}

egg::yolk::Exception::Exception(const std::string& what, const std::string& file, size_t line, size_t column)
  : Exception(what, File::normalizePath(file) + "(" + std::to_string(line) + ", " + std::to_string(column) + ")") {
}
