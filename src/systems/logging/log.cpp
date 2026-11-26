#include "fortify/systems/console/console.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/serial/serial.h"

#ifndef FORTIFY_DISPLAY_LOGS
#define FORTIFY_DISPLAY_LOGS 1
#endif

#ifndef FORTIFY_DISPLAY_INFO_LOGS
#define FORTIFY_DISPLAY_INFO_LOGS 0
#endif

static void write_line(const char *prefix, const char *message, bool display) {
    serial_write(prefix);
    serial_write(message);
    serial_write("\n");
#if FORTIFY_DISPLAY_LOGS
    if (display) {
        console_write(prefix);
        console_write(message);
        console_write("\n");
    }
#else
    (void)display;
#endif
}

void log_info(const char *message) {
    write_line("[INFO] ", message, FORTIFY_DISPLAY_INFO_LOGS != 0);
}

void log_warn(const char *message) {
    write_line("[WARN] ", message, true);
}

void log_error(const char *message) {
    write_line("[ERR ] ", message, true);
}
