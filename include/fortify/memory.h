#ifndef FORTIFY_MEMORY_H
#define FORTIFY_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

bool pmm_ready(void);
bool pmm_alloc_frame(uint64_t *out_phys_addr);
bool pmm_free_frame(uint64_t phys_addr);
bool pmm_reserve_range(uint64_t phys_base, uint64_t length);

bool vmm_ready(void);
bool vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);
bool vmm_unmap_page(uint64_t virt_addr);
bool vmm_translate(uint64_t virt_addr, uint64_t *out_phys_addr);

uint64_t vmm_hhdm_offset(void);

#endif
