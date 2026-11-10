#include "fortify/systems/console/console.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/serial/serial.h"

#ifndef FORTIFY_DISPLAY_LOGS
#define FORTIFY_DISPLAY_LOGS 1
#endif

static void write_line(const char *prefix, const char *message) {
    serial_write(prefix);
    serial_write(message);
    serial_write("\n");
#if FORTIFY_DISPLAY_LOGS
    console_write(prefix);
    console_write(message);
    console_write("\n");
#endif
}

void log_info(const char *message) {
    write_line("[INFO] ", message);
}

void log_warn(const char *message) {
    write_line("[WARN] ", message);
}

void log_error(const char *message) {
    write_line("[ERR ] ", message);
}
