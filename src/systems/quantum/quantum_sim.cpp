#include <stdint.h>

#include "fortify/systems/logging/log.h"
#include "fortify/systems/subsystems/subsystems.h"

static uint32_t g_rng_state = 0x41c64e6dU;

static uint32_t xorshift32(void) {
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return x;
}

bool quantum_sim_init(void) {
    log_info("quantum: simulator subsystem ready.");
    return true;
}

void quantum_sim_run_demo(void) {
    uint32_t sample = xorshift32() & 1U;
    if (sample == 0U) {
        log_info("quantum: |+> collapsed to |0>.");
        return;
    }

    log_info("quantum: |+> collapsed to |1>.");
}

