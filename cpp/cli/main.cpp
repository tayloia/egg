#include "cli/cli.h"
#include "ovum/version.h"
#include "ovum/file.h"
#include "ovum/os-embed.h"
#include "ovum/os-file.h"

#include <iostream>

namespace {
  int version() {
    std::cout << egg::ovum::Version() << std::endl;
    return 0;
  }
  void WIBBLE() {
    auto tmpdir = egg::ovum::os::file::createTemporaryDirectory("WIBBLE-", 100);
    auto cloned = tmpdir + "cloned.exe";
    egg::ovum::os::embed::cloneExecutable(cloned);
    std::cout << "===" << std::endl;
    auto datapath = egg::ovum::File::resolvePath("~/cpp/data/jabberwocky.txt");
    egg::ovum::os::embed::updateResourceFromFile(cloned, "PROGBITS", "JABBERWOCKY", datapath);
    auto resource = egg::ovum::os::embed::findResourceByName(cloned, "PROGBITS", "JABBERWOCKY");
    if (resource != nullptr) {
      std::cout << resource->type << " " << resource->label << " " << resource->bytes << std::endl;
      auto locked = resource->lock();
      if (locked != nullptr) {
        std::cout << "LOCKED '" << std::string((const char*)locked, resource->bytes) << "'" << std::endl;
      }
    }
    egg::ovum::os::embed::updateResourceFromMemory(cloned, "PROGBITS", "JABBERWOCKY", "Hello, world!", 13);
    resource = egg::ovum::os::embed::findResourceByName(cloned, "PROGBITS", "JABBERWOCKY");
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
