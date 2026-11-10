#include "fortify/systems/diagnostics/diag.h"
#include "fortify/systems/serial/serial.h"

#include <stdint.h>

namespace {
constexpr uint64_t kDiagMagic = 0x464F525449465944ULL; // FORTIFYD

extern "C" volatile uint64_t g_worker_rsp_before_sti;
extern "C" volatile uint64_t g_worker_rsp_after_sti;
extern "C" volatile uint64_t g_worker_rsp_first_loop;
extern "C" volatile uint64_t g_worker_rsp_probe_stack_base;
extern "C" volatile uint64_t g_worker_rsp_probe_stack_top;

struct diag_state {
    uint64_t magic;
    uint64_t boot_count;
    uint64_t last_phase;
    uint64_t last_aux;
};

volatile diag_state g_diag_state __attribute__((section(".noinit.diag"))) = {};

void serial_write_hex64(uint64_t value) {
    static const char kHex[] = "0123456789abcdef";
    char out[19] = "0x0000000000000000";
    for (uint64_t i = 0; i < 16; i++) {
        out[17 - i] = kHex[value & 0x0f];
        value >>= 4;
    }
    serial_write(out);
}
}

void diag_boot_enter(void) {
    if (g_diag_state.magic != kDiagMagic) {
        g_diag_state.magic = kDiagMagic;
        g_diag_state.boot_count = 0;
        g_diag_state.last_phase = 0;
        g_diag_state.last_aux = 0;
    }

    g_diag_state.boot_count++;
    serial_write("[DIAG] boot_count=");
    serial_write_hex64(g_diag_state.boot_count);
    serial_write(" prev_phase=");
    serial_write_hex64(g_diag_state.last_phase);
    serial_write(" prev_aux=");
    serial_write_hex64(g_diag_state.last_aux);
    serial_write("\n");

    if (g_worker_rsp_before_sti != 0 || g_worker_rsp_after_sti != 0 || g_worker_rsp_first_loop != 0) {
        serial_write("[DIAG] worker_rsp_snapshot base=");
        serial_write_hex64(g_worker_rsp_probe_stack_base);
        serial_write(" top=");
        serial_write_hex64(g_worker_rsp_probe_stack_top);
        serial_write(" pre_sti=");
        serial_write_hex64(g_worker_rsp_before_sti);
        serial_write(" post_sti=");
        serial_write_hex64(g_worker_rsp_after_sti);
        serial_write(" first_loop=");
        serial_write_hex64(g_worker_rsp_first_loop);
        serial_write("\n");
        g_worker_rsp_before_sti = 0;
        g_worker_rsp_after_sti = 0;
        g_worker_rsp_first_loop = 0;
        g_worker_rsp_probe_stack_base = 0;
        g_worker_rsp_probe_stack_top = 0;
    }
}

void diag_mark_phase(uint64_t phase, uint64_t aux) {
    g_diag_state.last_phase = phase;
    g_diag_state.last_aux = aux;
}
