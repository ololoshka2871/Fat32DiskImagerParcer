#ifdef _MSC_VER
#include <windows.h>
#else
#include <csignal>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

#include "argparser.h"

#include "mio/mmap.hpp"

#include "WavHeader.h"

static void separator(std::ostream &os) { os << std::endl; }

static bool verify_is_wav(WAVHEADER *h) {
  if (std::memcmp("RIFF", h->chunkId, 4)) {
    return false;
  }
  if (std::memcmp("WAVE", h->format, 4)) {
    return false;
  }
  if (std::memcmp("data", h->subchunk2Id, 4)) {
    return false;
  }

  return true;
}

static bool sample_rate_in_list(WAVHEADER *h) {
  static const uint32_t valid_sample_rates[] = {
      6000,  8000,  11025, 16000, 22050,  32000, 44100,
      48000, 64000, 88200, 96000, 176400, 192000};

  return std::find(std::cbegin(valid_sample_rates),
                   std::cend(valid_sample_rates),
                   h->sampleRate) != std::cend(valid_sample_rates);
}

int main(int argc, char *argv[]) {
  Options options;
  {
    auto ret = parseArguments(argc, argv, options);
    if (ret || options.file.empty()) {
      return ret;
    }
  }

  if (!options.quet) {
    options.dump(std::cout);
    separator(std::cout);
  }

  std::error_code err;
  mio::mmap_sink mapping;
  mapping.map(options.file, 0, sizeof(WAVHEADER), err);
  if (err.value()) {
    std::cerr << "Failed to map file: " << err.message() << std::endl;
    return -1;
  }

  // 1. Is Wav file?
  auto wav_header = reinterpret_cast<WAVHEADER *>(&mapping[0]);
  if (!verify_is_wav(wav_header)) {
    std::cerr << options.file << " is not a Wav file!" << std::endl;
    return -1;
  }

  // 2. Is Sample rate standart?
  if (!sample_rate_in_list(wav_header)) {
    if (!options.quet) {
      std::cout << "Current Sample Rate: " << wav_header->sampleRate
                << " is not standart." << std::endl
                << "Replace with " << options.override_to << " Hz [y/N] ";
      if (options.auto_accept) {
        std::cout << "Y" << std::endl;
      } else {
        auto in = std::cin.get();
        if (!(in == 'y' || in == 'Y')) {
          std::cout << "Canceled." << std::endl;
          return 0;
        }
      }
      std::cout << "Changing Sampling Rate to " << options.override_to
                << std::endl;
    }

    // 3. Update Sample rate
    wav_header->sampleRate = options.override_to;

    // 4. Sync
    mapping.sync(err);
    if (err.value()) {
      std::cerr << "Failed to sync file cache: " << err.message() << std::endl;
      return -1;
    }
  } else {
    if (!options.quet) {
      std::cout << "Sampling Rate " << wav_header->sampleRate
                << " is standart, skip." << std::endl;
      return 0;
    }
  }
}
