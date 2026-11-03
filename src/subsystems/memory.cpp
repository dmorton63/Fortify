#include "fortify/boot.h"
#include "fortify/log.h"
#include "fortify/memory.h"
#include "fortify/subsystems.h"

namespace {
constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kPageMask = kPageSize - 1;
constexpr uint64_t kBootstrapPoolBase = 0x01000000;
constexpr uint64_t kBootstrapPoolPages = 65536; // 256 MiB fallback window.
constexpr uint64_t kFreedFrameStackCap = 4096;
constexpr uint64_t kReservedRangeCap = 64;
constexpr uint64_t kMaxMapEntries = 4096;
constexpr uint64_t kMaxMemmapEntries = 8192;
constexpr uint64_t kMaxUsableRanges = 256;

struct reserved_range {
    uint64_t start;
    uint64_t end;
};

struct usable_range {
    uint64_t base;
    uint64_t end;
    uint64_t cursor;
};

struct limine_memmap_entry_raw {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response_raw {
    uint64_t revision;
    uint64_t entry_count;
    uint64_t entries;
};

enum class pmm_mode {
    bootstrap_pool,
    memmap_ranges
};

struct pmm_state {
    bool ready;
    pmm_mode mode;
    uint64_t hhdm_offset;
    uint64_t next_page_index;
    uint64_t freed_frames[kFreedFrameStackCap];
    uint64_t freed_count;
    reserved_range reserved[kReservedRangeCap];
    uint64_t reserved_count;
    usable_range ranges[kMaxUsableRanges];
    uint64_t range_count;
};

struct vmm_mapping {
    bool present;
    uint64_t virt_page;
    uint64_t phys_page;
    uint64_t flags;
};

struct vmm_state {
    bool ready;
    uint64_t hhdm_offset;
    vmm_mapping mappings[kMaxMapEntries];
};

pmm_state g_pmm = {};
vmm_state g_vmm = {};

uint64_t align_down(uint64_t value, uint64_t align) {
    return value & ~(align - 1);
}

uint64_t align_up(uint64_t value, uint64_t align) {
    uint64_t rem = value % align;
    if (rem == 0) {
        return value;
    }
    return value + (align - rem);
}

bool range_contains(uint64_t addr, const reserved_range &r) {
    return addr >= r.start && addr < r.end;
}

bool is_reserved(uint64_t phys_addr) {
    for (uint64_t i = 0; i < g_pmm.reserved_count; i++) {
        if (range_contains(phys_addr, g_pmm.reserved[i])) {
            return true;
        }
    }
    return false;
}

uint64_t normalize_boot_ptr_addr(uint64_t addr, uint64_t hhdm_offset) {
    if (addr == 0) {
        return 0;
    }
    if (addr >= hhdm_offset) {
        return addr;
    }
    return addr + hhdm_offset;
}

bool pmm_add_usable_range(uint64_t base, uint64_t length) {
    if (g_pmm.range_count >= kMaxUsableRanges) {
        return false;
    }

    uint64_t start = align_up(base, kPageSize);
    if (start < base) {
        return false;
    }
    if (start - base >= length) {
        return true;
    }

    uint64_t end = align_down(base + length, kPageSize);
    if (end <= start) {
        return true;
    }

    usable_range &slot = g_pmm.ranges[g_pmm.range_count++];
    slot.base = start;
    slot.end = end;
    slot.cursor = start;
    return true;
}

void pmm_reset_bootstrap_mode(void) {
    g_pmm.mode = pmm_mode::bootstrap_pool;
    g_pmm.range_count = 0;
    g_pmm.next_page_index = 0;
}

bool pmm_init_from_memmap(struct limine_memmap_response *memmap, uint64_t hhdm_offset) {
    limine_memmap_response_raw *raw = reinterpret_cast<limine_memmap_response_raw *>(memmap);
    if (raw == 0 || raw->entry_count == 0 || raw->entry_count > kMaxMemmapEntries) {
        return false;
    }

    uint64_t entries_array_addr = normalize_boot_ptr_addr(raw->entries, hhdm_offset);
    if (entries_array_addr < hhdm_offset) {
        return false;
    }

    uint64_t *entries = reinterpret_cast<uint64_t *>(entries_array_addr);
    if (entries == 0) {
        return false;
    }

    g_pmm.range_count = 0;
    for (uint64_t i = 0; i < raw->entry_count; i++) {
        uint64_t entry_addr = normalize_boot_ptr_addr(entries[i], hhdm_offset);
        if (entry_addr < hhdm_offset) {
            return false;
        }

        limine_memmap_entry_raw *entry = reinterpret_cast<limine_memmap_entry_raw *>(entry_addr);
        if (entry == 0) {
            continue;
        }

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            if (!pmm_add_usable_range(entry->base, entry->length)) {
                return false;
            }
        }
    }

