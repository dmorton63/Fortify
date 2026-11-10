#include "fortify/systems/boot/boot.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

static volatile uint64_t g_ap_online = 0;
static uint64_t g_smp_total_cpus = 1;
static uint64_t g_ipi_kick_count = 0;
static uint64_t g_ipi_kick_failures = 0;
static volatile uint64_t g_ipi_kick_last_mode = 0;
static volatile uint64_t g_ipi_kick_observed_total = 0;
static volatile uint64_t g_ipi_kick_observed_non_bsp_total = 0;
static volatile uint64_t g_ap_wake_count_total = 0;
static volatile uint64_t g_ap_interruptible_ready_count = 0;
static volatile uint64_t g_ap_nonkick_interrupt_total = 0;
static volatile uint64_t g_ipi_diag_mode = 0;
static volatile uint64_t g_ipi_diag_path = 0;
static volatile uint64_t g_ipi_diag_target = 0;
static volatile uint64_t g_ipi_diag_icr_low = 0;
static volatile uint64_t g_ipi_diag_icr_high = 0;
static volatile uint64_t g_ipi_diag_esr_before = 0;
static volatile uint64_t g_ipi_diag_esr_after = 0;
static volatile uint64_t g_ipi_diag_result = 0;
static bool g_apic_mmio_ready = false;
static bool g_apic_x2apic_ready = false;
static uint64_t g_apic_phys_base = 0;
static uint64_t g_apic_memmap_type = ~0ULL;
constexpr uint64_t kSmpCpuCounterCap = 64;
static volatile uint64_t g_ap_wake_count_by_cpu[kSmpCpuCounterCap] = {};
static volatile uint64_t g_ipi_kick_observed_by_cpu[kSmpCpuCounterCap] = {};
static volatile uint8_t g_ap_interruptible_ready_by_cpu[kSmpCpuCounterCap] = {};
static volatile uint32_t g_cpu_x2apic_id_by_index[kSmpCpuCounterCap] = {};

