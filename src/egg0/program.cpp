#include "egg0/egg0.h"
#include "egg0/program.h"

void egg0::Program::main(const std::vector<std::string>& args)
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
