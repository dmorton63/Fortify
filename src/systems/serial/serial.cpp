#include "fortify/systems/serial/serial.h"
#include "fortify/systems/boot/boot.h"

#include <stdint.h>

namespace {
constexpr uint16_t kCom1Base = 0x3F8;
constexpr uint64_t kMaxWriters = 64;
constexpr uint64_t kLineBufferSize = 512;
constexpr uint64_t kNoOwner = ~static_cast<uint64_t>(0);
bool g_serial_ready = false;
char g_line_buffer[kMaxWriters][kLineBufferSize];
uint16_t g_line_length[kMaxWriters] = {};
uint8_t g_write_lock = 0;
uint64_t g_write_owner = kNoOwner;
uint32_t g_write_recursion = 0;

void cpu_pause(void) {
    __asm__ volatile ("pause" : : : "memory");
}

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

bool can_receive(void) {
    return (inb(kCom1Base + 5) & 0x01) != 0;
}

uint64_t writer_slot(void) {
    return boot_current_cpu_index() % kMaxWriters;
}

void lock_writer(uint64_t slot) {
    if (__atomic_load_n(&g_write_owner, __ATOMIC_RELAXED) == slot) {
        __atomic_add_fetch(&g_write_recursion, 1, __ATOMIC_RELAXED);
        return;
    }
    while (__atomic_exchange_n(&g_write_lock, 1, __ATOMIC_ACQUIRE) != 0) {
        cpu_pause();
    }
    __atomic_store_n(&g_write_owner, slot, __ATOMIC_RELAXED);
    __atomic_store_n(&g_write_recursion, 1, __ATOMIC_RELAXED);
}

void unlock_writer(uint64_t slot) {
    if (__atomic_load_n(&g_write_owner, __ATOMIC_RELAXED) != slot) {
        return;
    }
    const uint32_t depth = __atomic_load_n(&g_write_recursion, __ATOMIC_RELAXED);
    if (depth > 1) {
        __atomic_sub_fetch(&g_write_recursion, 1, __ATOMIC_RELAXED);
        return;
    }
    __atomic_store_n(&g_write_recursion, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_write_owner, kNoOwner, __ATOMIC_RELAXED);
    __atomic_store_n(&g_write_lock, 0, __ATOMIC_RELEASE);
}

void write_raw(const char *text) {
    for (uint64_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            while (!can_transmit()) {}
            outb(kCom1Base, '\r');
        }
        while (!can_transmit()) {}
        outb(kCom1Base, static_cast<uint8_t>(text[i]));
    }
}

void flush_line_buffer(uint64_t slot) {
    uint16_t length = g_line_length[slot];
    if (length == 0) {
        return;
    }
    g_line_buffer[slot][length] = '\0';
    write_raw(g_line_buffer[slot]);
    g_line_length[slot] = 0;
}

void append_char(uint64_t slot, char c) {
    if (g_line_length[slot] >= (kLineBufferSize - 1)) {
        flush_line_buffer(slot);
    }
    g_line_buffer[slot][g_line_length[slot]] = c;
    g_line_length[slot]++;
    if (c == '\n') {
        flush_line_buffer(slot);
    }
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

    const uint64_t slot = writer_slot();
    lock_writer(slot);
    for (uint64_t i = 0; text[i] != '\0'; i++) {
        append_char(slot, text[i]);
    }
    unlock_writer(slot);
}

bool serial_read_char(char *out) {
    if (!g_serial_ready || out == 0 || !can_receive()) {
        return false;
    }
    *out = static_cast<char>(inb(kCom1Base));
    return true;
}
