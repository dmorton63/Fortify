#include "fortify/boot.h"
#include "fortify/console.h"
#include "fortify/cpp_runtime.h"
#include "fortify/diag.h"
#include "fortify/log.h"
#include "fortify/serial.h"
#include "fortify/subsystems.h"

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void idle_with_tick_heartbeat(void) {
    uint64_t last_heartbeat = 0;
    for (;;) {
        scheduler_service_deferred_switch();
        __asm__ volatile ("hlt");
        const uint64_t ticks = timer_ticks();
        if ((ticks - last_heartbeat) >= 100) {
            last_heartbeat = ticks;
            log_info("timer: heartbeat (100 ticks).");
        }
    }
}

static bool claim_bootstrap_role(void) {
    static volatile uint32_t g_bootstrap_claimed = 0;
    uint32_t expected = 1;
    __asm__ volatile ("lock xchg %0, %1" : "+r"(expected), "+m"(g_bootstrap_claimed) : : "memory");
    return expected == 0;
}

static bool init_subsystems(void) {
    if (!interrupts_init()) return false;
    if (!memory_init()) return false;
    if (!scheduler_init()) return false;
    if (!smp_init()) return false;
    if (!usb_init()) return false;
    if (!input_init()) return false;
    if (!filesystem_init()) return false;
    if (!port_manager_init()) return false;
    if (!video_init()) return false;
    if (!renderer_init()) return false;
    if (!compositor_init()) return false;
    if (!networking_init()) return false;
    if (!webaccess_init()) return false;
    if (!webcam_init()) return false;
    if (!audio_init()) return false;
    if (!tpm_init()) return false;
    if (!secureboot_init()) return false;
    if (!quantum_sim_init()) return false;

    return true;
}

extern "C" void _start(void) {
    serial_init();
    if (!claim_bootstrap_role()) {
        halt_forever();
    }
    diag_boot_enter();
    diag_mark_phase(kDiagPhaseBootEnter, 0);

    log_info("Starting Fortify kernel...\n");
    if (!boot_protocol_ready()) {
        log_error("Limine base revision not supported. Halting.");
        halt_forever();
    }
    diag_mark_phase(kDiagPhaseBootProtocolReady, 0);

    cpp_runtime_init();
    diag_mark_phase(kDiagPhaseCppRuntimeReady, 0);

    if (!console_init()) {
        log_error("Framebuffer console unavailable. Continuing with serial output only.");
    } else {
        log_info("Framebuffer console initialized.");
    }
    diag_mark_phase(kDiagPhaseConsoleReady, 0);

    log_info("Fortify kernel booted with Limine.");
    diag_mark_phase(kDiagPhaseSubsystemInitBegin, 0);

    if (!init_subsystems()) {
        log_error("Subsystem initialization failed. Halting.");
        halt_forever();
    }
    diag_mark_phase(kDiagPhaseSubsystemInitDone, 0);

    quantum_sim_run_demo();
    log_info("Fortify boot sequence complete.");
    diag_mark_phase(kDiagPhaseSchedulerStart, 0);
    scheduler_start();
    diag_mark_phase(kDiagPhaseIdleLoop, 0);
    idle_with_tick_heartbeat();
}
