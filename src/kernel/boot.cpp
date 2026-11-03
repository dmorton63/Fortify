#include "fortify/boot.h"

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