namespace {
constexpr uint32_t kMsrApicBase = 0x1b;
constexpr uint32_t kMsrX2ApicTpr = 0x808;
constexpr uint32_t kMsrX2ApicSivr = 0x80f;
constexpr uint64_t kApicBaseMask = 0xfffff000ULL;
constexpr uint64_t kApicGlobalEnable = (1ULL << 11);
constexpr uint64_t kApicX2ApicEnable = (1ULL << 10);
constexpr uint32_t kMsrX2ApicIcr = 0x830;
constexpr uint32_t kMsrX2ApicEsr = 0x828;
constexpr uint32_t kMsrX2ApicEoi = 0x80b;
constexpr uint32_t kLocalApicTpr = 0x80;
constexpr uint32_t kLocalApicSivr = 0xF0;
constexpr uint32_t kLocalApicEoi = 0xB0;
constexpr uint32_t kLocalApicEsr = 0x280;
constexpr uint32_t kLocalApicIcrLow = 0x300;
constexpr uint32_t kLocalApicIcrHigh = 0x310;
constexpr uint32_t kLocalApicIcrDeliveryStatus = (1U << 12);
constexpr uint8_t kSmpKickVector = 0x60;
constexpr bool kEnableSmpIpiKick = true;
constexpr bool kPreferX2ApicKick = false;
constexpr bool kAutoEnableX2Apic = true;
constexpr bool kLogApEntryDetails = false;
constexpr uint64_t kApicPageSize = 4096;

struct __attribute__((packed)) idtr_state {
    uint16_t limit;
    uint64_t base;
};

void write_hex64(char *out, uint64_t value) {
    static const char k_hex[] = "0123456789abcdef";
    out[0] = '0';
    out[1] = 'x';
    for (uint64_t i = 0; i < 16; i++) {
        out[17 - i] = k_hex[value & 0x0f];
        value >>= 4;
    }
    out[18] = '\0';
}

void log_smp_apic_readiness(const char *state, uint64_t phys_base, uint64_t memmap_type) {
    char base_hex[19];
    char type_hex[19];
    write_hex64(base_hex, phys_base);
    write_hex64(type_hex, memmap_type);

    serial_write("[INFO] smp: LAPIC MMIO readiness=");
    serial_write(state);
    serial_write(" apic_base=");
    serial_write(base_hex);
    serial_write(" memmap_type=");
    serial_write(type_hex);
    serial_write("\n");

    console_write("[INFO] smp: LAPIC MMIO readiness=");
    console_write(state);
    console_write(" apic_base=");
    console_write(base_hex);
    console_write(" memmap_type=");
    console_write(type_hex);
    console_write("\n");
}

void log_ap_entry_readiness(uint64_t cpu_index) {
    if (!kLogApEntryDetails) {
        return;
    }
    uint64_t rflags = 0;
    __asm__ volatile ("pushfq; popq %0" : "=r"(rflags));
    idtr_state idtr = {};
    __asm__ volatile ("sidt %0" : "=m"(idtr));

    char cpu_hex[19];
    char if_hex[19];
    char idtr_base_hex[19];
    char idtr_lim_hex[19];
    write_hex64(cpu_hex, cpu_index);
    write_hex64(if_hex, (rflags & 0x200) != 0 ? 1 : 0);
    write_hex64(idtr_base_hex, idtr.base);
    write_hex64(idtr_lim_hex, idtr.limit);

    serial_write("[INFO] smp: AP entry cpu=");
    serial_write(cpu_hex);
    serial_write(" if=");
    serial_write(if_hex);
    serial_write(" idtr.base=");
    serial_write(idtr_base_hex);
    serial_write(" idtr.limit=");
    serial_write(idtr_lim_hex);
    serial_write("\n");
}

uint64_t read_msr(uint32_t msr) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

void write_msr(uint32_t msr, uint64_t value) {
    uint32_t lo = static_cast<uint32_t>(value & 0xffffffffU);
    uint32_t hi = static_cast<uint32_t>(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

bool cpu_supports_x2apic(void) {
    uint32_t eax = 1;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    __asm__ volatile ("cpuid"
                      : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      :
                      : "memory");
    return (ecx & (1U << 21)) != 0;
}

uint32_t cpuid_max_basic_leaf(void) {
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    __asm__ volatile ("cpuid"
                      : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      :
                      : "memory");
    return eax;
}

uint32_t read_initial_apic_id(void) {
    uint32_t eax = 1;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    __asm__ volatile ("cpuid"
                      : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      :
                      : "memory");
    return (ebx >> 24) & 0xffU;
}

uint32_t read_current_lapic_id(void) {
    if (cpuid_max_basic_leaf() >= 0x0b) {
        uint32_t eax = 0x0b;
        uint32_t ebx = 0;
        uint32_t ecx = 0;
        uint32_t edx = 0;
        __asm__ volatile ("cpuid"
                          : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx)
                          :
                          : "memory");
        if (ebx != 0) {
            return edx;
        }
    }
    return read_initial_apic_id();
}

uint64_t cpu_index_from_lapic_id(uint32_t lapic_id) {
    struct limine_smp_response *smp = boot_smp_response();
    if (smp == nullptr || smp->cpus == nullptr || smp->cpu_count == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < smp->cpu_count; i++) {
        struct limine_smp_info *cpu = smp->cpus[i];
        if (cpu != nullptr && cpu->lapic_id == lapic_id) {
            return i;
        }
    }
    return 0;
}

void ensure_x2apic_enabled_current_cpu(void) {
    if (!cpu_supports_x2apic()) {
        return;
    }
    const uint64_t apic_base = read_msr(kMsrApicBase);
    const uint64_t desired = apic_base | kApicGlobalEnable | kApicX2ApicEnable;
    if (desired != apic_base) {
        write_msr(kMsrApicBase, desired);
    }
}

bool apic_phys_range_in_memmap(uint64_t phys_base, uint64_t length, uint64_t *out_type) {
    struct limine_memmap_response *memmap = boot_memmap_response();
    if (memmap == nullptr || memmap->entries == nullptr || memmap->entry_count == 0) {
        return false;
    }

    const uint64_t end = phys_base + length;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry == nullptr || entry->length == 0) {
            continue;
        }
        const uint64_t entry_start = entry->base;
        const uint64_t entry_end = entry->base + entry->length;
        if (phys_base >= entry_start && end <= entry_end) {
            if (out_type != nullptr) {
                *out_type = entry->type;
            }
            return true;
        }
    }
    return false;
}

