#include "fortify/serial.h"

#include <stdint.h>

namespace {
constexpr uint16_t kCom1Base = 0x3F8;
bool g_serial_ready = false;

void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

bool can_transmit(void) {
    return (inb(kCom1Base + 5) & 0x20) != 0;
}
}

void serial_init(void) {
    outb(kCom1Base + 1, 0x00);
    outb(kCom1Base + 3, 0x80);
    outb(kCom1Base + 0, 0x03);
    outb(kCom1Base + 1, 0x00);
    outb(kCom1Base + 3, 0x03);
    outb(kCom1Base + 2, 0xC7);
    outb(kCom1Base + 4, 0x0B);
    g_serial_ready = true;
}

void serial_write(const char *text) {
    if (!g_serial_ready || text == 0) {
        return;
    }

    for (uint64_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            while (!can_transmit()) {}
            outb(kCom1Base, '\r');
        }
        while (!can_transmit()) {}
        outb(kCom1Base, static_cast<uint8_t>(text[i]));
    }
}
