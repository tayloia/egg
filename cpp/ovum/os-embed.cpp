#include "ovum/ovum.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"
#include "ovum/os-process.h"

std::string egg::ovum::os::embed::getExecutableFilename() {
  auto executable = os::file::getExecutablePath();
  auto slash = executable.rfind('/');
  if (slash != std::string::npos) {
    executable.erase(0, slash + 1);
  }
  return executable;
}

std::string egg::ovum::os::embed::getExecutableStem() {
  auto executable = os::embed::getExecutableFilename();
  if ((executable.size() > 4) && executable.ends_with(".exe")) {
    executable.resize(executable.size() - 4);
  }
  return executable;
}

void egg::ovum::os::embed::cloneExecutable(const std::filesystem::path& target, bool overwrite) {
  auto source = os::file::getExecutablePath();
  auto options = overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none;
  std::error_code error;
  if (!std::filesystem::copy_file(os::file::denormalizePath(source, false), target, options, error)) {
    throw Exception("Cannot clone executable file: {error}")
      .with("source", source)
      .with("target", target.generic_string())
      .with("error", egg::ovum::os::process::format(error));
  }
}
