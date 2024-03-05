#include "ovum/ovum.h"
#include "ovum/file.h"

#include <filesystem>

namespace {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  std::string transform(const std::string& src, std::function<char(char)> lambda) {
    std::string dst;
    dst.reserve(src.size());
    std::transform(src.begin(), src.end(), std::back_inserter(dst), lambda);
    return dst;
  }
  std::string replace(const std::string& src, char from, char to) {
    return transform(src, [from, to](char x) { return (x == from) ? to : x; });
  }
#endif
  bool startsWith(const std::string& haystack, const std::string& needle) {
    return (haystack.size() >= needle.size()) && std::equal(needle.begin(), needle.end(), haystack.begin());
  }
  void terminate(std::string& str, char terminator) {
    if (str.empty() || (str.back() != terminator)) {
      str.push_back(terminator);
    }
  }
}

std::string egg::ovum::File::normalizePath(const std::string& path, bool trailingSlash) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  auto result = transform(path, [](char x) { return (x == '\\') ? '/' : char(std::tolower(x)); });
  if (trailingSlash) {
    terminate(result, '/');
  }
  return result;
#else
  if (trailingSlash) {
    auto result = path;
    terminate(result, '/');
    return result;
  }
  return path;
#endif
}

std::string egg::ovum::File::denormalizePath(const std::string& path, bool trailingSlash) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  auto result = replace(path, '/', '\\');
  if (trailingSlash) {
    terminate(result, '\\');
  }
  return result;
#else
  if (trailingSlash) {
    auto result = path;
    terminate(result, '/');
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
        directory = egg::ovum::File::normalizePath(std::string(exe, end), true);
        return true;
      }
    }
    return false;
  }

  bool getDevelopmentEggRoot(std::string& directory) {
    // Check for if we're running inside a development directory (e.g. Google Test adapter)
    if (getExecutableDirectory(directory)) {
      auto msvc = directory.rfind("/bin/msvc/");
      if (msvc != std::string::npos) {
        directory.resize(msvc + 1);
        return true;
      }
    }
    return false;
  }
}
#endif

std::string egg::ovum::File::getCurrentDirectory() {
  // Gets the current working directory in normalized form with a trailing slash
  auto path = std::filesystem::current_path().string();
  return File::normalizePath(path, true);
}

std::string egg::ovum::File::getTildeDirectory() {
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

std::string egg::ovum::File::resolvePath(const std::string& path) {
  // Resolve a file path in normalized form
  auto resolved{ path };
  if (startsWith(resolved, "~/")) {
    resolved = File::getTildeDirectory() + resolved.substr(2);
  }
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  std::replace(resolved.begin(), resolved.end(), '/', '\\');
#endif
  return resolved;
}

std::vector<std::string> egg::ovum::File::readDirectory(const std::string& path) {
  // Read the directory entries
  auto native = File::denormalizePath(File::resolvePath(path), false);
  std::vector<std::string> filenames;
  std::error_code error;
  for (auto& file : std::filesystem::directory_iterator(native, error)) {
    auto utf8 = file.path().filename().u8string();
    filenames.push_back(File::normalizePath(std::string(reinterpret_cast<char*>(utf8.data()), utf8.size())));
  }
  return filenames;
}

egg::ovum::File::Kind egg::ovum::File::getKind(const std::string& path) {
  // Read the directory entries
  auto native = File::denormalizePath(File::resolvePath(path), false);
  auto status = std::filesystem::status(native);
  auto type = status.type();
  if (type == std::filesystem::file_type::directory) {
    return Kind::Directory;
  }
  if (type == std::filesystem::file_type::regular) {
    return Kind::File;
  }
  return Kind::Unknown;
}
