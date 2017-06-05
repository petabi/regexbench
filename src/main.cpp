#include "regexbench.h"

using namespace regexbench;

int main(int argc, const char* argv[])
{
  try {
    auto args = regexbench::parse_options(argc, argv);
    return regexbench::exec(args);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
