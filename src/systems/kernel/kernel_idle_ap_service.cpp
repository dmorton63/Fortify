#include "fortify/systems/kernel/kernel_idle_ap_service.h"

#include "fortify/systems/kernel/ap_trigger_profile.h"
#include "fortify/systems/kernel/smp_diagnostics.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

extern "C" volatile uint64_t g_ap_trigger_profile;

namespace {
constexpr uint64_t kSmpCounterCap = 64;
constexpr uint64_t kCounterPollTicks = 100;
#if defined(FORTIFY_AP_STRESS)
constexpr uint64_t kPeriodicApKickTicks = 100;
#else
constexpr uint64_t kPeriodicApKickTicks = 500;
#endif

uint64_t last_heartbeat = 0;
uint64_t last_periodic_ap_kick_tick = 0;
uint64_t last_online_cpus = 0;
uint64_t last_ap_ready = 0;
uint64_t last_ap_trigger_profile = ~0ULL;
uint64_t smp_kick_attempts = 0;
bool smp_kick_unavailable_logged = false;
bool smp_periodic_kick_unavailable_logged = false;
bool smp_partial_online_logged = false;
#if defined(FORTIFY_AP_STRESS)
bool ap_pinned_task_autostart_attempted = false;
#endif
} // namespace

bool kernel_idle_poll_ap_service(uint64_t ticks, kernel_idle_ap_poll_result *out_result) {
    if (out_result == 0 || (ticks - last_heartbeat) < kCounterPollTicks) {
        return false;
    }
    last_heartbeat = ticks;

    uint64_t requested_profile = __atomic_load_n(&g_ap_trigger_profile, __ATOMIC_RELAXED);
    if (requested_profile != last_ap_trigger_profile) {
        uint64_t applied_profile = requested_profile;
        if (!apply_ap_trigger_profile(applied_profile)) {
            applied_profile = kApTriggerProfileDefault;
            if (!apply_ap_trigger_profile(applied_profile)) {
                log_warn("smp: failed to apply AP trigger profile.");
            } else {
                log_warn("smp: unknown AP trigger profile requested, reverted to default.");
            }
            __atomic_store_n(&g_ap_trigger_profile, applied_profile, __ATOMIC_RELAXED);
        }
        if (applied_profile != last_ap_trigger_profile) {
            log_ap_trigger_profile(applied_profile);
            last_ap_trigger_profile = applied_profile;
        }
    }
    const uint64_t online = smp_online_cpu_count();
    const uint64_t total = smp_total_cpu_count();
    const uint64_t ap_ready = smp_ap_interruptible_ready_count();
    if (online != last_online_cpus) {
        last_online_cpus = online;
        log_smp_online_update(online, total, scheduler_online_cpu_count());
    }
    if (ap_ready != last_ap_ready && total > 1) {
        last_ap_ready = ap_ready;
        log_smp_ap_ready_update(ap_ready, total - 1);
    }
    if (total > 1 && online != total && !smp_partial_online_logged) {
        smp_partial_online_logged = true;
        log_warn("smp: online CPU set below expected total; using currently online AP set for self-test/work dispatch.");
    } else if (online == total) {
        smp_partial_online_logged = false;
    }
    const bool smp_work_ready = (total > 1) && (online > 1) && (ap_ready != 0);
#if defined(FORTIFY_AP_STRESS)
    if (!ap_pinned_task_autostart_attempted &&
        smp_work_ready) {
        ap_pinned_task_autostart_attempted = true;
        const bool started = scheduler_start_ap_pinned_demo_tasks();
        if (started) {
            scheduler_set_ap_pinned_task_migration_enabled(true);
            log_info("smp: AP-pinned demo tasks auto-started (stress mode).");
            log_info("smp: AP-pinned task migration enabled (stress mode).");
        } else {
            log_warn("smp: AP-pinned demo task auto-start found no AP targets.");
        }
    }
#endif
    const uint64_t ipi_observed_aps = smp_ipi_kick_observed_ap_count();
    if (smp_work_ready &&
        ipi_observed_aps == 0 &&
        smp_kick_attempts < 5) {
        smp_kick_attempts++;
        const bool kicked = smp_kick_all_aps();
        if (kicked && smp_kick_attempts == 1) {
            log_info("smp: issued BSP IPI kick to AP set.");
            log_smp_kick_mode(smp_ipi_kick_last_mode());
            log_smp_ipi_diag_snapshot();
        } else if (kicked) {
            log_smp_kick_mode(smp_ipi_kick_last_mode());
            log_smp_ipi_diag_snapshot();
        } else if (!kicked && !smp_kick_unavailable_logged) {
            smp_kick_unavailable_logged = true;
            log_warn("smp: BSP IPI kick unavailable (APIC/x2APIC/MMIO path not ready).");
        }
    }
    if (smp_work_ready &&
        ipi_observed_aps != 0 &&
        (ticks - last_periodic_ap_kick_tick) >= kPeriodicApKickTicks) {
        last_periodic_ap_kick_tick = ticks;
        bool requested_work = false;
        for (uint64_t cpu = 1; cpu < total && cpu < kSmpCounterCap; cpu++) {
            const bool usb_requested = usb_poll_ap_processing_trigger(cpu, ticks);
            const bool audio_requested = audio_poll_ap_buffer_trigger(cpu, ticks);
            const bool webcam_requested = webcam_poll_ap_processing_trigger(cpu, ticks);
            const bool port_requested = port_manager_poll_ap_maintenance_trigger(cpu, ticks);
            const bool filesystem_requested = filesystem_poll_ap_maintenance_trigger(cpu, ticks);
            const bool networking_requested = networking_poll_ap_cleanup_trigger(cpu, ticks);
            const bool input_requested = input_poll_ap_processing_trigger(cpu, ticks);
            requested_work =
                requested_work || usb_requested || audio_requested ||
                webcam_requested || port_requested || filesystem_requested ||
                networking_requested || input_requested;
        }
        requested_work = scheduler_dispatch_ap_pinned_tasks() || requested_work;
        if (requested_work) {
            const bool kicked = smp_kick_all_aps();
            if (!kicked && !smp_periodic_kick_unavailable_logged) {
                smp_periodic_kick_unavailable_logged = true;
                log_warn("smp: periodic AP kick unavailable.");
            }
        }
    }

    out_result->online = online;
    out_result->total = total;
    out_result->ipi_observed_aps = ipi_observed_aps;
    out_result->kick_attempts_exhausted = (smp_kick_attempts >= 5);
    return true;
}
