#include "fortify/systems/kernel/fs_mock_bridge.h"

#include "fortify/systems/logging/log.h"
#include "fortify/systems/subsystems/subsystems.h"

#ifndef FORTIFY_FS_MOCK_READER
#define FORTIFY_FS_MOCK_READER 0
#endif

#ifndef FORTIFY_SHARE_README_BRIDGE
#define FORTIFY_SHARE_README_BRIDGE 0
#endif

#if FORTIFY_SHARE_README_BRIDGE
#include "share_readme_blob.h"
#endif

namespace {
#if FORTIFY_FS_MOCK_READER || FORTIFY_SHARE_README_BRIDGE
void fs_mock_set_le16(uint8_t *bytes, uint64_t offset, uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value & 0xffU);
    bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffU);
}

void fs_mock_set_le32(uint8_t *bytes, uint64_t offset, uint32_t value) {
    bytes[offset] = static_cast<uint8_t>(value & 0xffU);
    bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffU);
    bytes[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xffU);
    bytes[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xffU);
}

void fs_mock_set_literal(uint8_t *bytes, uint64_t offset, const char *literal, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        bytes[offset + i] = static_cast<uint8_t>(literal[i]);
    }
}

void fs_mock_clear_sector(uint8_t *sector) {
    for (uint64_t i = 0; i < 512; i++) {
        sector[i] = 0;
    }
}

void fs_mock_set_directory_entry(uint8_t *entry,
                                 const char *name11,
                                 uint8_t attributes,
                                 uint32_t size,
                                 uint16_t first_cluster) {
    for (uint64_t i = 0; i < 11; i++) {
        entry[i] = static_cast<uint8_t>(name11[i]);
    }
    entry[11] = attributes;
    fs_mock_set_le16(entry, 20, 0);
    fs_mock_set_le16(entry, 26, first_cluster);
    fs_mock_set_le32(entry, 28, size);
}

#if FORTIFY_SHARE_README_BRIDGE
uint32_t fs_mock_share_readme_size(void) {
    const uint64_t raw_size = static_cast<uint64_t>(fortify_share_readme_blob_len);
    return raw_size > 0xffffffffULL ? 0xffffffffU : static_cast<uint32_t>(raw_size);
}

bool fs_mock_copy_share_readme_sector(uint32_t cluster_sector, uint8_t *out_sector) {
    const uint64_t blob_size = static_cast<uint64_t>(fortify_share_readme_blob_len);
    const uint64_t offset = static_cast<uint64_t>(cluster_sector) * 512ULL;
    if (offset >= blob_size) {
        return false;
    }
    const uint64_t remaining = blob_size - offset;
    const uint64_t copy = remaining > 512ULL ? 512ULL : remaining;
    for (uint64_t i = 0; i < copy; i++) {
        out_sector[i] = fortify_share_readme_blob[offset + i];
    }
    return true;
}
#endif

