#include "yolk/yolk.h"
#include "yolk/stub.h"

int main(int argc, char* argv[], char* envp[]) noexcept {
  egg::ovum::os::memory::initialize();
  return egg::yolk::IStub::main(argc, argv, envp);
}
