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
#include <sstream>
#include <vector>

#include "argparser.h"

#include "mio/mmap.hpp"

#include "emfat.h"
#include "emfat1.h"

static void separator(std::ostream &os) { os << std::endl; }

template <typename T = uint8_t>
static auto dump_bytes(const T *p, size_t count) {
  std::stringstream ss;

  ss << '[';
  for (size_t i = 0; i < count; ++i) {
    ss << "0x" << std::hex << (uint64_t)p[i];
    if (i < count - 1) {
      ss << ' ';
    }
  }
  ss << ']';

  return ss.str();
}

static auto dump_mbr(const mio::mmap_source &mapping, std::ostream &os) {
  using std::endl;

  auto mbr = reinterpret_cast<const mbr_t *>(&mapping[0]);

  std::vector<uint32_t> res;

  os << "MBR:" << endl
     << ".DiskSig=" << mbr->DiskSig
     << " .BootSignature=" << dump_bytes(mbr->BootSignature, 2) << endl
     << "MBR partitions:" << endl;
  auto p = std::cbegin(mbr->PartTable);
  for (int i = 0; i < 4; ++i, ++p) {
    os << "#:" << i << ":"
       << " .status=" << (int)p->status << " .start(HSC)=("
       << (int)p->start_head << ", " << (int)p->start_sector << ", "
       << (int)p->start_cylinder << ") "
       << " .PartType=" << (int)p->PartType << " .end(HSC)=("
       << (int)p->end_head << ", " << (int)p->end_sector << ", "
       << (int)p->end_cylinder << ") "
       << " .StartLBA=" << p->StartLBA << " .SizeLBA=" << p->SizeLBA << endl;

    if (p->StartLBA != 0) {
      res.emplace_back(p->StartLBA);
    }
  }
  return res;
}

static auto dump_boot_sect(int id, const mio::mmap_source &mapping,
                           uint32_t file_offset, std::ostream &os) {
  using std::endl;

  auto boot_sect = reinterpret_cast<const boot_sector *>(&mapping[0]);

  os << "boot_sector at offset 0x" << std::hex << file_offset << " :"
     << std::dec << endl
     << "\t.jump[JUMP_INS_LEN] = " << dump_bytes(boot_sect->jump, JUMP_INS_LEN)
     << endl
     << "\t.OEM_name[OEM_NAME_LEN] = \""
     << std::string((const char *)boot_sect->OEM_name, OEM_NAME_LEN) << "\""
     << endl
     << "\t.bytes_per_sec = " << boot_sect->bytes_per_sec << endl
     << "\t.sec_per_clus = " << (int)boot_sect->sec_per_clus << endl
     << "\t.reserved_sec_cnt = " << boot_sect->reserved_sec_cnt << endl
     << "\t.fat_cnt = " << (int)boot_sect->fat_cnt << endl
     << "\t.root_dir_max_cnt = " << boot_sect->root_dir_max_cnt << endl
     << "\t.tot_sectors = " << boot_sect->tot_sectors << endl
     << "\t.media_desc = 0x" << std::hex << (int)boot_sect->media_desc
     << std::dec << endl
     << "\t.sec_per_fat_fat16 = " << boot_sect->sec_per_fat_fat16 << endl
     << "\t.sec_per_track = " << boot_sect->sec_per_track << endl
     << "\t.number_of_heads = " << boot_sect->number_of_heads << endl
     << "\t.hidden_sec_cnt = " << boot_sect->hidden_sec_cnt << endl
     << "\t.tol_sector_cnt = " << boot_sect->tol_sector_cnt << endl
     << "\t.sectors_per_fat = " << boot_sect->sectors_per_fat << endl
     << "\t.ext_flags = " << boot_sect->ext_flags << endl
     << "\t.fs_version[FS_VER_LEN] = "
     << dump_bytes(boot_sect->fs_version, FS_VER_LEN) << endl
     << "\t.root_dir_strt_cluster = " << boot_sect->root_dir_strt_cluster
     << endl
     << "\t.fs_info_sector = " << boot_sect->fs_info_sector << endl
     << "\t.backup_boot_sector = " << boot_sect->backup_boot_sector
     << endl
     //<< "\t.reserved[RESERV_LEN] = "
     //<< dump_bytes(boot_sect->reserved, RESERV_LEN) << endl
     << "\t.drive_number = " << dump_bytes(&boot_sect->drive_number, 1) << endl
     << "\t.reserved1 = " << dump_bytes(&boot_sect->reserved1, 1) << endl
     << "\t.boot_sig = " << dump_bytes(&boot_sect->boot_sig, 1) << endl
     << "\t.volume_id[VOL_ID_LEN] = "
     << dump_bytes(boot_sect->volume_id, VOL_ID_LEN) << endl
     << "\t.volume_label[VOL_LABEL_LEN] = \""
     << std::string((const char *)boot_sect->volume_label, VOL_LABEL_LEN)
     << "\"" << endl
     << "\t.file_system_type[FILE_SYS_TYPE_LENGTH] = \""
     << std::string((const char *)boot_sect->file_system_type,
                    FILE_SYS_TYPE_LENGTH)
     << "\"" << endl;

  return boot_sect->reserved_sec_cnt;
}

