#include "yolk/yolk.h"
#include "yolk/stub.h"
#include "ovum/version.h"

#include <iostream>

int egg::yolk::Stub::main() {
  if ((argv.size() == 2) && (argv[1] == "version")) {
    return this->cmdVersion();
  }
  this->cmdVersion();
  for (auto& arg : this->argv) {
    std::cout << arg << std::endl;
  }
  for (auto& env : this->envp) {
    std::cout << env.first << " = " << env.second << std::endl;
  }
  return 0;
}

int egg::yolk::Stub::cmdVersion() {
  std::cout << egg::ovum::Version() << std::endl;
  return 0;
}
