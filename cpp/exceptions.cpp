#include "yolk.h"

egg::yolk::Exception::Exception(const std::string& what, const std::string& location)
  : std::runtime_error(what), location(location) {
}

egg::yolk::Exception::Exception(const std::string& what, const std::string& file, size_t line)
  : Exception(what, File::normalizePath(file) + "(" + std::to_string(line) + ")") {
}

egg::yolk::Exception::Exception(const std::string& what, const std::string& file, size_t line, size_t column)
  : Exception(what, File::normalizePath(file) + "(" + std::to_string(line) + ", " + std::to_string(column) + ")") {
}