bool mock_filesystem_sector_reader(uint8_t source, uint32_t lba, uint8_t *out_sector) {
    if (out_sector == 0) {
        return false;
    }
    fs_mock_clear_sector(out_sector);

    const uint8_t selector = static_cast<uint8_t>(source % 3);
    if (lba == 0) {
        out_sector[0] = 0xeb;
        out_sector[1] = 0x58;
        out_sector[2] = 0x90;
        fs_mock_set_le16(out_sector, 11, 512);
        out_sector[510] = 0x55;
        out_sector[511] = 0xaa;
        if (selector == 1) {
            out_sector[13] = 8;
            fs_mock_set_le16(out_sector, 14, 32);
            out_sector[16] = 2;
            fs_mock_set_le16(out_sector, 17, 0);
            fs_mock_set_le16(out_sector, 19, 0);
            out_sector[21] = 0xf8;
            fs_mock_set_le16(out_sector, 22, 0);
            fs_mock_set_le32(out_sector, 32, 8192);
            fs_mock_set_le32(out_sector, 36, 32);
            fs_mock_set_le32(out_sector, 44, 2);
            fs_mock_set_literal(out_sector, 82, "FAT32   ", 8);
        } else if (selector == 2) {
            out_sector[13] = 4;
            fs_mock_set_le16(out_sector, 14, 1);
            out_sector[16] = 2;
            fs_mock_set_le16(out_sector, 17, 512);
            fs_mock_set_le16(out_sector, 19, 32768);
            out_sector[21] = 0xf8;
            fs_mock_set_le16(out_sector, 22, 128);
            fs_mock_set_literal(out_sector, 54, "FAT16   ", 8);
        } else {
            out_sector[13] = 8;
            fs_mock_set_literal(out_sector, 3, "NTFS    ", 8);
        }
        return true;
    }

    if (selector != 1) {
        return true;
    }

    constexpr uint32_t reserved = 32;
    constexpr uint32_t fat_count = 2;
    constexpr uint32_t fat_sectors = 32;
    constexpr uint32_t data_start = reserved + fat_count * fat_sectors;
    constexpr uint32_t total_sectors = 8192;
    if (lba >= total_sectors) {
        return false;
    }

    if (lba >= reserved && lba < data_start) {
        const uint32_t fat_sector = (lba - reserved) % fat_sectors;
        for (uint32_t i = 0; i < 128; i++) {
            const uint32_t cluster = fat_sector * 128 + i;
            uint32_t value = 0;
            if (cluster == 0) value = 0x0ffffff8U;
            else if (cluster == 1 || cluster == 2 || cluster == 3 || cluster == 4 || cluster == 5) value = 0x0fffffffU;
            fs_mock_set_le32(out_sector, static_cast<uint64_t>(i) * 4, value);
        }
        return true;
    }

    if (lba < data_start) {
        return true;
    }
    const uint32_t data_lba = lba - data_start;
    const uint32_t cluster = (data_lba / 8) + 2;
    const uint32_t cluster_sector = data_lba % 8;

    if (cluster == 2 && cluster_sector == 0) {
        fs_mock_set_directory_entry(out_sector + 0, "KERNEL  BIN", 0x20, 1536, 3);
        fs_mock_set_directory_entry(out_sector + 32, "CONFIG  TXT", 0x20, 1024, 4);
        fs_mock_set_directory_entry(out_sector + 64, "ASSETS     ", 0x10, 0, 6);
        if ((source & 1U) != 0) {
#if FORTIFY_SHARE_README_BRIDGE
            fs_mock_set_directory_entry(out_sector + 96, "README  TXT", 0x20, fs_mock_share_readme_size(), 5);
#else
            fs_mock_set_directory_entry(out_sector + 96, "README  TXT", 0x20, 640, 5);
#endif
            out_sector[128] = 0x00;
        } else {
            out_sector[96] = 0x00;
        }
        return true;
    }

    if (cluster == 5) {
#if FORTIFY_SHARE_README_BRIDGE
        if (fs_mock_copy_share_readme_sector(cluster_sector, out_sector)) {
            return true;
        }
#endif
        for (uint64_t i = 0; i < 512; i++) {
            out_sector[i] = static_cast<uint8_t>((static_cast<uint64_t>(cluster) + i +
                                                  static_cast<uint64_t>(source) + cluster_sector) & 0xffULL);
        }
        return true;
    }

    if (cluster == 3 || cluster == 4) {
        for (uint64_t i = 0; i < 512; i++) {
            out_sector[i] = static_cast<uint8_t>((static_cast<uint64_t>(cluster) + i +
                                                  static_cast<uint64_t>(source) + cluster_sector) & 0xffULL);
        }
    }
    return true;
}
#endif
} // namespace

void kernel_configure_filesystem_sector_reader_bridge(void) {
#if FORTIFY_FS_MOCK_READER || FORTIFY_SHARE_README_BRIDGE
    filesystem_set_sector_reader(mock_filesystem_sector_reader);
#if FORTIFY_FS_MOCK_READER
    log_warn("filesystem: build-flag mock sector reader enabled.");
#endif
#if FORTIFY_SHARE_README_BRIDGE
    log_warn("filesystem: host-share README bridge enabled (build-time snapshot via sector reader callback).");
#endif
#endif
}