bool apic_mmio_ready(void) {
    const uint64_t apic_base = read_msr(kMsrApicBase);
    const bool apic_enabled = (apic_base & kApicGlobalEnable) != 0;
    const bool x2apic_enabled = (apic_base & kApicX2ApicEnable) != 0;
    g_apic_phys_base = apic_base & kApicBaseMask;

    if (!apic_enabled || x2apic_enabled) {
        g_apic_memmap_type = ~0ULL;
        return false;
    }

    struct limine_hhdm_response *hhdm = boot_hhdm_response();
    if (hhdm == nullptr) {
        g_apic_memmap_type = ~0ULL;
        return false;
    }

    uint64_t memmap_type = ~0ULL;
    if (!apic_phys_range_in_memmap(g_apic_phys_base, kApicPageSize, &memmap_type)) {
        g_apic_memmap_type = ~0ULL;
        return false;
    }
    g_apic_memmap_type = memmap_type;

    if (memmap_type == LIMINE_MEMMAP_USABLE) {
        return false;
    }
    return true;
}

volatile uint32_t *local_apic_mmio_base(void) {
    if (!g_apic_mmio_ready) {
        return nullptr;
    }
    struct limine_hhdm_response *hhdm = boot_hhdm_response();
    if (hhdm == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<volatile uint32_t *>(hhdm->offset + g_apic_phys_base);
}

bool local_apic_send_fixed_ipi(uint32_t lapic_id, uint8_t vector) {
    if (g_apic_x2apic_ready && (kPreferX2ApicKick || !g_apic_mmio_ready)) {
        const uint64_t icr = (static_cast<uint64_t>(lapic_id) << 32) | vector;
        write_msr(kMsrX2ApicIcr, icr);
        return true;
    }

    volatile uint32_t *apic = local_apic_mmio_base();
    if (apic == nullptr) {
        return false;
    }

    while ((apic[kLocalApicIcrLow / 4] & kLocalApicIcrDeliveryStatus) != 0) {
    }
    apic[kLocalApicIcrHigh / 4] = (lapic_id << 24);
    apic[kLocalApicIcrLow / 4] = vector;
    while ((apic[kLocalApicIcrLow / 4] & kLocalApicIcrDeliveryStatus) != 0) {
    }
    return true;
}

bool local_apic_send_broadcast_ipi_excluding_self(uint8_t vector) {
    constexpr uint32_t kIcrDestinationShorthandAllExSelf = (3U << 18);

    if (g_apic_x2apic_ready && (kPreferX2ApicKick || !g_apic_mmio_ready)) {
        const uint64_t icr = static_cast<uint64_t>(kIcrDestinationShorthandAllExSelf | vector);
        write_msr(kMsrX2ApicIcr, icr);
        return true;
    }

    volatile uint32_t *apic = local_apic_mmio_base();
    if (apic == nullptr) {
        return false;
    }
    while ((apic[kLocalApicIcrLow / 4] & kLocalApicIcrDeliveryStatus) != 0) {
    }
    apic[kLocalApicIcrLow / 4] = (kIcrDestinationShorthandAllExSelf | vector);
    while ((apic[kLocalApicIcrLow / 4] & kLocalApicIcrDeliveryStatus) != 0) {
    }
    return true;
}

bool local_apic_send_broadcast_ipi_including_self(uint8_t vector) {
    constexpr uint32_t kIcrDestinationShorthandAllInclSelf = (2U << 18);

    if (g_apic_x2apic_ready && (kPreferX2ApicKick || !g_apic_mmio_ready)) {
        const uint64_t icr = static_cast<uint64_t>(kIcrDestinationShorthandAllInclSelf | vector);
        write_msr(kMsrX2ApicIcr, icr);
        return true;
    }

    volatile uint32_t *apic = local_apic_mmio_base();
    if (apic == nullptr) {
        return false;
    }
    while ((apic[kLocalApicIcrLow / 4] & kLocalApicIcrDeliveryStatus) != 0) {
    }
    apic[kLocalApicIcrLow / 4] = (kIcrDestinationShorthandAllInclSelf | vector);
    while ((apic[kLocalApicIcrLow / 4] & kLocalApicIcrDeliveryStatus) != 0) {
    }
    return true;
}

uint64_t local_apic_read_esr(void) {
    if (g_apic_x2apic_ready && (kPreferX2ApicKick || !g_apic_mmio_ready)) {
        return read_msr(kMsrX2ApicEsr);
    }

    volatile uint32_t *apic = local_apic_mmio_base();
    if (apic == nullptr) {
        return 0;
    }
    return apic[kLocalApicEsr / 4];
}

void local_apic_read_icr_snapshot(uint64_t *out_low, uint64_t *out_high) {
    if (out_low == nullptr || out_high == nullptr) {
        return;
    }
    if (g_apic_x2apic_ready && (kPreferX2ApicKick || !g_apic_mmio_ready)) {
        const uint64_t icr = read_msr(kMsrX2ApicIcr);
        *out_low = icr & 0xffffffffULL;
        *out_high = (icr >> 32) & 0xffffffffULL;
        return;
    }

    volatile uint32_t *apic = local_apic_mmio_base();
    if (apic == nullptr) {
        *out_low = 0;
        *out_high = 0;
        return;
    }
    *out_low = apic[kLocalApicIcrLow / 4];
    *out_high = apic[kLocalApicIcrHigh / 4];
}

void capture_ipi_diag(uint64_t mode, uint64_t target, bool success, uint64_t esr_before, uint64_t esr_after) {
    const uint64_t path = (g_apic_x2apic_ready && (kPreferX2ApicKick || !g_apic_mmio_ready)) ? 1 : 2;
    uint64_t icr_low = 0;
    uint64_t icr_high = 0;
    local_apic_read_icr_snapshot(&icr_low, &icr_high);

    __atomic_store_n(&g_ipi_diag_mode, mode, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ipi_diag_path, path, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ipi_diag_target, target, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ipi_diag_icr_low, icr_low, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ipi_diag_icr_high, icr_high, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ipi_diag_esr_before, esr_before, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ipi_diag_esr_after, esr_after, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ipi_diag_result, success ? 1 : 0, __ATOMIC_RELAXED);
}

void local_apic_enable_current_cpu(void) {
    const uint64_t apic_base = read_msr(kMsrApicBase);
    if ((apic_base & kApicGlobalEnable) == 0) {
        return;
    }
    if ((apic_base & kApicX2ApicEnable) != 0) {
        write_msr(kMsrX2ApicTpr, 0);
        uint64_t sivr = read_msr(kMsrX2ApicSivr);
        sivr |= 0x100ULL;
        if ((sivr & 0xffULL) < 0x10ULL) {
            sivr = (sivr & ~0xffULL) | 0xffULL;
        }
        write_msr(kMsrX2ApicSivr, sivr);
        return;
    }

    volatile uint32_t *apic = local_apic_mmio_base();
    if (apic == nullptr) {
        return;
    }
    apic[kLocalApicTpr / 4] = 0;
    uint32_t sivr = apic[kLocalApicSivr / 4];
    sivr |= 0x100U;
    if ((sivr & 0xffU) < 0x10U) {
        sivr = (sivr & ~0xffU) | 0xffU;
    }
    apic[kLocalApicSivr / 4] = sivr;
}
}  // namespace

