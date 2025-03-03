#include "yolk/yolk.h"
#include "yolk/stub.h"

int main(int argc, char* argv[], char* envp[]) noexcept {
  return egg::yolk::IStub::main(argc, argv, envp);
}
