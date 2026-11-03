#include "fortify/log.h"
#include "fortify/subsystems.h"

bool usb_init(void) {
    log_info("usb: host controller discovery stub ready.");
    return true;
}

