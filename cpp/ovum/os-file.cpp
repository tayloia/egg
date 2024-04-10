#include "ovum/ovum.h"
#include "ovum/os-file.h"

#include <fstream>
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
  std::string terminate(const std::string& str, bool trailing, char terminator) {
    if (str.empty() || (str.back() != terminator)) {
      if (trailing) {
        return str + terminator;
      }
    } else {
      if (!trailing) {
        return str.substr(0, str.size() - 1);
      }
    }
    return str;
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
    static std::string remember(const std::filesystem::path& entry, bool directory) {
      static TemporaryDirectories registry;
      registry.add(entry);
      return egg::ovum::os::file::normalizePath(entry.string(), directory);
    }
  };
}

std::string egg::ovum::os::file::normalizePath(const std::string& path, bool trailingSlash) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  auto result = transform(path, [](char x) { return (x == '\\') ? '/' : char(std::tolower(x)); });
  return terminate(result, trailingSlash, '/');
#else
  return terminate(path, trailingSlash, '/');
#endif
}

std::string egg::ovum::os::file::denormalizePath(const std::string& path, bool trailingSlash) {
#if EGG_PLATFORM == EGG_PLATFORM_MSVC
  auto result = replace(path, '/', '\\');
  return terminate(result, trailingSlash, '\\');
#else
  return terminate(path, trailingSlash, '/');
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

std::string egg::ovum::os::file::getExecutableName(const std::string& path, bool removeExe) {
  // Gets the name of the currently-running executable
  std::string name;
  auto slash = path.find_last_of("/\\");
  if (slash == std::string::npos) {
    name = path;
  } else {
    name = path.substr(slash + 1);
  }
  if (removeExe) {
    auto length = name.size();
    if (length > 4) {
      const char* p = name.data() + length - 4;
      if ((p[0] == '.') && (std::tolower(p[1]) == 'e') && (std::tolower(p[2]) == 'x') && (std::tolower(p[3]) == 'e')) {
        name.erase(length - 4);
      }
    }
  }
  return name;
}

std::string egg::ovum::os::file::createTemporaryFile(const std::string& prefix, const std::string& suffix, size_t attempts) {
  auto tmpdir = std::filesystem::temp_directory_path();
  std::random_device randev{};
  std::mt19937 prng{ randev() };
  std::uniform_int_distribution<uint64_t> rand{ 0 };
  for (size_t attempt = 0; attempt < attempts; ++attempt) {
    std::stringstream ss;
    ss << prefix << std::hex << rand(prng) << suffix;
    auto path = tmpdir / ss.str();
    std::ofstream ofs{ path };
    if (ofs) {
      return TemporaryDirectories::remember(path, false);
    }
  }
  throw Exception("Failed to create temporary file: '{path}'").with("path", prefix + '*' + suffix);
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
      return TemporaryDirectories::remember(path, true);
    }
  }
  throw Exception("Failed to create temporary directory: '{path}'").with("path", prefix + '*');
}

char egg::ovum::os::file::slash() {
  // Gets the path of the currently-running executable
  return char(std::filesystem::path::preferred_separator);
}
