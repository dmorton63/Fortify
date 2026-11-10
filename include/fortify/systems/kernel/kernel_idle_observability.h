#ifndef FORTIFY_KERNEL_IDLE_OBSERVABILITY_H
#define FORTIFY_KERNEL_IDLE_OBSERVABILITY_H

#include <stdint.h>

void kernel_idle_poll_smp_observability(uint64_t ticks);

#endif
