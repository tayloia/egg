#include "yolk/yolk.h"
#include "lexer.h"
#include "program.h"

int main(int argc, char *argv[])
{
  try {
    std::cout << "This is egg" << std::endl;
    auto argn = size_t(argc);
    std::vector<std::string> args(argn);
    for (size_t i = 0; i < argn; ++i) {
      args[i] = argv[i];
    }
    egg0::Program::main(args);
    return 0;
  }
  catch (const std::runtime_error& exception) {
    std::cerr << "Exception caught: " << exception.what() << std::endl;
    return 1;
  }
}
