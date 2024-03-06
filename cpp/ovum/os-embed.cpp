#include "ovum/ovum.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"

std::string egg::ovum::os::embed::getExecutableFilename() {
  auto executable = egg::ovum::os::file::getExecutablePath();
  auto slash = executable.rfind('/');
  if (slash != std::string::npos) {
    executable.erase(0, slash + 1);
  }
  return executable;
}

std::string egg::ovum::os::embed::getExecutableStub() {
  auto executable = egg::ovum::os::embed::getExecutableFilename();
  if ((executable.size() > 4) && executable.ends_with(".exe")) {
    executable.resize(executable.size() - 4);
  }
  return executable;
}
