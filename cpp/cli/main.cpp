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
}

int main(int argc, char* argv[]) {
  if ((argc == 2) && (std::strcmp(argv[1], "version") == 0)) {
    return version();
  }
  version();
  for (int argi = 0; argi < argc; ++argi) {
    std::cout << argv[argi] << std::endl;
  }
  return 0;
}
