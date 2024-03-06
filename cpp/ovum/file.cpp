#include "ovum/ovum.h"
#include "ovum/file.h"
#include "ovum/os-file.h"

#include <filesystem>

std::string egg::ovum::File::normalizePath(const std::string& path, bool trailingSlash) {
  return os::file::normalizePath(path, trailingSlash);
}

std::string egg::ovum::File::denormalizePath(const std::string& path, bool trailingSlash) {
  return os::file::denormalizePath(path, trailingSlash);
}

std::string egg::ovum::File::resolvePath(const std::string& path) {
  // Resolve a file path in normalized form
  auto resolved{ path };
  if ((resolved.size() >= 2) && (resolved[0] == '~') && (resolved[1] == '/')) {
    resolved = os::file::getDevelopmentDirectory() + resolved.substr(2);
  }
  return os::file::denormalizePath(resolved, false);
}

std::vector<std::string> egg::ovum::File::readDirectory(const std::string& path) {
  // Read the directory entries
  auto native = os::file::denormalizePath(File::resolvePath(path), false);
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
  auto native = os::file::denormalizePath(File::resolvePath(path), false);
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
