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

#include "str_trim.h"

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
       << " .PartType=0x" << std::hex << (int)p->PartType << std::dec
       << " .end(HSC)=(" << (int)p->end_head << ", " << (int)p->end_sector
       << ", " << (int)p->end_cylinder << ") "
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
     << "\t.bytes_per_sec = " << boot_sect->bytes_per_sec
     << " # Размер сектора в байтах" << endl
     << "\t.sec_per_clus = " << (int)boot_sect->sec_per_clus << endl
     << "\t.reserved_sec_cnt = " << boot_sect->reserved_sec_cnt
     << " # зарезервированых секторов между началом раздела и первой копией FAT"
     << endl
     << "\t.fat_cnt = " << (int)boot_sect->fat_cnt << endl
     << "\t.root_dir_max_cnt = " << boot_sect->root_dir_max_cnt
     << " # Для FAT32 0, иначе - количество записей в корневом каталоге" << endl
     << "\t.tot_sectors = " << boot_sect->tot_sectors << endl
     << "\t.media_desc = 0x" << std::hex << (int)boot_sect->media_desc
     << std::dec << " # 0xF8 - HDD, 0xF0 - Floppy" << endl
     << "\t.sec_per_fat_fat16 = " << boot_sect->sec_per_fat_fat16 << endl
     << "\t.sec_per_track = " << boot_sect->sec_per_track << endl
     << "\t.number_of_heads = " << boot_sect->number_of_heads << endl
     << "\t.hidden_sec_cnt = " << boot_sect->hidden_sec_cnt
     << " # Число скрытых секторов перед разделом" << endl
     << "\t.tol_sector_cnt = " << boot_sect->tol_sector_cnt
     << " # Всего секторов в разделе" << endl
     << "\t.sectors_per_fat = " << boot_sect->sectors_per_fat
     << " # Cколько секторов занимает 1 копия FAT" << endl
     << "\t.ext_flags = " << boot_sect->ext_flags << endl
     << "\t.fs_version[FS_VER_LEN] = "
     << dump_bytes(boot_sect->fs_version, FS_VER_LEN) << endl

     << "\t.root_dir_strt_cluster = " << boot_sect->root_dir_strt_cluster
     << " # Первый КЛАСТЕР корневого каталога ("
     << boot_sect->root_dir_strt_cluster << "cls *"
     << (int)boot_sect->sec_per_clus << "sec/cls = "
     << boot_sect->root_dir_strt_cluster * (int)boot_sect->sec_per_clus
     << "sec)" << endl

     << "\t.fs_info_sector = " << boot_sect->fs_info_sector
     << " # Сектор, в котором лежит fsinfo (0x" << std::hex << file_offset
     << " + 0x" << boot_sect->bytes_per_sec << " * "
     << boot_sect->fs_info_sector << " = 0x"
     << (file_offset + boot_sect->bytes_per_sec * boot_sect->fs_info_sector)
     << std::dec << endl

     << "\t.backup_boot_sector = " << boot_sect->backup_boot_sector
     << " # Сектор в котором лежит бакап MBR (0 - откл.)"
     << endl

     //<< "\t.reserved[RESERV_LEN] = "
     //<< dump_bytes(boot_sect->reserved, RESERV_LEN) << endl

     << "\t.drive_number = 0x" << std::hex << (int)boot_sect->drive_number
     << std::dec
     << endl

     //<< "\t.reserved1 = " << dump_bytes(&boot_sect->reserved1, 1) << endl
     << "\t.boot_sig = 0x" << std::hex << (int)boot_sect->boot_sig << std::dec
     << endl
     << "\t.volume_id[VOL_ID_LEN] = "
     << dump_bytes(boot_sect->volume_id, VOL_ID_LEN) << endl
     << "\t.volume_label[VOL_LABEL_LEN] = \""
     << std::string((const char *)boot_sect->volume_label, VOL_LABEL_LEN)
     << "\"" << endl
     << "\t.file_system_type[FILE_SYS_TYPE_LENGTH] = \""
     << std::string((const char *)boot_sect->file_system_type,
                    FILE_SYS_TYPE_LENGTH)
     << "\"" << endl
     << endl;

  size_t first_fat_sector = boot_sect->reserved_sec_cnt;

  std::vector<uint32_t> fat_sectors_offsets;
  for (auto i = (uint8_t)0; i < boot_sect->fat_cnt; ++i) {
    auto s = first_fat_sector + boot_sect->sectors_per_fat * i;
    os << "FAT" << i + 1 << " sector: " << s << std::hex << " (offset: 0x"
       << file_offset + s * boot_sect->bytes_per_sec << ")" << std::dec << endl;
    fat_sectors_offsets.emplace_back(s);
  }

  auto root_dir = *fat_sectors_offsets.crbegin() + boot_sect->sectors_per_fat;

  os << "Root dir sector:" << root_dir << std::hex << " (offset: 0x"
     << file_offset + root_dir * boot_sect->bytes_per_sec << ")" << std::dec
     << endl;

  return std::make_tuple(boot_sect->fs_info_sector, fat_sectors_offsets,
                         boot_sect->sectors_per_fat, root_dir);
}

static void dump_fsinfo(const mio::mmap_source &mapping, uint32_t offset,
                        std::ostream &os) {
  using std::endl;

  auto fsinfo = reinterpret_cast<const fsinfo_t *>(&mapping[0]);

  os << "fsinfo at offset 0x" << std::hex << offset << " :" << endl
     << "\t.signature1 = 0x" << fsinfo->signature1 << endl
     << "\t.signature2 = 0x" << fsinfo->signature2 << endl
     << "\t.free_clusters = " << std::dec << fsinfo->free_clusters << endl
     << "\t.next_cluster = " << fsinfo->next_cluster
     << endl
     //<< "\t.reserved2[3] = " << dump_bytes(fsinfo->reserved2, 3) << endl
     << "\t.signature3 = 0x" << std::hex << fsinfo->signature3 << endl
     << std::dec << endl;
}