static void fortify_ap_entry(struct limine_smp_info *cpu) {
    (void)cpu;
    uint64_t cpu_index = boot_current_cpu_index();
    if (cpu != nullptr && cpu->extra_argument < kSmpCpuCounterCap) {
        cpu_index = cpu->extra_argument;
    }
    if (g_apic_x2apic_ready) {
        ensure_x2apic_enabled_current_cpu();
    }
    local_apic_enable_current_cpu();
    interrupts_load_idt_for_current_cpu();
    log_ap_entry_readiness(cpu_index);
    g_ap_online++;
    scheduler_mark_current_cpu_online();
    if (cpu_index < kSmpCpuCounterCap &&
        __atomic_exchange_n(&g_ap_interruptible_ready_by_cpu[cpu_index], 1, __ATOMIC_RELAXED) == 0) {
        __atomic_fetch_add(&g_ap_interruptible_ready_count, 1, __ATOMIC_RELAXED);
    }
    if (cpu_index < kSmpCpuCounterCap) {
        __atomic_store_n(&g_cpu_x2apic_id_by_index[cpu_index], read_current_lapic_id(), __ATOMIC_RELAXED);
    }
    for (;;) {
        scheduler_service_deferred_switch();
        __asm__ volatile ("sti; hlt; cli" : : : "memory");
        __atomic_fetch_add(&g_ap_wake_count_total, 1, __ATOMIC_RELAXED);
        if (cpu_index < kSmpCpuCounterCap) {
            __atomic_fetch_add(&g_ap_wake_count_by_cpu[cpu_index], 1, __ATOMIC_RELAXED);
        }
    }
}

