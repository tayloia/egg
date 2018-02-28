#include "yolk.h"
#include "program.h"

void egg0::Program::main(const std::vector<std::string>&)
{
  std::ifstream ifs("example.egg");
  if (!ifs.is_open())
  {
    throw std::runtime_error("Failed to open input file");
  }
  std::string line;
  while (std::getline(ifs, line))
  {
    std::cout << line << std::endl;
  }
}
