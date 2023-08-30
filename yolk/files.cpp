#include "yolk/yolk.h"

#if EGG_PLATFORM == EGG_PLATFORM_MSVC
#include <direct.h>
#include <filesystem>
#define getcwd _getcwd
#else
#include <unistd.h>
#include <dirent.h>
#endif

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
  if (getcwd(buffer, sizeof(buffer)) == nullptr) {
    return "./";
  }
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
  auto resolved{ path };
  if (String::startsWith(resolved, "~/")) {
    resolved = File::getTildeDirectory() + resolved.substr(2);
  }
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  std::replace(resolved.begin(), resolved.end(), '/', '\\');
#endif
  return resolved;
}

std::vector<std::string> egg::yolk::File::readDirectory(const std::string& path) {
  // Read the directory entries
  auto native = File::denormalizePath(File::resolvePath(path), false);
  std::vector<std::string> filenames;
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  std::error_code error;
  for (auto& file : std::filesystem::directory_iterator(native, error)) {
    auto utf8 = file.path().filename().u8string();
    filenames.push_back(File::normalizePath(std::string((const char*)utf8.data(), utf8.size())));
  }
#elif EGG_PLATFORM == EGG_PLATFORM_GCC
  // GCC doesn't have "std::filesystem" at all, yet
  DIR* dir = opendir(native.c_str());
  if (dir != nullptr) {
    for (auto* entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
      if (entry->d_name[0] == '.') {
        // Ignore "." and ".." entries
        if ((entry->d_name[1] == '\0') || ((entry->d_name[1] == '.') && (entry->d_name[2] == '\0'))) {
          continue;
        }
      }
      filenames.push_back(File::normalizePath(entry->d_name));
    }
    closedir(dir);
  }
#else
#error "Unsupported platform for egg::yolk::File::readDirectory"
#endif
  return filenames;
}
