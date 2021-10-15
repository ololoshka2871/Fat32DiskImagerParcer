#ifdef _MSC_VER
#include <windows.h>
#else
#include <csignal>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "progresscpp/ProgressBar.hpp"

#include "argparser.h"

static void separator(std::ostream &os) { os << std::endl; }

int main(int argc, char *argv[]) {
  Options options;
  {
    auto ret = parseArguments(argc, argv, options);
    if (ret) {
      return ret;
    }
  }

  options.dump(std::cout);

  separator(std::cout);

  return 0;
}
