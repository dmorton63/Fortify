#include "fortify/systems/logging/log.h"
#include "fortify/systems/subsystems/subsystems.h"

bool tpm_init(void) {
    log_info("tpm: TPM discovery/auth stub ready.");
    return true;
}

bool secureboot_init(void) {
    log_info("secureboot: chain-of-trust policy stub ready.");
    return true;
}

