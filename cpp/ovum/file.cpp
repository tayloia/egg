#include "ovum/ovum.h"
#include "ovum/file.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"

#include <fstream>

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
  auto native = File::resolvePath(path);
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
  auto native = File::resolvePath(path);
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

std::string egg::ovum::File::slurp(const std::string& path) {
  // Slurp the entire file as a string of bytes
  std::ifstream ifs{ File::resolvePath(path), std::ios::in | std::ios::binary };
  ifs.seekg(0, std::ios::end);
  auto bytes = ifs.tellg();
  std::string buffer(size_t(bytes), '\0');
  ifs.seekg(0);
  ifs.read(buffer.data(), bytes);
  return buffer;
}

bool egg::ovum::File::removeFile(const std::string& path) {
  // Remove the file
  auto native = File::resolvePath(path);
  auto status = std::filesystem::status(native);
  auto type = status.type();
  if ((type == std::filesystem::file_type::regular) || (type == std::filesystem::file_type::symlink)) {
    std::error_code error;
    if (!std::filesystem::remove(native, error)) {
      throw Exception("Cannot remove file: {error}")
        .with("path", path)
        .with("error", egg::ovum::os::process::format(error));
    }
    return true;
  }
  if (type == std::filesystem::file_type::not_found) {
    return false;
  }
  throw Exception("Cannot remove file: {error}")
    .with("path", path)
    .with("error", "not a file");
}
