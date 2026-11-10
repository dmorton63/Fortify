#include "fortify/systems/boot/boot.h"

__attribute__((section(".limine_requests"), used))
volatile LIMINE_BASE_REVISION(3);

__attribute__((section(".limine_requests_start"), used))
volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((section(".limine_requests"), used))
volatile struct limine_framebuffer_request g_framebuffer_request = {
    .id = { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, LIMINE_FRAMEBUFFER_REQUEST_ID_1, LIMINE_FRAMEBUFFER_REQUEST_ID_2 },
    .revision = 0,
    .response = 0
};

__attribute__((section(".limine_requests"), used))
volatile struct limine_smp_request g_smp_request = {
    .id = { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, LIMINE_MP_REQUEST_ID_1, LIMINE_MP_REQUEST_ID_2 },
    .revision = 0,
    .response = 0,
    .flags = 0
};

__attribute__((section(".limine_requests"), used))
volatile struct limine_memmap_request g_memmap_request = {
    .id = { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, LIMINE_MEMMAP_REQUEST_ID_1, LIMINE_MEMMAP_REQUEST_ID_2 },
    .revision = 0,
    .response = 0
};

__attribute__((section(".limine_requests"), used))
volatile struct limine_hhdm_request g_hhdm_request = {
    .id = { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, LIMINE_HHDM_REQUEST_ID_1, LIMINE_HHDM_REQUEST_ID_2 },
    .revision = 0,
    .response = 0
};

__attribute__((section(".limine_requests_end"), used))
volatile LIMINE_REQUESTS_END_MARKER;

bool boot_protocol_ready(void) {
    return LIMINE_BASE_REVISION_SUPPORTED;
}

struct limine_framebuffer_response *boot_framebuffer_response(void) {
    return g_framebuffer_request.response;
}

struct limine_smp_response *boot_smp_response(void) {
    return g_smp_request.response;
}

struct limine_memmap_response *boot_memmap_response(void) {
    return g_memmap_request.response;
}

struct limine_hhdm_response *boot_hhdm_response(void) {
    return g_hhdm_request.response;
}

uint64_t boot_cpu_count(void) {
    if (g_smp_request.response == 0) {
        return 1;
    }

    if (g_smp_request.response->cpu_count == 0) {
        return 1;
    }

    return g_smp_request.response->cpu_count;
}

namespace {
uint32_t cpuid_max_basic_leaf(void) {
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    __asm__ volatile (
        "cpuid"
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
    __asm__ volatile (
        "cpuid"
        : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        :
        : "memory");
    return (ebx >> 24) & 0xffU;
}

uint32_t read_x2apic_id(void) {
    uint32_t eax = 0x0b;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;
    __asm__ volatile (
        "cpuid"
        : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx)
        :
        : "memory");
    if (ebx == 0) {
        return 0xffffffffU;
    }
    return edx;
}

uint32_t read_current_lapic_id(void) {
    if (cpuid_max_basic_leaf() >= 0x0b) {
        const uint32_t x2apic_id = read_x2apic_id();
        if (x2apic_id != 0xffffffffU) {
            return x2apic_id;
        }
    }
    return read_initial_apic_id();
}
}  // namespace

uint64_t boot_current_cpu_index(void) {
    struct limine_smp_response *smp = g_smp_request.response;
    if (smp == 0 || smp->cpus == 0 || smp->cpu_count == 0) {
        return 0;
    }

    const uint32_t lapic_id = read_current_lapic_id();
    for (uint64_t i = 0; i < smp->cpu_count; i++) {
        struct limine_smp_info *cpu = smp->cpus[i];
        if (cpu != 0 && cpu->lapic_id == lapic_id) {
            return i;
        }
    }
    return 0;
}
