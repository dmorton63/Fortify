#include "fortify/systems/boot/boot.h"
#include "fortify/systems/console/console.h"

#include <stdint.h>

#ifndef FORTIFY_DISPLAY_LOGS
#define FORTIFY_DISPLAY_LOGS 1
#endif

namespace {
struct limine_framebuffer *g_fb = 0;
uint8_t *g_fb_base = 0;
uint32_t g_fg = 0;
uint32_t g_bg = 0;
uint64_t g_cursor_x = 0;
uint64_t g_cursor_y = 0;
#if FORTIFY_DISPLAY_LOGS
constexpr uint64_t kMaxWriters = 64;
constexpr uint64_t kLineBufferSize = 512;
constexpr uint64_t kNoOwner = ~static_cast<uint64_t>(0);
char g_line_buffer[kMaxWriters][kLineBufferSize];
uint16_t g_line_length[kMaxWriters] = {};
uint8_t g_write_lock = 0;
uint64_t g_write_owner = kNoOwner;
uint32_t g_write_recursion = 0;
#endif

constexpr uint64_t kGlyphW = 6;
constexpr uint64_t kGlyphH = 8;
constexpr uint64_t kScale = 2;

#if FORTIFY_DISPLAY_LOGS
void cpu_pause(void) {
    __asm__ volatile ("pause" : : : "memory");
}
#endif

uint8_t to_upper_ascii(uint8_t c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

void set_rows(uint8_t *rows, uint8_t r0, uint8_t r1, uint8_t r2, uint8_t r3, uint8_t r4, uint8_t r5, uint8_t r6) {
    rows[0] = r0; rows[1] = r1; rows[2] = r2; rows[3] = r3; rows[4] = r4; rows[5] = r5; rows[6] = r6;
}

void glyph_5x7(uint8_t c, uint8_t *rows) {
    set_rows(rows, 0, 0, 0, 0, 0, 0, 0);
    c = to_upper_ascii(c);
    switch (c) {
        case 'A': set_rows(rows, 0x04, 0x0a, 0x11, 0x1f, 0x11, 0x11, 0x11); return;
        case 'B': set_rows(rows, 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e); return;
        case 'C': set_rows(rows, 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e); return;
        case 'D': set_rows(rows, 0x1c, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1c); return;
        case 'E': set_rows(rows, 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f); return;
        case 'F': set_rows(rows, 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10); return;
        case 'G': set_rows(rows, 0x0f, 0x10, 0x10, 0x13, 0x11, 0x11, 0x0f); return;
        case 'H': set_rows(rows, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11); return;
        case 'I': set_rows(rows, 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e); return;
        case 'J': set_rows(rows, 0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e); return;
        case 'K': set_rows(rows, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11); return;
        case 'L': set_rows(rows, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f); return;
        case 'M': set_rows(rows, 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11); return;
        case 'N': set_rows(rows, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11); return;
        case 'O': set_rows(rows, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e); return;
        case 'P': set_rows(rows, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10); return;
        case 'Q': set_rows(rows, 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d); return;
        case 'R': set_rows(rows, 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11); return;
        case 'S': set_rows(rows, 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e); return;
        case 'T': set_rows(rows, 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04); return;
        case 'U': set_rows(rows, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e); return;
        case 'V': set_rows(rows, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04); return;
        case 'W': set_rows(rows, 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a); return;
        case 'X': set_rows(rows, 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11); return;
        case 'Y': set_rows(rows, 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04); return;
        case 'Z': set_rows(rows, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f); return;
        case '0': set_rows(rows, 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e); return;
        case '1': set_rows(rows, 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e); return;
        case '2': set_rows(rows, 0x0e, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1f); return;
        case '3': set_rows(rows, 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e); return;
        case '4': set_rows(rows, 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02); return;
        case '5': set_rows(rows, 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e); return;
        case '6': set_rows(rows, 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e); return;
        case '7': set_rows(rows, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08); return;
        case '8': set_rows(rows, 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e); return;
        case '9': set_rows(rows, 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e); return;
        case '[': set_rows(rows, 0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e); return;
        case ']': set_rows(rows, 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e); return;
        case ':': set_rows(rows, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00); return;
        case '.': set_rows(rows, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06); return;
        case '-': set_rows(rows, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00); return;
        case ' ': return;
        default: set_rows(rows, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x00, 0x08); return;
    }
}

uint32_t compose_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t red = (static_cast<uint32_t>(r) >> (8U - g_fb->red_mask_size)) << g_fb->red_mask_shift;
    uint32_t green = (static_cast<uint32_t>(g) >> (8U - g_fb->green_mask_size)) << g_fb->green_mask_shift;
    uint32_t blue = (static_cast<uint32_t>(b) >> (8U - g_fb->blue_mask_size)) << g_fb->blue_mask_shift;
    return red | green | blue;
}

void plot(uint64_t x, uint64_t y, uint32_t color) {
    if (x >= g_fb->width || y >= g_fb->height) {
        return;
    }
    uint32_t *row = reinterpret_cast<uint32_t *>(g_fb_base + y * g_fb->pitch);
    row[x] = color;
}

void newline_fb(void) {
    g_cursor_x = 0;
    g_cursor_y += kGlyphH * kScale;
    if (g_cursor_y + (kGlyphH * kScale) > g_fb->height) {
        g_cursor_y = 0;
    }
}

__attribute__((unused)) void draw_char(uint8_t c) {
    if (c == '\n') {
        newline_fb();
        return;
    }
    if (c == '\r') {
        g_cursor_x = 0;
        return;
    }
    if (g_cursor_x + (kGlyphW * kScale) > g_fb->width) {
        newline_fb();
    }

    uint8_t rows[7];
    glyph_5x7(c, rows);

    for (uint64_t gy = 0; gy < 7; gy++) {
        for (uint64_t gx = 0; gx < 5; gx++) {
            uint32_t color = (rows[gy] & (1U << (4 - gx))) ? g_fg : g_bg;
            for (uint64_t sy = 0; sy < kScale; sy++) {
                for (uint64_t sx = 0; sx < kScale; sx++) {
                    plot(g_cursor_x + gx * kScale + sx, g_cursor_y + gy * kScale + sy, color);
                }
            }
        }
    }

    g_cursor_x += kGlyphW * kScale;
}

#if FORTIFY_DISPLAY_LOGS
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
        draw_char(static_cast<uint8_t>(text[i]));
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
#endif
}

