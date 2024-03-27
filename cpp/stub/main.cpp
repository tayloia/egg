#include "yolk/yolk.h"
#include "yolk/stub.h"

int main(int argc, char* argv[], char* envp[]) {
  egg::yolk::Stub stub(argc, argv, envp);
  return stub.main();
}
