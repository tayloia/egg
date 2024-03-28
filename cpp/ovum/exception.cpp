#include "ovum/ovum.h"
#include "ovum/exception.h"
#include "ovum/file.h"

namespace {
  const std::string& formatWhat(const char* fmt, egg::ovum::Exception& exception) {
    auto& value = exception["what"];
    if (value.empty()) {
      value = exception.format(fmt);
    }
    return value;
  }
  std::string formatResource(const std::string& resource, size_t limit) {
    auto path = egg::ovum::File::normalizePath(resource);
    if (limit > 0) {
      auto slash = path.rfind('/');
      while ((--limit > 0) && (slash != std::string::npos) && (slash > 0)) {
        slash = path.rfind('/', slash - 1);
      }
      if (++slash < path.size()) {
        return path.substr(slash);
      }
    }
    return path;
  }
  std::string formatWhere(const std::string& file, size_t line, size_t column = 0) {
    std::string result = file + '(' + std::to_string(line);
    if (column > 0) {
      result += ',' + std::to_string(column);
    }
    result += ')';
    return result;
  }
}

egg::ovum::Exception::Exception(const std::string& reason, const std::source_location location)
  : Exception(reason, formatWhere(formatResource(location.file_name(), 3), location.line())) {
}

egg::ovum::Exception::Exception(const std::string& reason, const std::string& where)
  : std::runtime_error(where + ": " + reason) {
  this->emplace("reason", reason);
  this->emplace("where", where);
}

const char* egg::ovum::Exception::what() const noexcept {
  const char* fmt = std::runtime_error::what();
  try {
    return formatWhat(fmt, *const_cast<Exception*>(this)).c_str();
  } catch (...) {
    return fmt;
  }
}

std::string egg::ovum::Exception::format(const char* fmt) const {
  std::string result{};
  if (fmt != nullptr) {
    while (*fmt != '\0') {
      if (*fmt != '{') {
        result.push_back(*fmt);
      } else {
        auto* end = std::strchr(fmt, '}');
        if (end == nullptr) {
          // No closing brace, so append the remainder of the format
          return result + fmt;
        }
        auto key = std::string(fmt + 1, size_t(end - fmt - 1));
        auto found = this->find(key);
        if (found != this->end()) {
          // Known key
          result.append(found->second);
        } else {
          // Unknown key
          result.append(fmt, size_t(end - fmt + 1));
        }
        fmt = end;
      }
      fmt++;
    }
  }
  return result;
}

egg::ovum::SyntaxException::SyntaxException(const std::string& reason, const std::string& resource, const SourceLocation& location, const std::string& token)
  : Exception(reason, formatWhere(resource, location.line, location.column)),
    range_value({ location, location }) {
  this->emplace("token", token);
  this->emplace("resource", resource);
}

egg::ovum::SyntaxException::SyntaxException(const std::string& reason, const std::string& resource, const SourceRange& range, const std::string& token)
  : Exception(reason, formatWhere(resource, range.begin.line, range.begin.column)),
    range_value({ range.begin, range.end }) {
  this->emplace("token", token);
  this->emplace("resource", resource);
}