bool smp_init(void) {
    struct limine_smp_response *smp = boot_smp_response();
    if (smp == 0 || smp->cpus == 0 || smp->cpu_count == 0) {
        g_smp_total_cpus = 1;
        log_warn("smp: unavailable, continuing BSP-only.");
        return true;
    }

    g_smp_total_cpus = smp->cpu_count;
    for (uint64_t i = 0; i < kSmpCpuCounterCap; i++) {
        __atomic_store_n(&g_cpu_x2apic_id_by_index[i], 0xffffffffU, __ATOMIC_RELAXED);
    }

    const uint64_t apic_base = read_msr(kMsrApicBase);
    const bool apic_enabled = (apic_base & kApicGlobalEnable) != 0;
    const bool x2apic_enabled = (apic_base & kApicX2ApicEnable) != 0;
    g_apic_x2apic_ready = false;
    if (apic_enabled && cpu_supports_x2apic()) {
        if (kAutoEnableX2Apic && !x2apic_enabled) {
            write_msr(kMsrApicBase, apic_base | kApicX2ApicEnable | kApicGlobalEnable);
        }
        const uint64_t apic_base_after = read_msr(kMsrApicBase);
        g_apic_x2apic_ready = (apic_base_after & kApicX2ApicEnable) != 0;
    }
    __atomic_store_n(&g_cpu_x2apic_id_by_index[0], read_current_lapic_id(), __ATOMIC_RELAXED);

    g_apic_mmio_ready = apic_mmio_ready();
    local_apic_enable_current_cpu();
    log_smp_apic_readiness((g_apic_x2apic_ready || g_apic_mmio_ready) ? "ready" : "not-ready",
                           g_apic_phys_base, g_apic_memmap_type);

    for (uint64_t i = 0; i < smp->cpu_count; i++) {
        struct limine_smp_info *cpu = smp->cpus[i];
        if (cpu == 0) {
            continue;
        }

        if (cpu->lapic_id == smp->bsp_lapic_id) {
            continue;
        }

        cpu->extra_argument = i;
        cpu->goto_address = fortify_ap_entry;
    }

    log_info("smp: AP startup callbacks installed.");
    return true;
}

uint64_t smp_online_cpu_count(void) {
    return g_ap_online + 1;
}

uint64_t smp_total_cpu_count(void) {
    return g_smp_total_cpus;
}

uint64_t smp_ap_interruptible_ready_count(void) {
    return __atomic_load_n(&g_ap_interruptible_ready_count, __ATOMIC_RELAXED);
}

bool smp_kick_all_aps(void) {
    if (!kEnableSmpIpiKick) {
        return false;
    }
    if (!g_apic_x2apic_ready && !g_apic_mmio_ready) {
        return false;
    }

    struct limine_smp_response *smp = boot_smp_response();
    if (smp == nullptr || smp->cpus == nullptr || smp->cpu_count <= 1) {
        return false;
    }

    bool kicked_any = false;
    uint64_t mode = 1;  // directed physical destination (preferred)
    for (uint64_t i = 0; i < smp->cpu_count; i++) {
        struct limine_smp_info *cpu = smp->cpus[i];
        if (cpu == nullptr || cpu->lapic_id == smp->bsp_lapic_id) {
            continue;
        }
        uint32_t target_id = cpu->lapic_id;
        if (g_apic_x2apic_ready && i < kSmpCpuCounterCap) {
            const uint32_t discovered = __atomic_load_n(&g_cpu_x2apic_id_by_index[i], __ATOMIC_RELAXED);
            if (discovered != 0xffffffffU) {
                target_id = discovered;
            }
        }
        const uint64_t esr_before = local_apic_read_esr();
        const bool sent = local_apic_send_fixed_ipi(target_id, kSmpKickVector);
        const uint64_t esr_after = local_apic_read_esr();
        capture_ipi_diag(mode, target_id, sent, esr_before, esr_after);
        if (sent) {
            kicked_any = true;
        } else {
            g_ipi_kick_failures++;
        }
    }
    if (!kicked_any) {
        mode = 2;  // fallback: broadcast excluding self
        const uint64_t esr_before = local_apic_read_esr();
        kicked_any = local_apic_send_broadcast_ipi_excluding_self(kSmpKickVector);
        const uint64_t esr_after = local_apic_read_esr();
        capture_ipi_diag(mode, 0xffffffffffffffffULL, kicked_any, esr_before, esr_after);
        if (!kicked_any) {
            g_ipi_kick_failures++;
            mode = 3;  // final fallback: broadcast including self
            const uint64_t esr_before_self = local_apic_read_esr();
            kicked_any = local_apic_send_broadcast_ipi_including_self(kSmpKickVector);
            const uint64_t esr_after_self = local_apic_read_esr();
            capture_ipi_diag(mode, 0xfffffffffffffffeULL, kicked_any, esr_before_self, esr_after_self);
            if (!kicked_any) {
                g_ipi_kick_failures++;
            }
        }
    }

    if (kicked_any) {
        g_ipi_kick_count++;
        g_ipi_kick_last_mode = mode;
    }
    return kicked_any;
}

