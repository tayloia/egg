#include "yolk.h"

egg::yolk::Exception::Exception(const std::string& what, const std::string& file, size_t line)
  : std::runtime_error(what), location(File::normalizePath(file) + "(" + std::to_string(line) + ")") {
}