    if (g_pmm.range_count == 0) {
        return false;
    }

    g_pmm.mode = pmm_mode::memmap_ranges;
    return true;
}

bool pmm_reserve_range_internal(uint64_t phys_base, uint64_t length) {
    if (length == 0) {
        return true;
    }

    uint64_t start = align_down(phys_base, kPageSize);
    uint64_t end = align_up(phys_base + length, kPageSize);
    if (end <= start) {
        return true;
    }

    if (g_pmm.reserved_count < kReservedRangeCap) {
        reserved_range &slot = g_pmm.reserved[g_pmm.reserved_count++];
        slot.start = start;
        slot.end = end;
    }

    if (g_pmm.mode != pmm_mode::memmap_ranges) {
        return true;
    }

    for (uint64_t i = 0; i < g_pmm.range_count; i++) {
        usable_range &r = g_pmm.ranges[i];
        if (r.end <= start || r.base >= end) {
            continue;
        }

        if (start <= r.base && end >= r.end) {
            r.base = r.end;
            r.cursor = r.end;
            continue;
        }

        if (start <= r.base) {
            r.base = end;
            if (r.cursor < r.base) {
                r.cursor = r.base;
            }
            if (r.base > r.end) {
                r.base = r.end;
                r.cursor = r.end;
            }
            continue;
        }

        if (end >= r.end) {
            r.end = start;
            if (r.cursor > r.end) {
                r.cursor = r.end;
            }
            continue;
        }

        if (g_pmm.range_count >= kMaxUsableRanges) {
            r.end = start;
            if (r.cursor > r.end) {
                r.cursor = r.end;
            }
            continue;
        }

        usable_range tail = {};
        tail.base = end;
        tail.end = r.end;
        tail.cursor = (r.cursor > tail.base) ? r.cursor : tail.base;

        r.end = start;
        if (r.cursor > r.end) {
            r.cursor = r.end;
        }

        g_pmm.ranges[g_pmm.range_count++] = tail;
    }

    return true;
}

bool pmm_alloc_from_ranges(uint64_t *out_phys_addr) {
    for (uint64_t i = 0; i < g_pmm.range_count; i++) {
        usable_range &r = g_pmm.ranges[i];
        if (r.cursor >= r.end) {
            continue;
        }

        uint64_t candidate = r.cursor;
        r.cursor += kPageSize;
        if (!is_reserved(candidate)) {
            *out_phys_addr = candidate;
            return true;
        }
    }
    return false;
}

bool pmm_alloc_from_bootstrap(uint64_t *out_phys_addr) {
    while (g_pmm.next_page_index < kBootstrapPoolPages) {
        uint64_t candidate = kBootstrapPoolBase + (g_pmm.next_page_index * kPageSize);
        g_pmm.next_page_index++;
        if (!is_reserved(candidate)) {
            *out_phys_addr = candidate;
            return true;
        }
    }
    return false;
}

bool frame_in_bootstrap_window(uint64_t phys_addr) {
    if ((phys_addr & kPageMask) != 0) {
        return false;
    }
    return phys_addr >= kBootstrapPoolBase &&
           phys_addr < (kBootstrapPoolBase + (kBootstrapPoolPages * kPageSize));
}

int64_t vmm_find_slot_by_vpage(uint64_t virt_page) {
    for (uint64_t i = 0; i < kMaxMapEntries; i++) {
        if (g_vmm.mappings[i].present && g_vmm.mappings[i].virt_page == virt_page) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

int64_t vmm_find_free_slot(void) {
    for (uint64_t i = 0; i < kMaxMapEntries; i++) {
        if (!g_vmm.mappings[i].present) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}
}

bool memory_init(void) {
    struct limine_memmap_response *memmap = boot_memmap_response();
    if (memmap == 0 || memmap->entry_count == 0) {
        log_error("memory: Limine memory map unavailable.");
        return false;
    }

    struct limine_hhdm_response *hhdm = boot_hhdm_response();
    if (hhdm == 0 || (hhdm->offset & kPageMask) != 0) {
        log_error("memory: HHDM response unavailable/invalid.");
        return false;
    }

    g_vmm.hhdm_offset = hhdm->offset;
    g_vmm.ready = true;

    g_pmm.hhdm_offset = hhdm->offset;
    g_pmm.freed_count = 0;
    g_pmm.reserved_count = 0;
    g_pmm.ready = true;

    if (!pmm_init_from_memmap(memmap, hhdm->offset)) {
        pmm_reset_bootstrap_mode();
        log_warn("memory: memmap PMM init failed, using bootstrap frame pool.");
    }

    struct limine_framebuffer_response *fb = boot_framebuffer_response();
    if (fb != 0 && fb->framebuffer_count != 0 && fb->framebuffers != 0 && fb->framebuffers[0] != 0) {
        struct limine_framebuffer *primary = fb->framebuffers[0];
        uint64_t fb_base = reinterpret_cast<uint64_t>(primary->address);
        uint64_t fb_len = primary->pitch * primary->height;
        pmm_reserve_range_internal(fb_base, fb_len);
    }

    if (g_pmm.mode == pmm_mode::memmap_ranges) {
        log_info("memory: PMM memmap-range allocator + VMM APIs initialized.");
    } else {
        log_info("memory: PMM bootstrap allocator + VMM APIs initialized.");
    }
    return true;
}

bool pmm_ready(void) {
    return g_pmm.ready;
}

bool pmm_alloc_frame(uint64_t *out_phys_addr) {
    if (!g_pmm.ready || out_phys_addr == 0) {
        return false;
    }

    while (g_pmm.freed_count > 0) {
        g_pmm.freed_count--;
        uint64_t candidate = g_pmm.freed_frames[g_pmm.freed_count];
        if (!is_reserved(candidate)) {
            *out_phys_addr = candidate;
            return true;
        }
    }

    if (g_pmm.mode == pmm_mode::memmap_ranges) {
        if (pmm_alloc_from_ranges(out_phys_addr)) {
            return true;
        }
    }

    return pmm_alloc_from_bootstrap(out_phys_addr);
}

bool pmm_free_frame(uint64_t phys_addr) {
    if (!g_pmm.ready || (phys_addr & kPageMask) != 0) {
        return false;
    }

    if (!frame_in_bootstrap_window(phys_addr) && g_pmm.mode != pmm_mode::memmap_ranges) {
        return false;
    }

    if (g_pmm.freed_count >= kFreedFrameStackCap) {
        return false;
    }

    g_pmm.freed_frames[g_pmm.freed_count++] = phys_addr;
    return true;
}

bool pmm_reserve_range(uint64_t phys_base, uint64_t length) {
    if (!g_pmm.ready || length == 0) {
        return false;
    }
    return pmm_reserve_range_internal(phys_base, length);
}

bool vmm_ready(void) {
    return g_vmm.ready;
}

bool vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    if (!g_vmm.ready || (virt_addr & kPageMask) != 0 || (phys_addr & kPageMask) != 0) {
        return false;
    }

    uint64_t vpage = virt_addr / kPageSize;
    uint64_t ppage = phys_addr / kPageSize;

    int64_t slot = vmm_find_slot_by_vpage(vpage);
    if (slot >= 0) {
        g_vmm.mappings[slot].phys_page = ppage;
        g_vmm.mappings[slot].flags = flags;
        return true;
    }

    slot = vmm_find_free_slot();
    if (slot < 0) {
        return false;
    }

    g_vmm.mappings[slot].present = true;
    g_vmm.mappings[slot].virt_page = vpage;
    g_vmm.mappings[slot].phys_page = ppage;
    g_vmm.mappings[slot].flags = flags;
    return true;
}

bool vmm_unmap_page(uint64_t virt_addr) {
    if (!g_vmm.ready || (virt_addr & kPageMask) != 0) {
        return false;
    }

    int64_t slot = vmm_find_slot_by_vpage(virt_addr / kPageSize);
    if (slot < 0) {
        return false;
    }

    g_vmm.mappings[slot].present = false;
    g_vmm.mappings[slot].virt_page = 0;
    g_vmm.mappings[slot].phys_page = 0;
    g_vmm.mappings[slot].flags = 0;
    return true;
}

bool vmm_translate(uint64_t virt_addr, uint64_t *out_phys_addr) {
    if (!g_vmm.ready || out_phys_addr == 0) {
        return false;
    }

    uint64_t vpage = virt_addr / kPageSize;
    uint64_t offset = virt_addr & kPageMask;
    int64_t slot = vmm_find_slot_by_vpage(vpage);
    if (slot < 0) {
        return false;
    }

    *out_phys_addr = (g_vmm.mappings[slot].phys_page * kPageSize) + offset;
    return true;
}

uint64_t vmm_hhdm_offset(void) {
    return g_vmm.hhdm_offset;
}