static void dump_fsinfo(const mio::mmap_source &mapping, uint32_t offset,
                        std::ostream &os) {
  using std::endl;

  auto fsinfo = reinterpret_cast<const fsinfo_t *>(&mapping[0]);

  os << "fsinfo at offset 0x" << std::hex << offset << " :" << std::hex << endl
     << "\t.signature1 = 0x" << fsinfo->signature1 << endl
     << "\t.signature2 = 0x" << fsinfo->signature2 << endl
     << "\t.free_clusters = " << std::dec << fsinfo->free_clusters << endl
     << "\t.next_cluster = " << fsinfo->next_cluster
     << endl
     //<< "\t.reserved2[3] = " << dump_bytes(fsinfo->reserved2, 3) << endl
     << "\t.signature3 = 0x" << std::hex << fsinfo->signature3 << endl
     << std::dec << endl;
}

int main(int argc, char *argv[]) {
  Options options;
  {
    auto ret = parseArguments(argc, argv, options);
    if (ret || options.file.empty()) {
      return ret;
    }
  }

  std::vector<uint32_t> parts;
  {
    std::error_code err;
    mio::mmap_source mapping;
    mapping.map(options.file, 0, sizeof(mbr_t), err);
    if (err.value()) {
      std::cerr << "Failed to map file: " << err.message() << std::endl;
      return -1;
    }
    parts = dump_mbr(mapping, std::cout);
  }
  separator(std::cout);

  int i = 0;
  for (auto &startlba : parts) {
    std::cout << "Partition #" << i << std::endl;

    std::error_code err;
    mio::mmap_source mapping;
    mapping.map(options.file, startlba * SECT, sizeof(boot_sector), err);
    if (err.value()) {
      std::cerr << "Failed to map boot_sector: " << err.message() << std::endl;
      return -1;
    }
    auto reserved_sectors =
        dump_boot_sect(i, mapping, startlba * SECT, std::cout);
    if (reserved_sectors > 0) {
      separator(std::cout);
      mio::mmap_source mapping;
      mapping.map(options.file, (startlba + 1) * SECT, sizeof(fsinfo_t), err);
      if (err.value()) {
        std::cerr << "Failed to map fsinfo_t: " << err.message() << std::endl;
        return -1;
      }
      dump_fsinfo(mapping, (startlba + 1) * SECT, std::cout);
    }

    // TODO
    /*
    {
      separator(std::cout);
      mio::mmap_source mapping;
      mapping.map(options.file, (startlba + reserved_sectors) * SECT, SECT,
                  err);
      if (err.value()) {
        std::cerr << "Failed to map fsinfo_t: " << err.message() << std::endl;
        return -1;
      }
      dump_fsinfo(mapping, std::cout);
    }*/

    separator(std::cout);
    ++i;
  }
}