uint64_t smp_ipi_kick_count(void) {
    return g_ipi_kick_count;
}

uint64_t smp_ipi_kick_last_mode(void) {
    return __atomic_load_n(&g_ipi_kick_last_mode, __ATOMIC_RELAXED);
}

uint8_t smp_ipi_kick_vector(void) {
    return kSmpKickVector;
}

void smp_note_ipi_kick_interrupt(void) {
    const uint32_t lapic_id = read_current_lapic_id();
    const uint64_t cpu_idx = cpu_index_from_lapic_id(lapic_id);
    struct limine_smp_response *smp = boot_smp_response();
    __atomic_fetch_add(&g_ipi_kick_observed_total, 1, __ATOMIC_RELAXED);
    if (smp != nullptr && lapic_id != smp->bsp_lapic_id) {
        __atomic_fetch_add(&g_ipi_kick_observed_non_bsp_total, 1, __ATOMIC_RELAXED);
    }

    if (cpu_idx < kSmpCpuCounterCap) {
        __atomic_fetch_add(&g_ipi_kick_observed_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
    }
}

void smp_acknowledge_ipi_interrupt(void) {
    if (g_apic_x2apic_ready && (kPreferX2ApicKick || !g_apic_mmio_ready)) {
        write_msr(kMsrX2ApicEoi, 0);
        return;
    }
    volatile uint32_t *apic = local_apic_mmio_base();
    if (apic == nullptr) {
        return;
    }
    apic[kLocalApicEoi / 4] = 0;
}

void smp_note_ap_nonkick_interrupt(uint8_t vector) {
    if (vector < 32) {
        return;
    }
    const uint64_t cpu_idx = cpu_index_from_lapic_id(read_current_lapic_id());
    if (cpu_idx == 0 || cpu_idx >= kSmpCpuCounterCap) {
        return;
    }
    __atomic_fetch_add(&g_ap_nonkick_interrupt_total, 1, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_observed_count(void) {
    return __atomic_load_n(&g_ipi_kick_observed_total, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_observed_ap_count(void) {
    return __atomic_load_n(&g_ipi_kick_observed_non_bsp_total, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_diag_mode(void) {
    return __atomic_load_n(&g_ipi_diag_mode, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_diag_path(void) {
    return __atomic_load_n(&g_ipi_diag_path, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_diag_target(void) {
    return __atomic_load_n(&g_ipi_diag_target, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_diag_icr_low(void) {
    return __atomic_load_n(&g_ipi_diag_icr_low, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_diag_icr_high(void) {
    return __atomic_load_n(&g_ipi_diag_icr_high, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_diag_esr_before(void) {
    return __atomic_load_n(&g_ipi_diag_esr_before, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_diag_esr_after(void) {
    return __atomic_load_n(&g_ipi_diag_esr_after, __ATOMIC_RELAXED);
}

uint64_t smp_ipi_kick_diag_result(void) {
    return __atomic_load_n(&g_ipi_diag_result, __ATOMIC_RELAXED);
}

uint64_t smp_ap_nonkick_interrupt_count(void) {
    return __atomic_load_n(&g_ap_nonkick_interrupt_total, __ATOMIC_RELAXED);
}

uint64_t smp_ap_wake_count_total(void) {
    return __atomic_load_n(&g_ap_wake_count_total, __ATOMIC_RELAXED);
}

uint64_t smp_ap_wake_count_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kSmpCpuCounterCap) {
        return 0;
    }
    return __atomic_load_n(&g_ap_wake_count_by_cpu[cpu_index], __ATOMIC_RELAXED);
}
