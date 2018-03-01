#include "yolk.h"

#include <direct.h>

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

#if EGG_PLATFORM == EGG_PLATFORM_MSVC
namespace {
  const char* getExecutablePath() {
    // See https://github.com/gpakosz/whereami/blob/master/src/whereami.c
    char* pgmptr = nullptr;
    _get_pgmptr(&pgmptr);
    return pgmptr;
  }

  bool getExecutableDirectory(std::string& directory) {
    // Try to get the directory of the executable in normalized form with a trailing slash
    const char* exe = getExecutablePath();
    if (exe != nullptr) {
      const char* end = strrchr(exe, '\\');
      if (end) {
        directory = egg::yolk::File::normalizePath(std::string(exe, end), true);
        return true;
      }
    }
    return false;
  }

  bool getDevelopmentEggRoot(std::string& directory) {
    // Check for if we're running inside a development directory (e.g. Google Test adapter)
    if (getExecutableDirectory(directory)) {
      auto msvc = directory.rfind("/msvc/bin/");
      if (msvc != std::string::npos) {
        directory.resize(msvc + 1);
        return true;
      }
    }
    return false;
  }
}
#endif

std::string egg::yolk::File::getCurrentDirectory() {
  // Gets the current working directory in normalized form with a trailing slash
  char buffer[2048];
  _getcwd(buffer, sizeof(buffer));
  return File::normalizePath(buffer, true);
}

std::string egg::yolk::File::getTildeDirectory() {
  // Gets the egg directory in normalized form with a trailing slash
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  // Check for if we're running inside a development directory (e.g. Google Test adapter)
  std::string eggroot;
  if (getDevelopmentEggRoot(eggroot)) {
    return eggroot;
  }
#endif
  return File::getCurrentDirectory();
}

std::string egg::yolk::File::resolvePath(const std::string& path) {
  // Resolve a file path in normalized form
  auto resolved = path;
  if (String::startsWith(resolved, "~/")) {
    resolved = File::getTildeDirectory() + resolved.substr(2);
  }
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  std::replace(resolved.begin(), resolved.end(), '/', '\\');
#endif
  return resolved;
}
