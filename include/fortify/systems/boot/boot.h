#ifndef FORTIFY_BOOT_H
#define FORTIFY_BOOT_H

#include <stdbool.h>
#include <stdint.h>

#include "fortify/systems/boot/limine.h"

bool boot_protocol_ready(void);
struct limine_framebuffer_response *boot_framebuffer_response(void);
struct limine_smp_response *boot_smp_response(void);
struct limine_memmap_response *boot_memmap_response(void);
struct limine_hhdm_response *boot_hhdm_response(void);
uint64_t boot_cpu_count(void);
uint64_t boot_current_cpu_index(void);

#endif
