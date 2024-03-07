#include "cli/cli.h"
#include "ovum/version.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"

#include <iostream>

namespace {
  int version() {
    std::cout << egg::ovum::Version() << std::endl;
    return 0;
  }
  void WIBBLE() {
    auto executable = egg::ovum::os::file::getExecutablePath();
    std::cout << "===" << std::endl;
    auto resource = egg::ovum::os::embed::findResourceByName(executable, "PROGBITS", ".comment");
    if (resource != nullptr) {
      std::cout << resource->type << " " << resource->label << " " << resource->bytes << std::endl;
      auto locked = resource->lock();
      if (locked != nullptr) {
        std::cout << "LOCKED '" << std::string((const char*)locked, resource->bytes) << "'" << std::endl;
      }
    }
    std::cout << "===" << std::endl;
  }
}

int main(int argc, char* argv[]) {
  if ((argc == 2) && (std::strcmp(argv[1], "version") == 0)) {
    return version();
  }
  version();
  for (int argi = 0; argi < argc; ++argi) {
    std::cout << argv[argi] << std::endl;
  }
  WIBBLE();
  return 0;
}
