#include "ovum/ovum.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"

#include <filesystem>

std::string egg::ovum::os::embed::getExecutableFilename() {
  auto executable = os::file::getExecutablePath();
  auto slash = executable.rfind('/');
  if (slash != std::string::npos) {
    executable.erase(0, slash + 1);
  }
  return executable;
}

std::string egg::ovum::os::embed::getExecutableStub() {
  auto executable = os::embed::getExecutableFilename();
  if ((executable.size() > 4) && executable.ends_with(".exe")) {
    executable.resize(executable.size() - 4);
  }
  return executable;
}

void egg::ovum::os::embed::cloneExecutable(const std::string& target) {
  auto source = os::file::getExecutablePath();
  if (!std::filesystem::copy_file(os::file::denormalizePath(source, false), os::file::denormalizePath(target, false))) {
    throw std::runtime_error("Cannot clone executable file");
  }
}
