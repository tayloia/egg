#include "ovum/ovum.h"
#include "ovum/os-file.h"

#include <filesystem>
#include <random>

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
  const char* getExecutableFile() {
    // See https://github.com/gpakosz/whereami/blob/master/src/whereami.c
    char* pgmptr = nullptr;
    _get_pgmptr(&pgmptr);
    return pgmptr;
  }
  bool tryExecutableDirectory(std::string& directory) {
    // Try to get the directory of the executable in normalized form with a trailing slash
    auto* exe = getExecutableFile();
    if (exe != nullptr) {
      const char* end = strrchr(exe, '\\');
      if (end) {
        directory = egg::ovum::os::file::normalizePath(std::string(exe, end), true);
        return true;
      }
    }
    return false;
  }
#else
  std::string getExecutableFile() {
    return std::filesystem::canonical("/proc/self/exe");
  }
#endif
  void terminate(std::string& str, char terminator) {
    if (str.empty() || (str.back() != terminator)) {
      str.push_back(terminator);
    }
  }
  class TemporaryDirectories final {
    TemporaryDirectories(const TemporaryDirectories&) = delete;
    TemporaryDirectories& operator=(const TemporaryDirectories&) = delete;
  private:
    std::vector<std::filesystem::path> entries;
    std::mutex mutex;
  public:
    TemporaryDirectories() = default;
    void add(const std::filesystem::path& entry) {
      // TODO exception handling
      std::unique_lock lock{ this->mutex };
      this->entries.push_back(entry);
    }
    void purge() {
      // TODO exception handling
      std::unique_lock lock{ this->mutex };
      for (auto& entry : this->entries) {
        std::filesystem::remove_all(entry);
      }
      this->entries.clear();
    }
    ~TemporaryDirectories() {
      this->purge();
    }
  };
}

std::string egg::ovum::os::file::normalizePath(const std::string& path, bool trailingSlash) {
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

std::string egg::ovum::os::file::denormalizePath(const std::string& path, bool trailingSlash) {
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

std::string egg::ovum::os::file::getCurrentDirectory() {
  // Gets the current working directory in normalized form with a trailing slash
  auto path = std::filesystem::current_path().string();
  return os::file::normalizePath(path, true);
}

std::string egg::ovum::os::file::getDevelopmentDirectory() {
  // Gets the egg directory in normalized form with a trailing slash
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  // Check for if we're running inside a development directory (e.g. Google Test adapter)
  std::string directory;
  if (tryExecutableDirectory(directory)) {
    auto msvc = directory.rfind("/bin/msvc/");
    if (msvc != std::string::npos) {
      directory.resize(msvc + 1);
      return directory;
    }
  }
#endif
  return os::file::getCurrentDirectory();
}

std::string egg::ovum::os::file::getExecutableDirectory() {
  // Gets the directory of the currently-running executable
  auto executable = os::file::normalizePath(getExecutableFile(), false);
  auto slash = executable.rfind('/');
  if (slash == std::string::npos) {
    return os::file::getCurrentDirectory();
  }
  executable.resize(slash + 1);
  return executable;
}

std::string egg::ovum::os::file::getExecutablePath() {
  // Gets the path of the currently-running executable
  return os::file::normalizePath(getExecutableFile(), false);
}

std::string egg::ovum::os::file::createTemporaryDirectory(const std::string& prefix, size_t attempts) {
  // See https://stackoverflow.com/a/58454949
  auto tmpdir = std::filesystem::temp_directory_path();
  std::random_device randev{};
  std::mt19937 prng{ randev() };
  std::uniform_int_distribution<uint64_t> rand{ 0 };
  for (size_t attempt = 0; attempt < attempts; ++attempt) {
    std::stringstream ss;
    ss << prefix << std::hex << rand(prng);
    auto path = tmpdir / ss.str();
    if (std::filesystem::create_directory(path)) {
      static TemporaryDirectories registry;
      registry.add(path);
      return egg::ovum::os::file::normalizePath(path.string(), true);
    }
  }
  throw std::runtime_error("Failed to create temporary directory");
}

char egg::ovum::os::file::slash() {
  // Gets the path of the currently-running executable
  return char(std::filesystem::path::preferred_separator);
}