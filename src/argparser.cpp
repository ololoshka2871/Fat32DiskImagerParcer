
#include "CLI/CLI.hpp"

#include "argparser.h"

// const char Options::uncompresed[] = "rawvideo";

static std::string printBool(bool v) { return v ? "YES" : "NO"; }

template <typename T>
static auto newOption(CLI::App &app, const std::string &option_name, T &value,
                      const std::string &description = "") {
  return app
      .add_option(option_name, value,
                  description + " [Default: " + std::to_string(value) + "]")
      ->expected(1);
}

template <typename T>
static std::string generate_default_str(std::string &default_str,
                                        const T &val) {
  return default_str.empty() ? " [Default: " + std::to_string(val) + "]"
                             : " [Default: " + default_str + "]";
}

template <typename T>
static auto newFlag(CLI::App &app, const std::string &flag_name, T &value,
                    const std::string &description = "",
                    std::string default_val = std::string()) {
  return app
      .add_flag(flag_name, value,
                description + generate_default_str(default_val, value))
      ->expected(0);
}

template <>
auto newOption<std::string>(CLI::App &app, const std::string &option_name,
                            std::string &value,
                            const std::string &description) {
  return app
      .add_option(option_name, value, description + " [Default: " + value + "]")
      ->expected(1);
}

template <>
std::string generate_default_str<bool>(std::string &default_str,
                                       const bool &val) {
  return default_str.empty() ? " [Default: " + printBool(val) + "]"
                             : " [Default: " + default_str + "]";
}

static void configureArgumentParcer(CLI::App &app, Options &options) {
  app.add_option("file", options.file, "WAV-file to process.")
      ->expected(1)
      ->required()
      ->check(CLI::ExistingFile);
}

void Options::dump(std::ostream &os) const {
  using namespace std;

  os << "Options:" << endl << "\tInput file: " << file << endl;
}

int parseArguments(int argc, char *argv[], Options &options) {
  CLI::App app{"SampleRateModificator"};
  configureArgumentParcer(app, options);
  CLI11_PARSE(app, argc, argv);
  return 0;
}