static void dump_fat(const mio::mmap_source &cluster_chains, uint32_t offset,
                     const mio::mmap_source &data, uint32_t data_offset,
                     std::ostream &os) {
  using std::endl;

  static constexpr uint32_t END = 0x0fffffff;
  static constexpr uint32_t BROCKEN = 0x0ffffff7;

  os << "FAT at offset 0x" << std::hex << offset << " :" << endl;
  os << "Reserved: " << dump_bytes((uint8_t *)&cluster_chains[0], 4) << ", "
     << dump_bytes((uint8_t *)&cluster_chains[4], 4) << ", "
     << dump_bytes((uint8_t *)&cluster_chains[8], 4) << endl;

  const auto cluster_chain_base = (uint32_t *)&cluster_chains[12];

  auto decode_attr = [](uint8_t attr) -> std::string {
    std::stringstream ss;
    if (attr & (1 << 5)) {
      ss << "Archive |";
    }
    if (attr & (1 << 4)) {
      ss << "Dir |";
    }
    if (attr & (1 << 3)) {
      ss << "VolID |";
    }
    if (attr & (1 << 2)) {
      ss << "Sys |";
    }
    if (attr & (1 << 1)) {
      ss << "Hidden |";
    }
    if (attr & (1 << 0)) {
      ss << "| RO";
    }
    return ss.str();
  };

  auto print_file_info = [&os, decode_attr, &cluster_chains,
                          cluster_chain_base](auto f, uint32_t offset,
                                              std::string name) {
    if (f->attr == 0x0f) {
      os << "> Long file record at 0x" << std::hex << offset << std::dec
         << ", skip" << endl;
    } else {
      auto claster = ((uint32_t)f->strt_clus_hword) << 16 | f->strt_clus_lword;
      if (f->name[0] == 0x05) {
        os << "Deleted file ?" << name.erase(0) << " at 0x" << std::hex
           << offset << std::dec << ": " << endl;
      } else {
        os << "File " << name << " at 0x" << std::hex << offset << std::dec
           << ": " << endl;
      }

      os << "\t.attr = " << decode_attr(f->attr) << endl
         << "\t.crt_time_tenth = " << (int)f->crt_time_tenth << endl
         << "\t.crt_time = " << f->crt_time << endl
         << "\t.crt_date = " << f->crt_date << endl
         << "\t.lst_access_date = " << f->lst_access_date << endl
         << "\t.strt_clus_hword = " << f->strt_clus_hword << endl
         << "\t.lst_mod_time = " << f->lst_mod_time << endl
         << "\t.lst_mod_date = " << f->lst_mod_date << endl
         << "\t.strt_clus_lword = " << f->strt_clus_lword << endl
         << "\t.size = " << f->size << endl

         << "\t ->strt_clus = " << claster << endl;

      if (f->attr & (1 << 3)) {
        separator(os);
        return;
      }

      os << "\t> Claster chain: ";
      uint32_t cluster = cluster_chain_base[claster];
      while (true) {
        if (cluster == END) {
          os << "<END>" << endl;
          break;
        }
        if (cluster == BROCKEN) {
          os << "<brocken>" << endl;
          break;
        }

        os << dump_bytes(&cluster, 1) << " -> ";

        cluster = cluster_chain_base[cluster];
      }
    }

    separator(os);
  };

  // files
  auto f = (const dir_entry *)&data[0];

  while (true) {
    if (*f->name == '\0' && *f->extn == '\0') {
      break;
    }

    auto name = trim_copy(std::string((char *)f->name, FILE_NAME_SHRT_LEN));
    auto ext = trim_copy(std::string((char *)f->extn, FILE_NAME_EXTN_LEN));
    if (!ext.empty()) {
      name += "." + ext;
    }

    print_file_info(f, data_offset, name);
    ++f;
    data_offset += sizeof(dir_entry);
  }
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

    auto [fsinfo_sec, fat_offsets, fat_size, root_dir] =
        dump_boot_sect(i, mapping, startlba * SECT, std::cout);

    separator(std::cout);
    mapping.map(options.file, (startlba + fsinfo_sec) * SECT, sizeof(fsinfo_t),
                err);
    if (err.value()) {
      std::cerr << "Failed to map fsinfo_t: " << err.message() << std::endl;
      return -1;
    }
    dump_fsinfo(mapping, (startlba + 1) * SECT, std::cout);

    mio::mmap_source data_mapping;
    data_mapping.map(options.file, (startlba + root_dir) * SECT, SECT * 4, err);
    if (err.value()) {
      std::cerr << "Failed to map data region: " << err.message() << std::endl;
      return -1;
    }

    for (auto offset : fat_offsets) {
      separator(std::cout);
      mapping.map(options.file, (startlba + offset) * SECT, fat_size * SECT,
                  err);
      if (err.value()) {
        std::cerr << "Failed to map FAT at 0x" << std::hex << offset << ":"
                  << err.message() << std::dec << std::endl;
        return -1;
      }
      dump_fat(mapping, (startlba + offset) * SECT, data_mapping,
               (startlba + root_dir) * SECT, std::cout);
    }

    separator(std::cout);
    ++i;
  }
}
