#ifndef ARGPARSER_H
#define ARGPARSER_H

#include <string>

namespace CLI {
class App;
}

struct Options {
  std::string file;

  void dump(std::ostream &os) const;
};

int parseArguments(int argc, char *argv[], Options &options);

#endif // ARGPARSER_H
