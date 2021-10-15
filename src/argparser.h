#ifndef ARGPARSER_H
#define ARGPARSER_H

#include <string>

namespace CLI {
class App;
}

struct Options {
  // static const char uncompresed[];

  // std::string codec{uncompresed};
  // bool pal = true;
  // uint32_t crop_top = 0;
  // std::string OutputFile;

  // std::string formatsStr() const { return pal ? "PAL" : "NTSC"; }
  // std::string bitWidthsStr() const { return width14 ? "14 bit" : "16 bit"; }

  // bool Play() const {  }

  bool auto_accept = false;
  bool quet = false;
  uint32_t override_to = 44100;

  std::string file;

  void dump(std::ostream &os) const;
};

int parseArguments(int argc, char *argv[], Options &options);

#endif // ARGPARSER_H
