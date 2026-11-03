#include "fortify/console.h"
#include "fortify/log.h"
#include "fortify/serial.h"

static void write_line(const char *prefix, const char *message) {
    serial_write(prefix);
    serial_write(message);
    serial_write("\n");
    console_write(prefix);
    console_write(message);
    console_write("\n");
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
