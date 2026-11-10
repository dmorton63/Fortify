#ifndef FORTIFY_KERNEL_IDLE_AP_SERVICE_H
#define FORTIFY_KERNEL_IDLE_AP_SERVICE_H

#include <stdint.h>

struct kernel_idle_ap_poll_result {
    uint64_t online;
    uint64_t total;
    uint64_t ipi_observed_aps;
    bool kick_attempts_exhausted;
};

bool kernel_idle_poll_ap_service(uint64_t ticks, kernel_idle_ap_poll_result *out_result);

#endif
