#include "ovum/ovum.h"
#include "ovum/file.h"
#include "ovum/stream.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"

#include <fstream>

std::vector<std::string> egg::ovum::File::readDirectory(const std::filesystem::path& path) {
  // Read the directory entries
  std::vector<std::string> filenames;
  std::error_code error;
  for (auto& file : std::filesystem::directory_iterator(path, error)) {
    filenames.push_back(file.path().filename().string());
  }
  return filenames;
}

egg::ovum::File::Kind egg::ovum::File::getKind(const std::filesystem::path& path) {
  // Read the status
  auto status = std::filesystem::status(path);
  if (std::filesystem::is_directory(status)) {
    return Kind::Directory;
  }
  if (std::filesystem::is_regular_file(status)) {
    return Kind::File;
  }
  return Kind::Unknown;
}

std::string egg::ovum::File::slurp(const std::filesystem::path& path) {
  // Slurp the entire file as a string of bytes
  std::ifstream ifs{ path, std::ios::binary };
  if (!ifs) {
    throw egg::ovum::Exception("Cannot read file: '{path}'").with("path", path.generic_string());
  }
  ifs.seekg(0, std::ios::end);
  auto bytes = ifs.tellg();
  std::string buffer(size_t(bytes), '\0');
  ifs.seekg(0);
  ifs.read(buffer.data(), bytes);
  return buffer;
}

bool egg::ovum::File::removeFile(const std::filesystem::path& path) {
  // Remove the file
  auto status = std::filesystem::status(path);
  if (!std::filesystem::exists(status)) {
    return false;
  }
  if (std::filesystem::is_regular_file(status)) {
    std::error_code error;
    if (!std::filesystem::remove(path, error)) {
      throw Exception("Cannot remove file: {error}")
        .with("path", path.generic_string())
        .with("error", egg::ovum::os::process::format(error));
    }
    return true;
  }
  throw Exception("Path to remove is not a regular file: '{path}'")
    .with("path", path.generic_string());
}
