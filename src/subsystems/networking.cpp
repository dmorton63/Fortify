#include "fortify/log.h"
#include "fortify/subsystems.h"

bool networking_init(void) {
    log_info("networking: NIC stack stub ready.");
    return true;
}

bool webaccess_init(void) {
    log_info("webaccess: HTTP client stack stub ready.");
    return true;
}

