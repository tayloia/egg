#include "yolk/yolk.h"
#include "yolk/program.h"

void egg0::Program::main(const std::vector<std::string>&)
{
  std::ifstream ifs("example.egg");
  if (!ifs.is_open())
  {
    EGG_THROW("Failed to open input file");
  }
  std::string line;
  while (std::getline(ifs, line))
  {
    std::cout << line << std::endl;
  }
}
