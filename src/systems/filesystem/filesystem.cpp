#include "fortify/systems/logging/log.h"
#include "fortify/systems/memory/memory.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

#ifndef FORTIFY_SHARE_README_BRIDGE
#define FORTIFY_SHARE_README_BRIDGE 0
#endif
#if FORTIFY_SHARE_README_BRIDGE
#include "share_readme_blob.h"
#endif

namespace {
constexpr uint64_t kMaxCpus = 64;
constexpr uint32_t kStorageQueueCapacity = 32;
constexpr uint64_t kStorageBatchPerApJob = 8;
constexpr uint64_t kBootSectorBytes = 512;
constexpr uint64_t kDirectoryEntryBytes = 32;
volatile uint64_t g_maintenance_poll_stride_ticks = 7;
volatile uint64_t g_dirty_pages_trigger = 2;
volatile uint64_t g_ap_deferred_job_total = 0;
volatile uint64_t g_ap_deferred_job_by_cpu[kMaxCpus] = {};
volatile uint64_t g_storage_processed_events_total = 0;
volatile uint64_t g_storage_processed_irq_events_total = 0;
volatile uint64_t g_storage_processed_injected_events_total = 0;
volatile uint64_t g_storage_batches_total = 0;
volatile uint64_t g_storage_irq_total = 0;
volatile uint64_t g_storage_irq_drop_total = 0;
volatile uint64_t g_next_target_ap = 1;
volatile uint64_t g_fs_probe_total = 0;
volatile uint64_t g_fs_probe_fat16_total = 0;
volatile uint64_t g_fs_probe_fat32_total = 0;
volatile uint64_t g_fs_probe_ntfs_total = 0;
volatile uint64_t g_fs_mount_attempt_total = 0;
volatile uint64_t g_fs_mount_success_total = 0;
volatile uint64_t g_fs_fat32_root_scan_total = 0;
volatile uint64_t g_fs_fat32_root_entry_total = 0;
volatile uint64_t g_fs_fat32_root_file_total = 0;
volatile uint64_t g_fs_fat32_root_dir_total = 0;
volatile uint64_t g_fs_fat32_open_attempt_total = 0;
volatile uint64_t g_fs_fat32_open_success_total = 0;
volatile uint64_t g_fs_fat32_read_attempt_total = 0;
volatile uint64_t g_fs_fat32_read_success_total = 0;
volatile uint64_t g_fs_fat32_read_bytes_total = 0;
volatile uint64_t g_fs_sector_read_total = 0;
volatile uint64_t g_fs_sector_read_callback_total = 0;
volatile uint64_t g_fs_sector_read_synthetic_total = 0;
volatile uint64_t g_fs_sector_read_fail_total = 0;
volatile uint64_t g_fs_seen_kinds_mask = 0;
volatile uint64_t g_fs_virtio_9p_probe_total = 0;
volatile uint64_t g_fs_virtio_9p_device_total = 0;
volatile uint64_t g_fs_virtio_9p_last_bus = 0;
volatile uint64_t g_fs_virtio_9p_last_slot = 0;
volatile uint64_t g_fs_virtio_9p_last_function = 0;
volatile uint64_t g_fs_virtio_9p_last_device_id = 0;
volatile uint64_t g_fs_virtio_9p_read_attempt_total = 0;
volatile uint64_t g_fs_virtio_9p_read_success_total = 0;
volatile uint64_t g_fs_virtio_9p_read_done_total = 0;
volatile uint64_t g_fs_virtio_9p_read_done_err_total = 0;
volatile uint64_t g_fs_virtio_9p_share_experimental_mode = 0;
volatile uint64_t g_fs_virtio_9p_done_bridge_ok_total = 0;
volatile uint64_t g_fs_virtio_9p_done_bridge_err_total = 0;
volatile uint64_t g_fs_virtio_9p_done_worker_ok_total = 0;
volatile uint64_t g_fs_virtio_9p_done_worker_err_total = 0;
volatile uint64_t g_fs_virtio_9p_enqueue_worker_total = 0;
volatile uint64_t g_fs_virtio_9p_enqueue_worker_fail_total = 0;
volatile uint64_t g_fs_virtio_9p_mode_blocked_total = 0;
volatile uint32_t g_storage_head_by_cpu[kMaxCpus] = {};
volatile uint32_t g_storage_tail_by_cpu[kMaxCpus] = {};
volatile uint32_t g_storage_queue_high_water_by_cpu[kMaxCpus] = {};
filesystem_sector_reader_t g_sector_reader = 0;

struct storage_event {
    uint8_t source;
};

enum class filesystem_kind : uint8_t {
    unknown = 0,
    fat16 = 1,
    fat32 = 2,
    ntfs = 3,
};

struct fs_probe_result {
    filesystem_kind kind;
    bool mount_supported;
};

struct fat32_volume_geometry {
    bool valid;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t total_sectors;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
};

struct ntfs_volume_geometry {
    bool valid;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint64_t total_sectors;
    uint64_t mft_cluster;
    uint64_t mft_mirror_cluster;
};

struct fat32_root_scan_result {
    bool valid;
    uint64_t entries;
    uint64_t files;
    uint64_t directories;
};

struct fat32_dir_entry_info {
    bool valid;
    bool is_directory;
    uint32_t first_cluster;
    uint32_t size;
};

bool pick_target_ap(uint64_t *out_cpu);
[[maybe_unused]] bool virtio_9p_read_file_head_impl(const char *path,
                                                    uint64_t max_bytes,
                                                    uint64_t *out_bytes,
                                                    uint64_t *out_checksum);

struct virtio_9p_pci_location {
    bool valid;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t device_id;
    uint16_t io_base;
};

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtio_9p_transport {
    bool ready;
    uint16_t io_base;
    uint16_t queue_size;
    uint16_t last_used_idx;
    uint32_t ring_pages;
    uint64_t ring_phys_base;
    uint8_t *ring_virt_base;
    uint64_t request_phys;
    uint8_t *request_virt;
    uint64_t response_phys;
    uint8_t *response_virt;
};

struct fs_virtio_9p_head_request {
    volatile uint64_t state;
    uint64_t request_id;
    char path[64];
    uint64_t max_bytes;
    uint64_t bytes;
    uint64_t checksum;
};

virtio_9p_transport g_virtio_9p_transport = {};
constexpr uint64_t kFsVirtio9pReqQueueCap = 4;
constexpr uint64_t kFsVirtio9pReqStateFree = 0;
constexpr uint64_t kFsVirtio9pReqStatePending = 1;
constexpr uint64_t kFsVirtio9pReqStateRunning = 2;
constexpr uint64_t kFsVirtio9pReqStateDoneOk = 3;
constexpr uint64_t kFsVirtio9pReqStateDoneErr = 4;
constexpr uint64_t kFsVirtio9pReqStateClaimed = 5;
volatile uint64_t g_fs_virtio_9p_req_next_id = 0;
fs_virtio_9p_head_request g_fs_virtio_9p_req_queue[kFsVirtio9pReqQueueCap] = {};

storage_event g_storage_queue_by_cpu[kMaxCpus][kStorageQueueCapacity] = {};

void reset_virtio_9p_transport_state(void) {
    g_virtio_9p_transport.ready = false;
    g_virtio_9p_transport.io_base = 0;
    g_virtio_9p_transport.queue_size = 0;
    g_virtio_9p_transport.last_used_idx = 0;
    g_virtio_9p_transport.ring_pages = 0;
    g_virtio_9p_transport.ring_phys_base = 0;
    g_virtio_9p_transport.ring_virt_base = 0;
    g_virtio_9p_transport.request_phys = 0;
    g_virtio_9p_transport.request_virt = 0;
    g_virtio_9p_transport.response_phys = 0;
    g_virtio_9p_transport.response_virt = 0;
}

bool is_power_of_two_u8(uint8_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

uint16_t load_le16(const uint8_t *bytes, uint64_t offset) {
    return static_cast<uint16_t>(bytes[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

uint16_t load_le16_volatile(const volatile uint8_t *bytes, uint64_t offset) {
    return static_cast<uint16_t>(bytes[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

uint32_t load_le32(const uint8_t *bytes, uint64_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint32_t load_le32_volatile(const volatile uint8_t *bytes, uint64_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint64_t load_le64(const uint8_t *bytes, uint64_t offset) {
    return static_cast<uint64_t>(bytes[offset]) |
           (static_cast<uint64_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint64_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint64_t>(bytes[offset + 3]) << 24) |
           (static_cast<uint64_t>(bytes[offset + 4]) << 32) |
           (static_cast<uint64_t>(bytes[offset + 5]) << 40) |
           (static_cast<uint64_t>(bytes[offset + 6]) << 48) |
           (static_cast<uint64_t>(bytes[offset + 7]) << 56);
}



bool literal_equals(const uint8_t *bytes, uint64_t offset, const char *literal, uint64_t length) {
    if (bytes == 0 || literal == 0) {
        return false;
    }
    for (uint64_t i = 0; i < length; i++) {
        if (bytes[offset + i] != static_cast<uint8_t>(literal[i])) {
            return false;
        }
    }
    return true;
}

void clear_sector(uint8_t *sector) {
    for (uint64_t i = 0; i < kBootSectorBytes; i++) {
        sector[i] = 0;
    }
}

void set_le16(uint8_t *bytes, uint64_t offset, uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value & 0xffU);
    bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffU);
}

void set_le32(uint8_t *bytes, uint64_t offset, uint32_t value) {
    bytes[offset] = static_cast<uint8_t>(value & 0xffU);
    bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffU);
    bytes[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xffU);
    bytes[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xffU);
}

void set_le64(uint8_t *bytes, uint64_t offset, uint64_t value) {
    bytes[offset] = static_cast<uint8_t>(value & 0xffU);
    bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffU);
    bytes[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xffU);
    bytes[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xffU);
    bytes[offset + 4] = static_cast<uint8_t>((value >> 32) & 0xffU);
    bytes[offset + 5] = static_cast<uint8_t>((value >> 40) & 0xffU);
    bytes[offset + 6] = static_cast<uint8_t>((value >> 48) & 0xffU);
    bytes[offset + 7] = static_cast<uint8_t>((value >> 56) & 0xffU);
}

void set_literal(uint8_t *bytes, uint64_t offset, const char *literal, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        bytes[offset + i] = static_cast<uint8_t>(literal[i]);
    }
}

void io_out32(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

void io_out16(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

void io_out8(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t io_in32(uint16_t port) {
    uint32_t value = 0;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint16_t io_in16(uint16_t port) {
    uint16_t value = 0;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint8_t io_in8(uint16_t port) {
    uint8_t value = 0;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t pci_config_read_u32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    const uint32_t address = 0x80000000U |
                             (static_cast<uint32_t>(bus) << 16) |
                             (static_cast<uint32_t>(slot) << 11) |
                             (static_cast<uint32_t>(function) << 8) |
                             (static_cast<uint32_t>(offset) & 0xfcU);
    io_out32(0xCF8U, address);
    return io_in32(0xCFCU);
}

bool is_virtio_9p_device_id(uint16_t vendor_id, uint16_t device_id) {
    if (vendor_id != 0x1AF4U) {
        return false;
    }
    return device_id == 0x1009U || device_id == 0x1049U;
}

void pci_config_write_u32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    const uint32_t address = 0x80000000U |
                             (static_cast<uint32_t>(bus) << 16) |
                             (static_cast<uint32_t>(slot) << 11) |
                             (static_cast<uint32_t>(function) << 8) |
                             (static_cast<uint32_t>(offset) & 0xfcU);
    io_out32(0xCF8U, address);
    io_out32(0xCFCU, value);
}

bool find_virtio_9p_pci_device(virtio_9p_pci_location *out_location) {
    if (out_location == 0) {
        return false;
    }
    out_location->valid = false;
    for (uint16_t bus = 0; bus <= 0xffU; bus++) {
        for (uint16_t slot = 0; slot < 32U; slot++) {
            for (uint16_t function = 0; function < 8U; function++) {
                const uint32_t id = pci_config_read_u32(static_cast<uint8_t>(bus),
                                                        static_cast<uint8_t>(slot),
                                                        static_cast<uint8_t>(function),
                                                        0x00U);
                const uint16_t vendor_id = static_cast<uint16_t>(id & 0xffffU);
                if (vendor_id == 0xffffU) {
                    continue;
                }
                const uint16_t device_id = static_cast<uint16_t>((id >> 16) & 0xffffU);
                if (!is_virtio_9p_device_id(vendor_id, device_id)) {
                    continue;
                }
                const uint32_t bar0 = pci_config_read_u32(static_cast<uint8_t>(bus),
                                                          static_cast<uint8_t>(slot),
                                                          static_cast<uint8_t>(function),
                                                          0x10U);
                if ((bar0 & 0x1U) == 0) {
                    continue;
                }
                out_location->valid = true;
                out_location->bus = static_cast<uint8_t>(bus);
                out_location->slot = static_cast<uint8_t>(slot);
                out_location->function = static_cast<uint8_t>(function);
                out_location->device_id = device_id;
                out_location->io_base = static_cast<uint16_t>(bar0 & 0xfffcU);
                return true;
            }
        }
    }
    return false;
}

uint64_t probe_virtio_9p_devices_impl(void) {
    uint64_t detected = 0;
    uint64_t last_bus = 0;
    uint64_t last_slot = 0;
    uint64_t last_function = 0;
    uint64_t last_device_id = 0;

    for (uint16_t bus = 0; bus <= 0xffU; bus++) {
        for (uint16_t slot = 0; slot < 32U; slot++) {
            for (uint16_t function = 0; function < 8U; function++) {
                const uint32_t id = pci_config_read_u32(static_cast<uint8_t>(bus),
                                                        static_cast<uint8_t>(slot),
                                                        static_cast<uint8_t>(function),
                                                        0x00U);
                const uint16_t vendor_id = static_cast<uint16_t>(id & 0xffffU);
                if (vendor_id == 0xffffU) {
                    continue;
                }
                const uint16_t device_id = static_cast<uint16_t>((id >> 16) & 0xffffU);
                if (!is_virtio_9p_device_id(vendor_id, device_id)) {
                    continue;
                }
                detected++;
                last_bus = bus;
                last_slot = slot;
                last_function = function;
                last_device_id = device_id;
            }
        }
    }

    __atomic_fetch_add(&g_fs_virtio_9p_probe_total, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_device_total, detected, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_last_bus, last_bus, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_last_slot, last_slot, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_last_function, last_function, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_last_device_id, last_device_id, __ATOMIC_RELAXED);
    return detected;
}

uint64_t align_up_u64(uint64_t value, uint64_t alignment) {
    if (alignment == 0) {
        return value;
    }
    const uint64_t rem = value % alignment;
    return rem == 0 ? value : value + (alignment - rem);
}

uint64_t virtq_ring_bytes(uint16_t queue_size) {
    const uint64_t desc_bytes = static_cast<uint64_t>(sizeof(virtq_desc)) * queue_size;
    const uint64_t avail_bytes = 6ULL + (2ULL * queue_size);
    const uint64_t used_offset = align_up_u64(desc_bytes + avail_bytes, 4096ULL);
    const uint64_t used_bytes = 6ULL + (8ULL * queue_size);
    return used_offset + used_bytes;
}

bool alloc_contiguous_frames(uint32_t page_count, uint64_t *out_phys_base) {
    if (page_count == 0 || out_phys_base == 0) {
        return false;
    }
    if (page_count > 4) {
        return false;
    }
    for (uint32_t attempt = 0; attempt < 32; attempt++) {
        uint64_t frames[4] = {};
        bool ok = true;
        for (uint32_t i = 0; i < page_count; i++) {
            if (!pmm_alloc_frame(&frames[i])) {
                ok = false;
                break;
            }
            if (i > 0 && frames[i] != frames[0] + (static_cast<uint64_t>(i) * 4096ULL)) {
                ok = false;
            }
        }
        if (ok) {
            *out_phys_base = frames[0];
            return true;
        }
        for (uint32_t i = 0; i < page_count; i++) {
            if (frames[i] != 0) {
                pmm_free_frame(frames[i]);
            }
        }
    }
    return false;
}

bool transport_write_status(uint16_t io_base, uint8_t status) {
    io_out8(static_cast<uint16_t>(io_base + 0x12U), status);
    return io_in8(static_cast<uint16_t>(io_base + 0x12U)) == status;
}

bool virtio_9p_transport_init(void) {
    if (g_virtio_9p_transport.ready) {
        return true;
    }
    if (!pmm_ready()) {
        return false;
    }
    virtio_9p_pci_location location;
    location.valid = false;
    location.bus = 0;
    location.slot = 0;
    location.function = 0;
    location.device_id = 0;
    location.io_base = 0;
    if (!find_virtio_9p_pci_device(&location)) {
        return false;
    }

    uint32_t command = pci_config_read_u32(location.bus, location.slot, location.function, 0x04U);
    command |= 0x00000005U;  // I/O space + bus master.
    pci_config_write_u32(location.bus, location.slot, location.function, 0x04U, command);

    const uint16_t io_base = location.io_base;
    transport_write_status(io_base, 0);
    if (!transport_write_status(io_base, 0x01U)) {
        return false;
    }
    if (!transport_write_status(io_base, 0x03U)) {
        return false;
    }

    io_out32(static_cast<uint16_t>(io_base + 0x04U), 0U);
    if (!transport_write_status(io_base, 0x0BU)) {
        return false;
    }
    if ((io_in8(static_cast<uint16_t>(io_base + 0x12U)) & 0x08U) == 0) {
        return false;
    }

    io_out16(static_cast<uint16_t>(io_base + 0x0EU), 0U);
    const uint16_t queue_size = io_in16(static_cast<uint16_t>(io_base + 0x0CU));
    if (queue_size < 2U) {
        return false;
    }

    const uint64_t ring_bytes = virtq_ring_bytes(queue_size);
    const uint32_t ring_pages = static_cast<uint32_t>(align_up_u64(ring_bytes, 4096ULL) / 4096ULL);
    uint64_t ring_phys = 0;
    if (!alloc_contiguous_frames(ring_pages, &ring_phys)) {
        return false;
    }
    const uint64_t hhdm = vmm_hhdm_offset();
    uint8_t *ring_virt = reinterpret_cast<uint8_t *>(hhdm + ring_phys);
    for (uint64_t i = 0; i < static_cast<uint64_t>(ring_pages) * 4096ULL; i++) {
        ring_virt[i] = 0;
    }

    uint64_t request_phys = 0;
    uint64_t response_phys = 0;
    if (!pmm_alloc_frame(&request_phys) || !pmm_alloc_frame(&response_phys)) {
        return false;
    }
    uint8_t *request_virt = reinterpret_cast<uint8_t *>(hhdm + request_phys);
    uint8_t *response_virt = reinterpret_cast<uint8_t *>(hhdm + response_phys);
    for (uint64_t i = 0; i < 4096; i++) {
        request_virt[i] = 0;
        response_virt[i] = 0;
    }

    io_out32(static_cast<uint16_t>(io_base + 0x08U), static_cast<uint32_t>(ring_phys >> 12));
    if (io_in32(static_cast<uint16_t>(io_base + 0x08U)) == 0) {
        return false;
    }
    if (!transport_write_status(io_base, 0x0FU)) {
        return false;
    }

    g_virtio_9p_transport.ready = true;
    g_virtio_9p_transport.io_base = io_base;
    g_virtio_9p_transport.queue_size = queue_size;
    g_virtio_9p_transport.last_used_idx = 0;
    g_virtio_9p_transport.ring_pages = ring_pages;
    g_virtio_9p_transport.ring_phys_base = ring_phys;
    g_virtio_9p_transport.ring_virt_base = ring_virt;
    g_virtio_9p_transport.request_phys = request_phys;
    g_virtio_9p_transport.request_virt = request_virt;
    g_virtio_9p_transport.response_phys = response_phys;
    g_virtio_9p_transport.response_virt = response_virt;
    return true;
}

bool append_u8(uint8_t *buffer, uint64_t max_len, uint64_t *cursor, uint8_t value) {
    if (buffer == 0 || cursor == 0 || *cursor + 1 > max_len) {
        return false;
    }
    buffer[*cursor] = value;
    *cursor += 1;
    return true;
}

bool append_u16(uint8_t *buffer, uint64_t max_len, uint64_t *cursor, uint16_t value) {
    if (buffer == 0 || cursor == 0 || *cursor + 2 > max_len) {
        return false;
    }
    set_le16(buffer, *cursor, value);
    *cursor += 2;
    return true;
}

bool append_u32(uint8_t *buffer, uint64_t max_len, uint64_t *cursor, uint32_t value) {
    if (buffer == 0 || cursor == 0 || *cursor + 4 > max_len) {
        return false;
    }
    set_le32(buffer, *cursor, value);
    *cursor += 4;
    return true;
}

bool append_string(uint8_t *buffer, uint64_t max_len, uint64_t *cursor, const char *text) {
    if (buffer == 0 || cursor == 0 || text == 0) {
        return false;
    }
    uint16_t len = 0;
    while (text[len] != '\0') {
        len++;
    }
    if (!append_u16(buffer, max_len, cursor, len)) {
        return false;
    }
    for (uint16_t i = 0; i < len; i++) {
        if (!append_u8(buffer, max_len, cursor, static_cast<uint8_t>(text[i]))) {
            return false;
        }
    }
    return true;
}

bool copy_ascii_path(char *out_path, uint64_t out_size, const char *path) {
    if (out_path == 0 || out_size < 2 || path == 0 || path[0] == '\0') {
        return false;
    }
    uint64_t i = 0;
    for (; path[i] != '\0'; i++) {
        const unsigned char ch = static_cast<unsigned char>(path[i]);
        if (ch < 0x21 || ch > 0x7e || ch == ' ') {
            return false;
        }
        if (i + 1 >= out_size) {
            return false;
        }
        out_path[i] = static_cast<char>(ch);
    }
    out_path[i] = '\0';
    return i != 0;
}

#if FORTIFY_QEMU_SHARE_ENABLE && FORTIFY_SHARE_README_BRIDGE
bool bridge_readme_head(uint64_t max_bytes, uint64_t *out_bytes, uint64_t *out_checksum) {
    if (out_bytes == 0) {
        return false;
    }
    *out_bytes = 0;
    if (out_checksum != 0) {
        *out_checksum = 0;
    }
    const uint64_t blob_len = static_cast<uint64_t>(fortify_share_readme_blob_len);
    if (blob_len == 0) {
        return false;
    }
    uint64_t bytes = blob_len;
    if (bytes > max_bytes) {
        bytes = max_bytes;
    }
    uint64_t checksum = 0;
    for (uint64_t i = 0; i < bytes; i++) {
        checksum += static_cast<uint64_t>(fortify_share_readme_blob[i]);
    }
    *out_bytes = bytes;
    if (out_checksum != 0) {
        *out_checksum = checksum;
    }
    return true;
}
#endif

void clear_virtio_9p_request_queue(void) {
    for (uint64_t i = 0; i < kFsVirtio9pReqQueueCap; i++) {
        __atomic_store_n(&g_fs_virtio_9p_req_queue[i].state, kFsVirtio9pReqStateFree, __ATOMIC_RELAXED);
        g_fs_virtio_9p_req_queue[i].request_id = 0;
        g_fs_virtio_9p_req_queue[i].path[0] = '\0';
        g_fs_virtio_9p_req_queue[i].max_bytes = 0;
        g_fs_virtio_9p_req_queue[i].bytes = 0;
        g_fs_virtio_9p_req_queue[i].checksum = 0;
    }
}

bool schedule_virtio_9p_request_worker(void) {
    __atomic_fetch_add(&g_fs_virtio_9p_enqueue_worker_total, 1, __ATOMIC_RELAXED);
    uint64_t target_cpu = 0;
    if (!pick_target_ap(&target_cpu)) {
        __atomic_fetch_add(&g_fs_virtio_9p_enqueue_worker_fail_total, 1, __ATOMIC_RELAXED);
        return false;
    }
    if (!scheduler_enqueue_ap_maintenance_job(target_cpu)) {
        __atomic_fetch_add(&g_fs_virtio_9p_enqueue_worker_fail_total, 1, __ATOMIC_RELAXED);
        return false;
    }
    return true;
}

void complete_virtio_9p_request(uint64_t slot,
                                bool ok,
                                uint64_t bytes,
                                uint64_t checksum,
                                bool worker_path,
                                bool bridge_path) {
    g_fs_virtio_9p_req_queue[slot].bytes = bytes;
    g_fs_virtio_9p_req_queue[slot].checksum = checksum;
    if (ok) {
        __atomic_fetch_add(&g_fs_virtio_9p_read_success_total, 1, __ATOMIC_RELAXED);
        if (worker_path) {
            __atomic_fetch_add(&g_fs_virtio_9p_done_worker_ok_total, 1, __ATOMIC_RELAXED);
        }
        if (bridge_path) {
            __atomic_fetch_add(&g_fs_virtio_9p_done_bridge_ok_total, 1, __ATOMIC_RELAXED);
        }
    } else {
        __atomic_fetch_add(&g_fs_virtio_9p_read_done_err_total, 1, __ATOMIC_RELAXED);
        if (worker_path) {
            __atomic_fetch_add(&g_fs_virtio_9p_done_worker_err_total, 1, __ATOMIC_RELAXED);
        }
        if (bridge_path) {
            __atomic_fetch_add(&g_fs_virtio_9p_done_bridge_err_total, 1, __ATOMIC_RELAXED);
        }
    }
    __atomic_fetch_add(&g_fs_virtio_9p_read_done_total, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_req_queue[slot].state,
                     ok ? kFsVirtio9pReqStateDoneOk : kFsVirtio9pReqStateDoneErr,
                     __ATOMIC_RELEASE);
}

void process_virtio_9p_head_requests_on_worker(void) {
    for (uint64_t i = 0; i < kFsVirtio9pReqQueueCap; i++) {
        uint64_t expected = kFsVirtio9pReqStatePending;
        if (!__atomic_compare_exchange_n(&g_fs_virtio_9p_req_queue[i].state,
                                         &expected,
                                         kFsVirtio9pReqStateRunning,
                                         false,
                                         __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE)) {
            continue;
        }

        uint64_t bytes = 0;
        uint64_t checksum = 0;
        const bool ok = virtio_9p_read_file_head_impl(g_fs_virtio_9p_req_queue[i].path,
                                                      g_fs_virtio_9p_req_queue[i].max_bytes,
                                                      &bytes,
                                                      &checksum);
        complete_virtio_9p_request(i, ok, bytes, checksum, true, false);
        return;
    }
}

bool virtio_9p_submit_request(const uint8_t *request, uint32_t request_len,
                              uint8_t *response, uint32_t response_cap,
                              uint32_t *out_response_len) {
    if (!g_virtio_9p_transport.ready || request == 0 || request_len == 0 || response == 0 ||
        response_cap < 7 || out_response_len == 0) {
        return false;
    }

    for (uint32_t i = 0; i < request_len && i < 4096U; i++) {
        g_virtio_9p_transport.request_virt[i] = request[i];
    }
    for (uint32_t i = 0; i < response_cap && i < 4096U; i++) {
        g_virtio_9p_transport.response_virt[i] = 0;
    }

    virtq_desc *descs = reinterpret_cast<virtq_desc *>(g_virtio_9p_transport.ring_virt_base);
    uint8_t *avail_base = g_virtio_9p_transport.ring_virt_base +
                          (sizeof(virtq_desc) * g_virtio_9p_transport.queue_size);
    uint64_t used_offset = align_up_u64((sizeof(virtq_desc) * g_virtio_9p_transport.queue_size) +
                                            (6ULL + (2ULL * g_virtio_9p_transport.queue_size)),
                                        4096ULL);
    volatile uint8_t *used_base = g_virtio_9p_transport.ring_virt_base + used_offset;

    descs[0].addr = g_virtio_9p_transport.request_phys;
    descs[0].len = request_len;
    descs[0].flags = 1U;
    descs[0].next = 1U;

    descs[1].addr = g_virtio_9p_transport.response_phys;
    descs[1].len = response_cap > 4096U ? 4096U : response_cap;
    descs[1].flags = 2U;
    descs[1].next = 0U;

    uint16_t avail_idx = load_le16(avail_base, 2);
    set_le16(avail_base, 4 + (2 * (avail_idx % g_virtio_9p_transport.queue_size)), 0U);
    set_le16(avail_base, 2, static_cast<uint16_t>(avail_idx + 1U));
    __asm__ volatile ("" : : : "memory");
    io_out16(static_cast<uint16_t>(g_virtio_9p_transport.io_base + 0x10U), 0U);

    const uint16_t expected = static_cast<uint16_t>(g_virtio_9p_transport.last_used_idx + 1U);
    const uint64_t start_tick = timer_ticks();
    const uint64_t timeout_ticks = 200;  // 2 seconds at 100 Hz PIT.
    uint32_t spin_limit = 200000;
    while (load_le16_volatile(used_base, 2) != expected) {
        if ((timer_ticks() - start_tick) > timeout_ticks || spin_limit-- == 0) {
            return false;
        }
        __asm__ volatile ("pause");
    }
    g_virtio_9p_transport.last_used_idx = expected;
    const uint32_t used_len =
        load_le32_volatile(used_base, 4 + ((expected - 1U) % g_virtio_9p_transport.queue_size) * 8ULL + 4ULL);
    const uint32_t copy_len = used_len > response_cap ? response_cap : used_len;
    for (uint32_t i = 0; i < copy_len; i++) {
        response[i] = g_virtio_9p_transport.response_virt[i];
    }
    *out_response_len = copy_len;
    return copy_len >= 7;
}

bool virtio_9p_send_simple_request(const uint8_t *request,
                                   uint32_t request_len,
                                   uint8_t expected_type,
                                   uint16_t expected_tag,
                                   uint8_t *response,
                                   uint32_t response_cap,
                                   uint32_t *out_response_len) {
    uint32_t response_len = 0;
    if (!virtio_9p_submit_request(request, request_len, response, response_cap, &response_len)) {
        return false;
    }
    const uint32_t packet_size = load_le32(response, 0);
    if (packet_size < 7 || packet_size > response_len) {
        return false;
    }
    const uint8_t type = response[4];
    const uint16_t tag = load_le16(response, 5);
    if (tag != expected_tag) {
        return false;
    }
    if (type == 107U || type == 7U) {
        return false;
    }
    if (type != expected_type) {
        return false;
    }
    *out_response_len = packet_size;
    return true;
}

bool virtio_9p_negotiate_session(uint16_t *out_fid) {
    uint8_t request[512];
    uint8_t response[4096];
    for (uint64_t i = 0; i < sizeof(request); i++) {
        request[i] = 0;
    }
    for (uint64_t i = 0; i < sizeof(response); i++) {
        response[i] = 0;
    }
    uint64_t cursor = 7;
    const uint16_t tag_version = 1;
    set_le32(request, 0, 0);
    request[4] = 100U;
    set_le16(request, 5, tag_version);
    if (!append_u32(request, sizeof(request), &cursor, 65536U) ||
        !append_string(request, sizeof(request), &cursor, "9P2000.L")) {
        return false;
    }
    set_le32(request, 0, static_cast<uint32_t>(cursor));

    uint32_t response_len = 0;
    if (!virtio_9p_send_simple_request(request, static_cast<uint32_t>(cursor), 101U, tag_version,
                                       response, sizeof(response), &response_len)) {
        return false;
    }

    cursor = 7;
    const uint16_t tag_attach = 2;
    const uint32_t root_fid = 1;
    set_le32(request, 0, 0);
    request[4] = 104U;
    set_le16(request, 5, tag_attach);
    if (!append_u32(request, sizeof(request), &cursor, root_fid) ||
        !append_u32(request, sizeof(request), &cursor, 0xffffffffU) ||
        !append_string(request, sizeof(request), &cursor, "fortify") ||
        !append_string(request, sizeof(request), &cursor, "") ||
        !append_u32(request, sizeof(request), &cursor, 0xffffffffU)) {
        return false;
    }
    set_le32(request, 0, static_cast<uint32_t>(cursor));
    if (!virtio_9p_send_simple_request(request, static_cast<uint32_t>(cursor), 105U, tag_attach,
                                       response, sizeof(response), &response_len)) {
        return false;
    }
    *out_fid = static_cast<uint16_t>(root_fid);
    return true;
}

[[maybe_unused]] bool virtio_9p_read_file_head_impl(const char *path,
                                                    uint64_t max_bytes,
                                                    uint64_t *out_bytes,
                                                    uint64_t *out_checksum) {
    if (path == 0 || path[0] == '\0' || max_bytes == 0 || out_bytes == 0) {
        return false;
    }
    *out_bytes = 0;
    if (out_checksum != 0) {
        *out_checksum = 0;
    }
    if (max_bytes > 2048) {
        max_bytes = 2048;
    }
    if (!virtio_9p_transport_init()) {
        return false;
    }

    uint16_t root_fid = 0;
    if (!virtio_9p_negotiate_session(&root_fid)) {
        return false;
    }

    uint8_t request[512];
    uint8_t response[4096];
    for (uint64_t i = 0; i < sizeof(request); i++) {
        request[i] = 0;
    }
    for (uint64_t i = 0; i < sizeof(response); i++) {
        response[i] = 0;
    }
    uint32_t response_len = 0;
    uint64_t cursor = 7;
    const uint16_t tag_walk = 3;
    const uint32_t file_fid = 2;
    set_le32(request, 0, 0);
    request[4] = 110U;
    set_le16(request, 5, tag_walk);
    if (!append_u32(request, sizeof(request), &cursor, root_fid) ||
        !append_u32(request, sizeof(request), &cursor, file_fid) ||
        !append_u16(request, sizeof(request), &cursor, 1U) ||
        !append_string(request, sizeof(request), &cursor, path)) {
        return false;
    }
    set_le32(request, 0, static_cast<uint32_t>(cursor));
    if (!virtio_9p_send_simple_request(request, static_cast<uint32_t>(cursor), 111U, tag_walk,
                                       response, sizeof(response), &response_len)) {
        return false;
    }
    if (load_le16(response, 7) == 0) {
        return false;
    }

    cursor = 7;
    const uint16_t tag_open = 4;
    set_le32(request, 0, 0);
    request[4] = 112U;
    set_le16(request, 5, tag_open);
    if (!append_u32(request, sizeof(request), &cursor, file_fid) ||
        !append_u8(request, sizeof(request), &cursor, 0U)) {
        return false;
    }
    set_le32(request, 0, static_cast<uint32_t>(cursor));
    if (!virtio_9p_send_simple_request(request, static_cast<uint32_t>(cursor), 113U, tag_open,
                                       response, sizeof(response), &response_len)) {
        return false;
    }

    cursor = 7;
    const uint16_t tag_read = 5;
    set_le32(request, 0, 0);
    request[4] = 116U;
    set_le16(request, 5, tag_read);
    if (!append_u32(request, sizeof(request), &cursor, file_fid) ||
        !append_u32(request, sizeof(request), &cursor, 0U) ||
        !append_u32(request, sizeof(request), &cursor, 0U) ||
        !append_u32(request, sizeof(request), &cursor, static_cast<uint32_t>(max_bytes))) {
        return false;
    }
    set_le32(request, 0, static_cast<uint32_t>(cursor));
    if (!virtio_9p_send_simple_request(request, static_cast<uint32_t>(cursor), 117U, tag_read,
                                       response, sizeof(response), &response_len)) {
        return false;
    }

    const uint32_t payload = load_le32(response, 7);
    if (payload == 0 || 11U + payload > response_len) {
        return false;
    }

    uint64_t checksum = 0;
    for (uint32_t i = 0; i < payload; i++) {
        checksum += response[11U + i];
    }
    *out_bytes = payload;
    if (out_checksum != 0) {
        *out_checksum = checksum;
    }

    cursor = 7;
    const uint16_t tag_clunk = 6;
    set_le32(request, 0, 0);
    request[4] = 120U;
    set_le16(request, 5, tag_clunk);
    if (!append_u32(request, sizeof(request), &cursor, file_fid)) {
        return true;
    }
    set_le32(request, 0, static_cast<uint32_t>(cursor));
    (void)virtio_9p_send_simple_request(request, static_cast<uint32_t>(cursor), 121U, tag_clunk,
                                        response, sizeof(response), &response_len);
    return true;
}

void build_sample_boot_sector(uint8_t source, uint8_t *sector) {
    clear_sector(sector);
    sector[0] = 0xeb;
    sector[1] = 0x58;
    sector[2] = 0x90;
    set_le16(sector, 11, 512);
    sector[510] = 0x55;
    sector[511] = 0xaa;

    const uint8_t selector = static_cast<uint8_t>(source % 3);
    if (selector == 1) {
        // FAT32 sample.
        sector[13] = 8;
        set_le16(sector, 14, 32);
        sector[16] = 2;
        set_le16(sector, 17, 0);
        set_le16(sector, 19, 0);
        sector[21] = 0xf8;
        set_le16(sector, 22, 0);
        set_le32(sector, 32, 8192);
        set_le32(sector, 36, 32);
        set_le32(sector, 44, 2);
        set_literal(sector, 82, "FAT32   ", 8);
        return;
    }
    if (selector == 2) {
        // FAT16 sample.
        sector[13] = 4;
        set_le16(sector, 14, 1);
        sector[16] = 2;
        set_le16(sector, 17, 512);
        set_le16(sector, 19, 32768);
        sector[21] = 0xf8;
        set_le16(sector, 22, 128);
        set_literal(sector, 54, "FAT16   ", 8);
        return;
    }
    // NTFS sample.
    sector[13] = 8;
    set_literal(sector, 3, "NTFS    ", 8);
    set_le64(sector, 40, 262144);
    set_le64(sector, 48, 4);
    set_le64(sector, 56, 8);
    sector[64] = 0xF6;
    sector[68] = 1;
}

ntfs_volume_geometry parse_ntfs_geometry(const uint8_t *sector) {
    ntfs_volume_geometry geometry = {};
    if (sector == 0) {
        return geometry;
    }
    if (sector[510] != 0x55 || sector[511] != 0xaa) {
        return geometry;
    }
    if (!literal_equals(sector, 3, "NTFS    ", 8)) {
        return geometry;
    }
    const uint16_t bytes_per_sector = load_le16(sector, 11);
    const uint8_t sectors_per_cluster = sector[13];
    const uint64_t total_sectors = load_le64(sector, 40);
    const uint64_t mft_cluster = load_le64(sector, 48);
    const uint64_t mft_mirror_cluster = load_le64(sector, 56);
    if ((bytes_per_sector != 512 && bytes_per_sector != 1024 &&
         bytes_per_sector != 2048 && bytes_per_sector != 4096) ||
        !is_power_of_two_u8(sectors_per_cluster) ||
        total_sectors == 0 || mft_cluster < 2 || mft_mirror_cluster < 2) {
        return geometry;
    }
    geometry.valid = true;
    geometry.bytes_per_sector = bytes_per_sector;
    geometry.sectors_per_cluster = sectors_per_cluster;
    geometry.total_sectors = total_sectors;
    geometry.mft_cluster = mft_cluster;
    geometry.mft_mirror_cluster = mft_mirror_cluster;
    return geometry;
}

fs_probe_result probe_boot_sector(const uint8_t *sector) {
    if (sector == 0) {
        return {filesystem_kind::unknown, false};
    }
    if (sector[510] != 0x55 || sector[511] != 0xaa) {
        return {filesystem_kind::unknown, false};
    }

    const uint16_t bytes_per_sector = load_le16(sector, 11);
    const uint8_t sectors_per_cluster = sector[13];
    if (bytes_per_sector != 512 && bytes_per_sector != 1024 &&
        bytes_per_sector != 2048 && bytes_per_sector != 4096) {
        return {filesystem_kind::unknown, false};
    }

    if (literal_equals(sector, 3, "NTFS    ", 8)) {
        const ntfs_volume_geometry ntfs = parse_ntfs_geometry(sector);
        return {filesystem_kind::ntfs, ntfs.valid};
    }

    const uint16_t reserved_sectors = load_le16(sector, 14);
    const uint8_t fats = sector[16];
    const uint16_t root_entries = load_le16(sector, 17);
    const uint16_t sectors_per_fat16 = load_le16(sector, 22);
    const uint32_t sectors_per_fat32 = load_le32(sector, 36);

    if (!is_power_of_two_u8(sectors_per_cluster) || reserved_sectors == 0 || fats == 0) {
        return {filesystem_kind::unknown, false};
    }
    if (root_entries == 0 && sectors_per_fat16 == 0 && sectors_per_fat32 != 0) {
        return {filesystem_kind::fat32, true};
    }
    if (root_entries != 0 && sectors_per_fat16 != 0) {
        return {filesystem_kind::fat16, true};
    }
    return {filesystem_kind::unknown, false};
}

fat32_volume_geometry parse_fat32_geometry(const uint8_t *sector) {
    fat32_volume_geometry geometry = {};
    if (sector == 0) {
        return geometry;
    }
    if (sector[510] != 0x55 || sector[511] != 0xaa) {
        return geometry;
    }
    const uint16_t bytes_per_sector = load_le16(sector, 11);
    const uint8_t sectors_per_cluster = sector[13];
    const uint16_t reserved_sectors = load_le16(sector, 14);
    const uint8_t fat_count = sector[16];
    const uint16_t root_entries = load_le16(sector, 17);
    const uint16_t sectors_per_fat16 = load_le16(sector, 22);
    const uint32_t total16 = static_cast<uint32_t>(load_le16(sector, 19));
    const uint32_t total32 = load_le32(sector, 32);
    const uint32_t sectors_per_fat = load_le32(sector, 36);
    const uint32_t root_cluster = load_le32(sector, 44);

    if (bytes_per_sector != kBootSectorBytes || !is_power_of_two_u8(sectors_per_cluster) ||
        reserved_sectors == 0 || fat_count == 0 || root_entries != 0 ||
        sectors_per_fat16 != 0 || sectors_per_fat == 0 || root_cluster < 2 ||
        !literal_equals(sector, 82, "FAT32   ", 8)) {
        return geometry;
    }

    geometry.valid = true;
    geometry.bytes_per_sector = bytes_per_sector;
    geometry.sectors_per_cluster = sectors_per_cluster;
    geometry.reserved_sectors = reserved_sectors;
    geometry.fat_count = fat_count;
    geometry.sectors_per_fat = sectors_per_fat;
    geometry.root_cluster = root_cluster;
    geometry.total_sectors = (total32 != 0) ? total32 : total16;
    geometry.fat_start_lba = reserved_sectors;
    geometry.data_start_lba = reserved_sectors + static_cast<uint32_t>(fat_count) * sectors_per_fat;
    return geometry;
}

void clear_bytes(uint8_t *bytes, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        bytes[i] = 0;
    }
}

void set_directory_entry(uint8_t *entry, const char *name11, uint8_t attributes, uint32_t size);

bool synthetic_block_read_sector(uint8_t source, uint32_t lba, uint8_t *out_sector) {
    if (out_sector == 0) {
        return false;
    }
    clear_sector(out_sector);
    uint8_t boot_sector[kBootSectorBytes];
    build_sample_boot_sector(source, boot_sector);
    if (lba == 0) {
        for (uint64_t i = 0; i < kBootSectorBytes; i++) {
            out_sector[i] = boot_sector[i];
        }
        return true;
    }

    const fat32_volume_geometry geometry = parse_fat32_geometry(boot_sector);
    if (!geometry.valid || lba >= geometry.total_sectors) {
        return false;
    }

    const uint32_t fat_span = geometry.fat_count * geometry.sectors_per_fat;
    if (lba >= geometry.fat_start_lba && lba < geometry.fat_start_lba + fat_span) {
        const uint32_t fat_sector = (lba - geometry.fat_start_lba) % geometry.sectors_per_fat;
        for (uint32_t i = 0; i < (kBootSectorBytes / 4); i++) {
            const uint32_t cluster = fat_sector * (kBootSectorBytes / 4) + i;
            uint32_t value = 0;
            if (cluster == 0) value = 0x0ffffff8U;
            else if (cluster == 1 || cluster == 2 || cluster == 3 || cluster == 4 || cluster == 5) value = 0x0fffffffU;
            set_le32(out_sector, static_cast<uint64_t>(i) * 4, value);
        }
        return true;
    }

    if (lba < geometry.data_start_lba) {
        return true;
    }
    const uint32_t data_lba = lba - geometry.data_start_lba;
    const uint32_t cluster = (data_lba / geometry.sectors_per_cluster) + 2;
    const uint32_t cluster_sector = data_lba % geometry.sectors_per_cluster;

    if (cluster == 2 && cluster_sector == 0) {
        set_directory_entry(out_sector + (0 * kDirectoryEntryBytes), "KERNEL  BIN", 0x20, 1536);
        set_directory_entry(out_sector + (1 * kDirectoryEntryBytes), "CONFIG  TXT", 0x20, 1024);
        set_directory_entry(out_sector + (2 * kDirectoryEntryBytes), "ASSETS     ", 0x10, 0);
        if ((source & 1U) != 0) {
            set_directory_entry(out_sector + (3 * kDirectoryEntryBytes), "README  TXT", 0x20, 640);
            out_sector[4 * kDirectoryEntryBytes] = 0x00;
        } else {
            out_sector[3 * kDirectoryEntryBytes] = 0x00;
        }
        return true;
    }

    if (cluster == 3 || cluster == 5) {
        for (uint64_t i = 0; i < kBootSectorBytes; i++) {
            out_sector[i] = static_cast<uint8_t>((static_cast<uint64_t>(cluster) + i +
                                                  static_cast<uint64_t>(source) + cluster_sector) & 0xffULL);
        }
        return true;
    }
    return true;
}

bool filesystem_block_read_sector(uint8_t source, uint32_t lba, uint8_t *out_sector) {
    __atomic_fetch_add(&g_fs_sector_read_total, 1, __ATOMIC_RELAXED);
    if (out_sector == 0) {
        __atomic_fetch_add(&g_fs_sector_read_fail_total, 1, __ATOMIC_RELAXED);
        return false;
    }

    filesystem_sector_reader_t reader = g_sector_reader;
    if (reader != 0) {
        if (reader(source, lba, out_sector)) {
            __atomic_fetch_add(&g_fs_sector_read_callback_total, 1, __ATOMIC_RELAXED);
            const uint64_t old_mask = __atomic_fetch_or(&g_fs_seen_kinds_mask, 1ULL << 5, __ATOMIC_RELAXED);
            if ((old_mask & (1ULL << 5)) == 0) {
                log_info("filesystem: external sector reader path active.");
            }
            return true;
        }
        __atomic_fetch_add(&g_fs_sector_read_fail_total, 1, __ATOMIC_RELAXED);
        return false;
    }

    if (!synthetic_block_read_sector(source, lba, out_sector)) {
        __atomic_fetch_add(&g_fs_sector_read_fail_total, 1, __ATOMIC_RELAXED);
        return false;
    }
    __atomic_fetch_add(&g_fs_sector_read_synthetic_total, 1, __ATOMIC_RELAXED);
    return true;
}

void set_directory_entry(uint8_t *entry, const char *name11, uint8_t attributes, uint32_t size) {
    for (uint64_t i = 0; i < 11; i++) {
        entry[i] = static_cast<uint8_t>(name11[i]);
    }
    entry[11] = attributes;
    set_le16(entry, 20, 0);
    set_le16(entry, 26, 3);
    set_le32(entry, 28, size);
}

fat32_root_scan_result scan_fat32_root_directory(const uint8_t *cluster, uint64_t cluster_bytes) {
    fat32_root_scan_result result = {};
    if (cluster == 0 || cluster_bytes < kDirectoryEntryBytes) {
        return result;
    }
    if ((cluster_bytes % kDirectoryEntryBytes) != 0) {
        return result;
    }
    result.valid = true;
    const uint64_t entry_count = cluster_bytes / kDirectoryEntryBytes;
    for (uint64_t i = 0; i < entry_count; i++) {
        const uint8_t *entry = cluster + (i * kDirectoryEntryBytes);
        const uint8_t first = entry[0];
        if (first == 0x00) {
            break;
        }
        if (first == 0xe5) {
            continue;
        }
        const uint8_t attributes = entry[11];
        if ((attributes & 0x0fU) == 0x0fU) {
            continue;
        }
        result.entries++;
        if ((attributes & 0x10U) != 0) {
            result.directories++;
        } else {
            result.files++;
        }
    }
    return result;
}

bool directory_entry_name_equals(const uint8_t *entry, const char *name11) {
    if (entry == 0 || name11 == 0) {
        return false;
    }
    for (uint64_t i = 0; i < 11; i++) {
        if (entry[i] != static_cast<uint8_t>(name11[i])) {
            return false;
        }
    }
    return true;
}

fat32_dir_entry_info find_fat32_root_entry(const uint8_t *cluster,
                                           uint64_t cluster_bytes,
                                           const char *name11) {
    fat32_dir_entry_info result = {};
    if (cluster == 0 || name11 == 0 || cluster_bytes < kDirectoryEntryBytes ||
        (cluster_bytes % kDirectoryEntryBytes) != 0) {
        return result;
    }
    const uint64_t entry_count = cluster_bytes / kDirectoryEntryBytes;
    for (uint64_t i = 0; i < entry_count; i++) {
        const uint8_t *entry = cluster + (i * kDirectoryEntryBytes);
        const uint8_t first = entry[0];
        if (first == 0x00) {
            break;
        }
        if (first == 0xe5) {
            continue;
        }
        const uint8_t attributes = entry[11];
        if ((attributes & 0x0fU) == 0x0fU) {
            continue;
        }
        if (!directory_entry_name_equals(entry, name11)) {
            continue;
        }
        result.valid = true;
        result.is_directory = (attributes & 0x10U) != 0;
        const uint32_t cluster_hi = static_cast<uint32_t>(load_le16(entry, 20));
        const uint32_t cluster_lo = static_cast<uint32_t>(load_le16(entry, 26));
        result.first_cluster = (cluster_hi << 16) | cluster_lo;
        result.size = load_le32(entry, 28);
        return result;
    }
    return result;
}

uint64_t read_fat32_file_head(const fat32_dir_entry_info &entry,
                              const fat32_volume_geometry &geometry,
                              uint8_t source,
                              uint64_t max_read,
                              uint64_t *out_checksum) {
    if (!entry.valid || entry.is_directory || entry.first_cluster < 2 || !geometry.valid) {
        return 0;
    }
    const uint64_t bounded_max_read = (max_read > 256) ? 256 : max_read;
    const uint64_t length = (entry.size < bounded_max_read) ? static_cast<uint64_t>(entry.size) : bounded_max_read;
    if (length == 0) {
        return 0;
    }

    const uint64_t cluster_bytes = static_cast<uint64_t>(geometry.bytes_per_sector) *
                                   static_cast<uint64_t>(geometry.sectors_per_cluster);
    const uint64_t readable = (length < cluster_bytes) ? length : cluster_bytes;
    if (readable == 0) {
        return 0;
    }
    uint8_t scratch[256];
    uint64_t copied = 0;
    const uint32_t cluster_lba_base =
        geometry.data_start_lba + (entry.first_cluster - 2U) * static_cast<uint32_t>(geometry.sectors_per_cluster);
    for (uint32_t sector = 0; sector < geometry.sectors_per_cluster && copied < readable; sector++) {
        uint8_t sector_buf[kBootSectorBytes];
        if (!filesystem_block_read_sector(source, cluster_lba_base + sector, sector_buf)) {
            break;
        }
        uint64_t to_copy = readable - copied;
        if (to_copy > kBootSectorBytes) {
            to_copy = kBootSectorBytes;
        }
        for (uint64_t i = 0; i < to_copy; i++) {
            scratch[copied + i] = sector_buf[i];
        }
        copied += to_copy;
    }
    if (copied == 0) {
        return 0;
    }
    uint64_t checksum = 0;
    for (uint64_t i = 0; i < copied; i++) {
        checksum += scratch[i];
    }
    if (out_checksum != 0) {
        *out_checksum = checksum;
    }
    return (checksum == 0 && copied != 0) ? 0 : copied;
}

uint8_t to_upper_ascii(uint8_t ch) {
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<uint8_t>(ch - ('a' - 'A'));
    }
    return ch;
}

void format_short_name_from_entry(const uint8_t *entry, char *out_name, uint64_t out_size) {
    if (entry == 0 || out_name == 0 || out_size == 0) {
        return;
    }
    uint64_t out = 0;
    for (uint64_t i = 0; i < 8 && out + 1 < out_size; i++) {
        const uint8_t ch = entry[i];
        if (ch == ' ') {
            continue;
        }
        out_name[out++] = static_cast<char>(ch);
    }
    bool has_ext = false;
    for (uint64_t i = 8; i < 11; i++) {
        if (entry[i] != ' ') {
            has_ext = true;
            break;
        }
    }
    if (has_ext && out + 1 < out_size) {
        out_name[out++] = '.';
        for (uint64_t i = 8; i < 11 && out + 1 < out_size; i++) {
            const uint8_t ch = entry[i];
            if (ch == ' ') {
                continue;
            }
            out_name[out++] = static_cast<char>(ch);
        }
    }
    out_name[out] = '\0';
}

bool normalize_name_8dot3_to_entry11(const char *name_8dot3, char *out_name11) {
    if (name_8dot3 == 0 || out_name11 == 0 || name_8dot3[0] == '\0') {
        return false;
    }
    for (uint64_t i = 0; i < 11; i++) {
        out_name11[i] = ' ';
    }

    uint64_t input = 0;
    uint64_t base = 0;
    while (name_8dot3[input] != '\0' && name_8dot3[input] != '.') {
        const uint8_t ch = to_upper_ascii(static_cast<uint8_t>(name_8dot3[input]));
        if ((ch < 'A' || ch > 'Z') && (ch < '0' || ch > '9') && ch != '_') {
            return false;
        }
        if (base >= 8) {
            return false;
        }
        out_name11[base++] = static_cast<char>(ch);
        input++;
    }
    if (base == 0) {
        return false;
    }
    if (name_8dot3[input] == '\0') {
        return true;
    }

    input++;
    uint64_t ext = 0;
    while (name_8dot3[input] != '\0') {
        const uint8_t ch = to_upper_ascii(static_cast<uint8_t>(name_8dot3[input]));
        if ((ch < 'A' || ch > 'Z') && (ch < '0' || ch > '9') && ch != '_') {
            return false;
        }
        if (ext >= 3) {
            return false;
        }
        out_name11[8 + ext] = static_cast<char>(ch);
        ext++;
        input++;
    }
    return ext != 0;
}

bool load_fat32_root_cluster(uint8_t source,
                             fat32_volume_geometry *out_geometry,
                             uint8_t *out_cluster,
                             uint64_t max_cluster_bytes,
                             uint64_t *out_cluster_bytes) {
    if (out_geometry == 0 || out_cluster == 0 || out_cluster_bytes == 0) {
        return false;
    }
    uint8_t boot_sector[kBootSectorBytes];
    if (!filesystem_block_read_sector(source, 0, boot_sector)) {
        return false;
    }
    const fs_probe_result probe = probe_boot_sector(boot_sector);
    if (probe.kind != filesystem_kind::fat32 || !probe.mount_supported) {
        return false;
    }

    const fat32_volume_geometry geometry = parse_fat32_geometry(boot_sector);
    if (!geometry.valid) {
        return false;
    }
    const uint64_t cluster_bytes =
        static_cast<uint64_t>(geometry.bytes_per_sector) * static_cast<uint64_t>(geometry.sectors_per_cluster);
    if (cluster_bytes < kDirectoryEntryBytes || cluster_bytes > max_cluster_bytes) {
        return false;
    }

    clear_bytes(out_cluster, cluster_bytes);
    const uint32_t root_lba = geometry.data_start_lba +
                              (geometry.root_cluster - 2U) * static_cast<uint32_t>(geometry.sectors_per_cluster);
    for (uint32_t sector_index = 0; sector_index < geometry.sectors_per_cluster; sector_index++) {
        uint8_t sector_buf[kBootSectorBytes];
        if (!filesystem_block_read_sector(source, root_lba + sector_index, sector_buf)) {
            return false;
        }
        const uint64_t base = static_cast<uint64_t>(sector_index) * kBootSectorBytes;
        for (uint64_t i = 0; i < kBootSectorBytes; i++) {
            out_cluster[base + i] = sector_buf[i];
        }
    }

    *out_geometry = geometry;
    *out_cluster_bytes = cluster_bytes;
    return true;
}

void maybe_log_first_seen(filesystem_kind kind) {
    uint64_t kind_bit = 0;
    const char *message = 0;
    if (kind == filesystem_kind::fat16) {
        kind_bit = 1ULL << 1;
        message = "filesystem: FAT16 boot sector detected.";
    } else if (kind == filesystem_kind::fat32) {
        kind_bit = 1ULL << 2;
        message = "filesystem: FAT32 boot sector detected.";
    } else if (kind == filesystem_kind::ntfs) {
        kind_bit = 1ULL << 3;
        message = "filesystem: NTFS boot sector detected.";
    } else {
        return;
    }
    const uint64_t old_mask = __atomic_fetch_or(&g_fs_seen_kinds_mask, kind_bit, __ATOMIC_RELAXED);
    if ((old_mask & kind_bit) == 0 && message != 0) {
        log_info(message);
    }
}

void process_storage_event(const storage_event &event) {
    uint8_t sector[kBootSectorBytes];
    if (!filesystem_block_read_sector(event.source, 0, sector)) {
        return;
    }
    const fs_probe_result probe = probe_boot_sector(sector);
    if (probe.kind == filesystem_kind::unknown) {
        return;
    }

    __atomic_fetch_add(&g_fs_probe_total, 1, __ATOMIC_RELAXED);
    maybe_log_first_seen(probe.kind);
    if (probe.kind == filesystem_kind::fat16) {
        __atomic_fetch_add(&g_fs_probe_fat16_total, 1, __ATOMIC_RELAXED);
    } else if (probe.kind == filesystem_kind::fat32) {
        __atomic_fetch_add(&g_fs_probe_fat32_total, 1, __ATOMIC_RELAXED);
    } else if (probe.kind == filesystem_kind::ntfs) {
        __atomic_fetch_add(&g_fs_probe_ntfs_total, 1, __ATOMIC_RELAXED);
    }

    if (!probe.mount_supported) {
        return;
    }
    __atomic_fetch_add(&g_fs_mount_attempt_total, 1, __ATOMIC_RELAXED);
    if (probe.kind == filesystem_kind::ntfs) {
        const ntfs_volume_geometry ntfs = parse_ntfs_geometry(sector);
        if (!ntfs.valid) {
            return;
        }
        __atomic_fetch_add(&g_fs_mount_success_total, 1, __ATOMIC_RELAXED);
        return;
    }
    if (probe.kind == filesystem_kind::fat16) {
        __atomic_fetch_add(&g_fs_mount_success_total, 1, __ATOMIC_RELAXED);
        return;
    }
    if (probe.kind != filesystem_kind::fat32) {
        return;
    }
    __atomic_fetch_add(&g_fs_mount_success_total, 1, __ATOMIC_RELAXED);

    fat32_volume_geometry geometry = {};
    uint8_t root_cluster[4096];
    uint64_t cluster_bytes = 0;
    if (!load_fat32_root_cluster(event.source, &geometry, root_cluster, sizeof(root_cluster), &cluster_bytes)) {
        return;
    }

    const fat32_root_scan_result scan = scan_fat32_root_directory(root_cluster, cluster_bytes);
    if (!scan.valid) {
        return;
    }
    __atomic_fetch_add(&g_fs_fat32_root_scan_total, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_fs_fat32_root_entry_total, scan.entries, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_fs_fat32_root_file_total, scan.files, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_fs_fat32_root_dir_total, scan.directories, __ATOMIC_RELAXED);

    __atomic_fetch_add(&g_fs_fat32_open_attempt_total, 1, __ATOMIC_RELAXED);
    const fat32_dir_entry_info file = find_fat32_root_entry(root_cluster, cluster_bytes, "KERNEL  BIN");
    if (!file.valid || file.is_directory) {
        return;
    }
    __atomic_fetch_add(&g_fs_fat32_open_success_total, 1, __ATOMIC_RELAXED);
    const uint64_t old_mask = __atomic_fetch_or(&g_fs_seen_kinds_mask, 1ULL << 4, __ATOMIC_RELAXED);
    if ((old_mask & (1ULL << 4)) == 0) {
        log_info("filesystem: FAT32 root open/read scaffold active for KERNEL.BIN.");
    }

    __atomic_fetch_add(&g_fs_fat32_read_attempt_total, 1, __ATOMIC_RELAXED);
    const uint64_t read_bytes = read_fat32_file_head(file, geometry, event.source, 256, 0);
    if (read_bytes == 0) {
        return;
    }
    __atomic_fetch_add(&g_fs_fat32_read_success_total, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_fs_fat32_read_bytes_total, read_bytes, __ATOMIC_RELAXED);
}

bool fat32_list_root_impl(uint8_t source,
                          filesystem_root_entry *out_entries,
                          uint64_t max_entries,
                          uint64_t *out_count) {
    if (out_count == 0 || out_entries == 0 || max_entries == 0) {
        return false;
    }
    *out_count = 0;

    fat32_volume_geometry geometry = {};
    uint8_t root_cluster[4096];
    uint64_t cluster_bytes = 0;
    if (!load_fat32_root_cluster(source, &geometry, root_cluster, sizeof(root_cluster), &cluster_bytes)) {
        return false;
    }
    (void)geometry;

    const uint64_t entry_count = cluster_bytes / kDirectoryEntryBytes;
    uint64_t out_index = 0;
    for (uint64_t i = 0; i < entry_count; i++) {
        if (out_index >= max_entries) {
            break;
        }
        const uint8_t *entry = root_cluster + (i * kDirectoryEntryBytes);
        const uint8_t first = entry[0];
        if (first == 0x00) {
            break;
        }
        if (first == 0xe5) {
            continue;
        }
        const uint8_t attributes = entry[11];
        if ((attributes & 0x0fU) == 0x0fU) {
            continue;
        }
        format_short_name_from_entry(entry, out_entries[out_index].name_8dot3,
                                     sizeof(out_entries[out_index].name_8dot3));
        out_entries[out_index].is_directory = (attributes & 0x10U) != 0;
        out_entries[out_index].size_bytes = load_le32(entry, 28);
        out_index++;
    }
    *out_count = out_index;
    return true;
}

bool fat32_read_root_file_head_impl(uint8_t source,
                                    const char *name_8dot3,
                                    uint64_t max_bytes,
                                    uint64_t *out_bytes,
                                    uint64_t *out_checksum) {
    if (name_8dot3 == 0 || out_bytes == 0 || max_bytes == 0) {
        return false;
    }
    *out_bytes = 0;
    if (out_checksum != 0) {
        *out_checksum = 0;
    }

    char name11[11];
    if (!normalize_name_8dot3_to_entry11(name_8dot3, name11)) {
        return false;
    }

    fat32_volume_geometry geometry = {};
    uint8_t root_cluster[4096];
    uint64_t cluster_bytes = 0;
    if (!load_fat32_root_cluster(source, &geometry, root_cluster, sizeof(root_cluster), &cluster_bytes)) {
        return false;
    }

    const fat32_dir_entry_info file = find_fat32_root_entry(root_cluster, cluster_bytes, name11);
    if (!file.valid || file.is_directory) {
        return false;
    }

    const uint64_t read_bytes = read_fat32_file_head(file, geometry, source, max_bytes, out_checksum);
    if (read_bytes == 0) {
        return false;
    }
    *out_bytes = read_bytes;
    return true;
}

bool pick_target_ap(uint64_t *out_cpu) {
    if (out_cpu == 0) {
        return false;
    }
    const uint64_t total = smp_total_cpu_count();
    if (total <= 1) {
        return false;
    }
    uint64_t next = __atomic_load_n(&g_next_target_ap, __ATOMIC_RELAXED);
    if (next >= total || next == 0) {
        next = 1;
    }
    *out_cpu = next;
    next++;
    if (next >= total) {
        next = 1;
    }
    __atomic_store_n(&g_next_target_ap, next, __ATOMIC_RELAXED);
    return true;
}

uint32_t queue_depth_for_cpu(uint64_t cpu_index) {
    const uint32_t head = __atomic_load_n(&g_storage_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_storage_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (tail >= head) {
        return tail - head;
    }
    return (kStorageQueueCapacity - head) + tail;
}

bool enqueue_storage_event(uint64_t cpu_index, uint8_t source) {
    if (cpu_index >= kMaxCpus) {
        return false;
    }
    const uint32_t tail = __atomic_load_n(&g_storage_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t next_tail = (tail + 1) % kStorageQueueCapacity;
    const uint32_t head = __atomic_load_n(&g_storage_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (next_tail == head) {
        return false;
    }
    g_storage_queue_by_cpu[cpu_index][tail].source = source;
    __atomic_store_n(&g_storage_tail_by_cpu[cpu_index], next_tail, __ATOMIC_RELAXED);
    const uint32_t depth_after_enqueue = queue_depth_for_cpu(cpu_index);
    const uint32_t high_water = __atomic_load_n(&g_storage_queue_high_water_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (depth_after_enqueue > high_water) {
        __atomic_store_n(&g_storage_queue_high_water_by_cpu[cpu_index], depth_after_enqueue, __ATOMIC_RELAXED);
    }
    return true;
}

bool dequeue_storage_event(uint64_t cpu_index, storage_event *out_event) {
    if (cpu_index >= kMaxCpus || out_event == 0) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_storage_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_storage_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (head == tail) {
        return false;
    }
    *out_event = g_storage_queue_by_cpu[cpu_index][head];
    __atomic_store_n(&g_storage_head_by_cpu[cpu_index], (head + 1) % kStorageQueueCapacity, __ATOMIC_RELAXED);
    return true;
}
}

bool filesystem_fat32_list_root(uint8_t source,
                                filesystem_root_entry *out_entries,
                                uint64_t max_entries,
                                uint64_t *out_count) {
    return fat32_list_root_impl(source, out_entries, max_entries, out_count);
}

bool filesystem_fat32_read_root_file_head(uint8_t source,
                                          const char *name_8dot3,
                                          uint64_t max_bytes,
                                          uint64_t *out_bytes,
                                          uint64_t *out_checksum) {
    return fat32_read_root_file_head_impl(source, name_8dot3, max_bytes, out_bytes, out_checksum);
}

bool filesystem_init(void) {
    __atomic_store_n(&g_ap_deferred_job_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_storage_processed_events_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_storage_processed_irq_events_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_storage_processed_injected_events_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_storage_batches_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_storage_irq_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_storage_irq_drop_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_next_target_ap, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_probe_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_probe_fat16_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_probe_fat32_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_probe_ntfs_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_mount_attempt_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_mount_success_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_root_scan_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_root_entry_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_root_file_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_root_dir_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_open_attempt_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_open_success_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_read_attempt_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_read_success_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_fat32_read_bytes_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_sector_read_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_sector_read_callback_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_sector_read_synthetic_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_sector_read_fail_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_seen_kinds_mask, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_probe_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_device_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_last_bus, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_last_slot, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_last_function, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_last_device_id, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_read_attempt_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_read_success_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_read_done_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_read_done_err_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_share_experimental_mode, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_done_bridge_ok_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_done_bridge_err_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_done_worker_ok_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_done_worker_err_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_enqueue_worker_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_enqueue_worker_fail_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_mode_blocked_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_fs_virtio_9p_req_next_id, 0, __ATOMIC_RELAXED);
    reset_virtio_9p_transport_state();
    clear_virtio_9p_request_queue();
    for (uint64_t i = 0; i < kMaxCpus; i++) {
        __atomic_store_n(&g_ap_deferred_job_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_storage_head_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_storage_tail_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_storage_queue_high_water_by_cpu[i], 0, __ATOMIC_RELAXED);
    }
    log_info("filesystem: storage IRQ queue + AP deferred processing ready.");
    log_info("filesystem: FAT16/FAT32/NTFS mount probe scaffold enabled.");
    log_info("filesystem: FAT32 root traversal now uses sector-backed read path.");
    log_info(filesystem_has_sector_reader()
                 ? "filesystem: using external sector reader callback."
                 : "filesystem: using synthetic sector reader fallback.");
    const uint64_t virtio_9p_devices = probe_virtio_9p_devices_impl();
    log_info(virtio_9p_devices > 0
                 ? "filesystem: virtio-9p PCI device detected."
                 : "filesystem: virtio-9p PCI device not detected.");
    return true;
}

void filesystem_on_storage_irq(void) {
    __atomic_fetch_add(&g_storage_irq_total, 1, __ATOMIC_RELAXED);
    uint64_t target_cpu = 0;
    if (!pick_target_ap(&target_cpu) || !enqueue_storage_event(target_cpu, 1)) {
        __atomic_fetch_add(&g_storage_irq_drop_total, 1, __ATOMIC_RELAXED);
    }
}

uint64_t filesystem_inject_storage_events(uint64_t count) {
    uint64_t injected = 0;
    for (uint64_t i = 0; i < count; i++) {
        __atomic_fetch_add(&g_storage_irq_total, 1, __ATOMIC_RELAXED);
        uint64_t target_cpu = 0;
        const uint8_t source = static_cast<uint8_t>(i % 3);
        if (!pick_target_ap(&target_cpu) || !enqueue_storage_event(target_cpu, source)) {
            __atomic_fetch_add(&g_storage_irq_drop_total, 1, __ATOMIC_RELAXED);
            continue;
        }
        injected++;
    }
    return injected;
}

bool filesystem_request_ap_maintenance(uint64_t cpu_index) {
    return scheduler_enqueue_ap_maintenance_job(cpu_index);
}

bool filesystem_poll_ap_maintenance_trigger(uint64_t cpu_index, uint64_t tick) {
    if (cpu_index >= kMaxCpus) {
        return false;
    }
    const uint64_t stride = __atomic_load_n(&g_maintenance_poll_stride_ticks, __ATOMIC_RELAXED);
    if (stride == 0 || (tick % stride) != 0) {
        return false;
    }
    const uint64_t trigger = __atomic_load_n(&g_dirty_pages_trigger, __ATOMIC_RELAXED);
    if (trigger == 0 || queue_depth_for_cpu(cpu_index) < trigger) {
        return false;
    }
    return filesystem_request_ap_maintenance(cpu_index);
}

bool filesystem_set_ap_maintenance_trigger_config(uint64_t poll_stride_ticks, uint64_t dirty_pages_trigger) {
    if (poll_stride_ticks == 0 || dirty_pages_trigger == 0) {
        return false;
    }
    __atomic_store_n(&g_maintenance_poll_stride_ticks, poll_stride_ticks, __ATOMIC_RELAXED);
    __atomic_store_n(&g_dirty_pages_trigger, dirty_pages_trigger, __ATOMIC_RELAXED);
    return true;
}

void filesystem_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence) {
    (void)sequence;
    if (cpu_index >= kMaxCpus) {
        return;
    }
#if FORTIFY_QEMU_SHARE_ENABLE
    if (filesystem_virtio_9p_share_experimental_mode_enabled()) {
        process_virtio_9p_head_requests_on_worker();
    }
#else
    process_virtio_9p_head_requests_on_worker();
#endif
    uint64_t processed = 0;
    uint64_t processed_irq = 0;
    uint64_t processed_injected = 0;
    for (uint64_t i = 0; i < kStorageBatchPerApJob; i++) {
        storage_event event = {};
        if (!dequeue_storage_event(cpu_index, &event)) {
            break;
        }
        if (event.source == 1) {
            processed_irq++;
        } else {
            processed_injected++;
        }
        process_storage_event(event);
        processed++;
    }
    if (processed == 0) {
        return;
    }
    __atomic_fetch_add(&g_storage_processed_events_total, processed, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_storage_processed_irq_events_total, processed_irq, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_storage_processed_injected_events_total, processed_injected, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_storage_batches_total, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_ap_deferred_job_total, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_ap_deferred_job_by_cpu[cpu_index], 1, __ATOMIC_RELAXED);
}

uint64_t filesystem_ap_deferred_job_total(void) {
    return __atomic_load_n(&g_ap_deferred_job_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_ap_deferred_job_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_deferred_job_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t filesystem_storage_irq_total(void) {
    return __atomic_load_n(&g_storage_irq_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_storage_irq_drop_total(void) {
    return __atomic_load_n(&g_storage_irq_drop_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_queue_depth_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return static_cast<uint64_t>(queue_depth_for_cpu(cpu_index));
}

uint64_t filesystem_queue_high_water_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return static_cast<uint64_t>(__atomic_load_n(&g_storage_queue_high_water_by_cpu[cpu_index], __ATOMIC_RELAXED));
}

uint64_t filesystem_queue_high_water_total(void) {
    uint64_t total = smp_total_cpu_count();
    if (total > kMaxCpus) {
        total = kMaxCpus;
    }
    uint64_t high_water_total = 0;
    for (uint64_t cpu = 1; cpu < total; cpu++) {
        high_water_total += static_cast<uint64_t>(__atomic_load_n(&g_storage_queue_high_water_by_cpu[cpu], __ATOMIC_RELAXED));
    }
    return high_water_total;
}

uint64_t filesystem_processed_events_total(void) {
    return __atomic_load_n(&g_storage_processed_events_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_processed_irq_events_total(void) {
    return __atomic_load_n(&g_storage_processed_irq_events_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_processed_injected_events_total(void) {
    return __atomic_load_n(&g_storage_processed_injected_events_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_batches_total(void) {
    return __atomic_load_n(&g_storage_batches_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_probe_total(void) {
    return __atomic_load_n(&g_fs_probe_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_probe_fat16_total(void) {
    return __atomic_load_n(&g_fs_probe_fat16_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_probe_fat32_total(void) {
    return __atomic_load_n(&g_fs_probe_fat32_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_probe_ntfs_total(void) {
    return __atomic_load_n(&g_fs_probe_ntfs_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_mount_attempt_total(void) {
    return __atomic_load_n(&g_fs_mount_attempt_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_mount_success_total(void) {
    return __atomic_load_n(&g_fs_mount_success_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_root_scan_total(void) {
    return __atomic_load_n(&g_fs_fat32_root_scan_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_root_entry_total(void) {
    return __atomic_load_n(&g_fs_fat32_root_entry_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_root_file_total(void) {
    return __atomic_load_n(&g_fs_fat32_root_file_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_root_dir_total(void) {
    return __atomic_load_n(&g_fs_fat32_root_dir_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_open_attempt_total(void) {
    return __atomic_load_n(&g_fs_fat32_open_attempt_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_open_success_total(void) {
    return __atomic_load_n(&g_fs_fat32_open_success_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_read_attempt_total(void) {
    return __atomic_load_n(&g_fs_fat32_read_attempt_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_read_success_total(void) {
    return __atomic_load_n(&g_fs_fat32_read_success_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_fat32_read_bytes_total(void) {
    return __atomic_load_n(&g_fs_fat32_read_bytes_total, __ATOMIC_RELAXED);
}

void filesystem_set_sector_reader(filesystem_sector_reader_t reader) {
    g_sector_reader = reader;
    __atomic_fetch_and(&g_fs_seen_kinds_mask, ~(1ULL << 5), __ATOMIC_RELAXED);
}

bool filesystem_has_sector_reader(void) {
    return g_sector_reader != 0;
}

uint64_t filesystem_sector_read_total(void) {
    return __atomic_load_n(&g_fs_sector_read_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_sector_read_callback_total(void) {
    return __atomic_load_n(&g_fs_sector_read_callback_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_sector_read_synthetic_total(void) {
    return __atomic_load_n(&g_fs_sector_read_synthetic_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_sector_read_fail_total(void) {
    return __atomic_load_n(&g_fs_sector_read_fail_total, __ATOMIC_RELAXED);
}

bool filesystem_probe_virtio_9p(void) {
    return probe_virtio_9p_devices_impl() > 0;
}

uint64_t filesystem_virtio_9p_probe_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_probe_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_device_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_device_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_last_bus(void) {
    return __atomic_load_n(&g_fs_virtio_9p_last_bus, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_last_slot(void) {
    return __atomic_load_n(&g_fs_virtio_9p_last_slot, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_last_function(void) {
    return __atomic_load_n(&g_fs_virtio_9p_last_function, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_last_device_id(void) {
    return __atomic_load_n(&g_fs_virtio_9p_last_device_id, __ATOMIC_RELAXED);
}

bool filesystem_virtio_9p_head_request_submit(const char *path,
                                              uint64_t max_bytes,
                                              uint64_t *out_request_id) {
    if (out_request_id == 0 || max_bytes == 0 || max_bytes > 2048) {
        return false;
    }
    char normalized_path[64];
    if (!copy_ascii_path(normalized_path, sizeof(normalized_path), path)) {
        return false;
    }

    for (uint64_t i = 0; i < kFsVirtio9pReqQueueCap; i++) {
        uint64_t state = __atomic_load_n(&g_fs_virtio_9p_req_queue[i].state, __ATOMIC_ACQUIRE);
        if (state == kFsVirtio9pReqStatePending || state == kFsVirtio9pReqStateRunning ||
            state == kFsVirtio9pReqStateClaimed) {
            continue;
        }
        uint64_t expected = state;
        if (!__atomic_compare_exchange_n(&g_fs_virtio_9p_req_queue[i].state,
                                         &expected,
                                         kFsVirtio9pReqStateClaimed,
                                         false,
                                         __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE)) {
            continue;
        }

        const uint64_t req_id = __atomic_add_fetch(&g_fs_virtio_9p_req_next_id, 1, __ATOMIC_RELAXED);
        g_fs_virtio_9p_req_queue[i].request_id = req_id;
        g_fs_virtio_9p_req_queue[i].max_bytes = max_bytes;
        g_fs_virtio_9p_req_queue[i].bytes = 0;
        g_fs_virtio_9p_req_queue[i].checksum = 0;
        for (uint64_t k = 0; k < sizeof(g_fs_virtio_9p_req_queue[i].path); k++) {
            g_fs_virtio_9p_req_queue[i].path[k] = normalized_path[k];
            if (normalized_path[k] == '\0') {
                break;
            }
        }
        __atomic_fetch_add(&g_fs_virtio_9p_read_attempt_total, 1, __ATOMIC_RELAXED);
#if FORTIFY_QEMU_SHARE_ENABLE
        if (__atomic_load_n(&g_fs_virtio_9p_share_experimental_mode, __ATOMIC_RELAXED) != 0) {
            __atomic_store_n(&g_fs_virtio_9p_req_queue[i].state, kFsVirtio9pReqStatePending, __ATOMIC_RELEASE);
            if (!schedule_virtio_9p_request_worker()) {
                complete_virtio_9p_request(i, false, 0, 0, false, false);
            }
            *out_request_id = req_id;
            return true;
        }
#if FORTIFY_SHARE_README_BRIDGE
        uint64_t bytes = 0;
        uint64_t checksum = 0;
        const bool ok = bridge_readme_head(max_bytes, &bytes, &checksum);
        complete_virtio_9p_request(i, ok, bytes, checksum, false, true);
        *out_request_id = req_id;
        return true;
#endif
        complete_virtio_9p_request(i, false, 0, 0, false, false);
#else
        __atomic_store_n(&g_fs_virtio_9p_req_queue[i].state, kFsVirtio9pReqStatePending, __ATOMIC_RELEASE);
        if (!schedule_virtio_9p_request_worker()) {
            complete_virtio_9p_request(i, false, 0, 0, false, false);
        }
#endif
        *out_request_id = req_id;
        return true;
    }
    return false;
}

bool filesystem_virtio_9p_head_request_status(uint64_t request_id,
                                              uint64_t *out_state,
                                              uint64_t *out_bytes,
                                              uint64_t *out_checksum) {
    if (request_id == 0 || out_state == 0 || out_bytes == 0 || out_checksum == 0) {
        return false;
    }
    for (uint64_t i = 0; i < kFsVirtio9pReqQueueCap; i++) {
        if (g_fs_virtio_9p_req_queue[i].request_id != request_id) {
            continue;
        }
        const uint64_t state = __atomic_load_n(&g_fs_virtio_9p_req_queue[i].state, __ATOMIC_ACQUIRE);
        if (state == kFsVirtio9pReqStateFree) {
            return false;
        }
        *out_state = state;
        *out_bytes = g_fs_virtio_9p_req_queue[i].bytes;
        *out_checksum = g_fs_virtio_9p_req_queue[i].checksum;
        return true;
    }
    return false;
}

uint64_t filesystem_virtio_9p_head_request_submit_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_read_attempt_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_done_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_read_done_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_done_ok_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_read_success_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_done_err_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_read_done_err_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_pending_total(void) {
    uint64_t total = 0;
    for (uint64_t i = 0; i < kFsVirtio9pReqQueueCap; i++) {
        if (__atomic_load_n(&g_fs_virtio_9p_req_queue[i].state, __ATOMIC_ACQUIRE) ==
            kFsVirtio9pReqStatePending) {
            total++;
        }
    }
    return total;
}

uint64_t filesystem_virtio_9p_head_request_running_total(void) {
    uint64_t total = 0;
    for (uint64_t i = 0; i < kFsVirtio9pReqQueueCap; i++) {
        if (__atomic_load_n(&g_fs_virtio_9p_req_queue[i].state, __ATOMIC_ACQUIRE) ==
            kFsVirtio9pReqStateRunning) {
            total++;
        }
    }
    return total;
}

uint64_t filesystem_virtio_9p_head_request_done_bridge_ok_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_done_bridge_ok_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_done_bridge_err_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_done_bridge_err_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_done_worker_ok_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_done_worker_ok_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_done_worker_err_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_done_worker_err_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_enqueue_worker_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_enqueue_worker_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_head_request_enqueue_worker_fail_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_enqueue_worker_fail_total, __ATOMIC_RELAXED);
}

uint64_t filesystem_virtio_9p_mode_blocked_total(void) {
    return __atomic_load_n(&g_fs_virtio_9p_mode_blocked_total, __ATOMIC_RELAXED);
}

bool filesystem_set_virtio_9p_share_experimental_mode(bool enabled) {
#if FORTIFY_QEMU_SHARE_ENABLE
    if (enabled) {
        __atomic_fetch_add(&g_fs_virtio_9p_mode_blocked_total, 1, __ATOMIC_RELAXED);
        __atomic_store_n(&g_fs_virtio_9p_share_experimental_mode, 0, __ATOMIC_RELAXED);
        return false;
    }
#endif
    __atomic_store_n(&g_fs_virtio_9p_share_experimental_mode, enabled ? 1ULL : 0ULL, __ATOMIC_RELAXED);
    return true;
}

bool filesystem_virtio_9p_share_experimental_mode_enabled(void) {
    return __atomic_load_n(&g_fs_virtio_9p_share_experimental_mode, __ATOMIC_RELAXED) != 0;
}
