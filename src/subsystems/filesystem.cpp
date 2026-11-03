#include "fortify/log.h"
#include "fortify/subsystems.h"

bool filesystem_init(void) {
    log_info("filesystem: VFS bootstrap stub ready.");
    return true;
}

