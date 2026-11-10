#include "fortify/systems/boot/boot.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/runtime/cpp_runtime.h"
#include "fortify/systems/diagnostics/diag.h"
#include "fortify/systems/kernel/fs_mock_bridge.h"
#include "fortify/systems/kernel/runtime_commands.h"
#include "fortify/systems/kernel/kernel_idle_loop.h"
#include "fortify/systems/kernel/smp_diagnostics.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

#ifndef FORTIFY_QEMU_SHARE_ENABLE
#define FORTIFY_QEMU_SHARE_ENABLE 0
#endif

#ifndef FORTIFY_QEMU_SHARE_TAG
#define FORTIFY_QEMU_SHARE_TAG "fortifyshare"
#endif

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

extern "C" volatile uint64_t g_ap_trigger_profile __attribute__((section(".noinit.diag"))) = 0;

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
    kernel_configure_filesystem_sector_reader_bridge();
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

static void log_filesystem_backend_mode(void) {
    if (filesystem_has_sector_reader()) {
        log_info("filesystem: sector backend active=external-callback.");
    } else {
        log_info("filesystem: sector backend active=synthetic-fallback.");
    }
    if (filesystem_virtio_9p_device_total() > 0) {
        log_info("filesystem: runtime virtio-9p transport present.");
    } else if (FORTIFY_QEMU_SHARE_ENABLE) {
        log_warn("filesystem: qemu share requested but virtio-9p transport not detected.");
    } else {
        log_info("filesystem: qemu share transport not requested.");
    }
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
    log_filesystem_backend_mode();

    quantum_sim_run_demo();
    log_info("Fortify boot sequence complete.");
#if defined(FORTIFY_AP_STRESS)
    log_warn("smp: AP stress mode enabled.");
#endif
    diag_mark_phase(kDiagPhaseSchedulerStart, 0);
    scheduler_start();
    diag_mark_phase(kDiagPhaseIdleLoop, 0);
    kernel_idle_with_tick_heartbeat();
}