bool console_init(void) {
    struct limine_framebuffer_response *fb = boot_framebuffer_response();
    if (fb == 0 || fb->framebuffer_count == 0 || fb->framebuffers == 0 || fb->framebuffers[0] == 0) {
        return false;
    }

    g_fb = fb->framebuffers[0];
    if (g_fb->address == 0 || g_fb->memory_model != LIMINE_FRAMEBUFFER_RGB || g_fb->bpp != 32) {
        return false;
    }
    if (g_fb->width == 0 || g_fb->height == 0 || g_fb->width > 8192 || g_fb->height > 4320) {
        return false;
    }
    if (g_fb->pitch < g_fb->width * 4) {
        return false;
    }

    uintptr_t addr = reinterpret_cast<uintptr_t>(g_fb->address);
    struct limine_hhdm_response *hhdm = boot_hhdm_response();
    if (addr < (1ULL << 32) && hhdm != 0) {
        addr += static_cast<uintptr_t>(hhdm->offset);
    }
    if (addr == 0) {
        return false;
    }
    g_fb_base = reinterpret_cast<uint8_t *>(addr);

    g_fg = compose_rgb(220, 220, 220);
    g_bg = compose_rgb(8, 12, 20);
    g_cursor_x = 0;
    g_cursor_y = 0;
    return true;
}

void console_write(const char *text) {
#if !FORTIFY_DISPLAY_LOGS
    (void)text;
    return;
#else
    if (g_fb == 0 || g_fb_base == 0 || text == 0) {
        return;
    }

    const uint64_t slot = writer_slot();
    lock_writer(slot);
    for (uint64_t i = 0; text[i] != '\0'; i++) {
        append_char(slot, text[i]);
    }
    unlock_writer(slot);
#endif
}
