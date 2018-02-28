#include "yolk.h"

std::string egg::yolk::File::normalizePath(const std::string& path, bool trailingSlash) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  auto result = String::transform(path, [](char x) { return (x == '\\') ? '/' : char(std::tolower(x)); });
  if (trailingSlash) {
    String::terminate(result, '/');
  }
  return result;
#else
  if (trailingSlash) {
    auto result = path;
    String::terminate(result, '/');
    return result;
  }
  return path;
#endif
}

std::string egg::yolk::File::denormalizePath(const std::string& path, bool trailingSlash) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  auto result = String::replace(path, '/', '\\');
  if (trailingSlash) {
    String::terminate(result, '\\');
  }
  return result;
#else
  if (trailingSlash) {
    auto result = path;
    String::terminate(result, '/');
    return result;
  }
  return path;
#endif
}

std::string egg::yolk::File::getTildeDirectory() {
  // Gets the current working directory in normalized form with a trailing slash
  char buffer[2048];
  _getcwd(buffer, sizeof(buffer));
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  // TODO Check for if we're running inside a development directory (e.g. Google Test adapter)
  auto msvc = std::strstr(buffer, "\\msvc\\bin\\");
  if (msvc != nullptr) {
    *msvc = '\0';
  }
#endif
  return File::normalizePath(buffer, true);
}

std::string egg::yolk::File::resolvePath(const std::string& path) {
  auto resolved = path;
  if (String::beginsWith(resolved, "~/")) {
    resolved = File::getTildeDirectory() + resolved.substr(2);
  }
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  std::replace(resolved.begin(), resolved.end(), '/', '\\');
#endif
  return resolved;
}
