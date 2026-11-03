#include "fortify/boot.h"
#include "fortify/log.h"
#include "fortify/subsystems.h"

static volatile uint64_t g_ap_online = 0;

static void fortify_ap_entry(struct limine_smp_info *cpu) {
    (void)cpu;
    g_ap_online++;
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

bool smp_init(void) {
    struct limine_smp_response *smp = boot_smp_response();
    if (smp == 0 || smp->cpus == 0 || smp->cpu_count == 0) {
        log_warn("smp: unavailable, continuing BSP-only.");
        return true;
    }

    for (uint64_t i = 0; i < smp->cpu_count; i++) {
        struct limine_smp_info *cpu = smp->cpus[i];
        if (cpu == 0) {
            continue;
        }

        if (cpu->lapic_id == smp->bsp_lapic_id) {
            continue;
        }

        cpu->goto_address = fortify_ap_entry;
    }

    log_info("smp: AP startup callbacks installed.");
    return true;
}

