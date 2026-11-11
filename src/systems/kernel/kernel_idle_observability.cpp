#include "fortify/systems/kernel/kernel_idle_observability.h"

#include "fortify/systems/kernel/kernel_idle_ap_service.h"
#include "fortify/systems/kernel/runtime_message_router.h"
#include "fortify/systems/kernel/smp_diagnostics.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

extern "C" volatile uint64_t g_ap_trigger_profile;

namespace {
constexpr uint64_t kSmpCounterCap = 64;
constexpr uint64_t kHeartbeatLogTicks = 500;
constexpr uint64_t kSmpHealthSnapshotTicks = 1000;
constexpr uint64_t kPerCpuDetailLogTicks = 1000;
constexpr uint64_t kApWorkPendingDebounceIntervals = 2;
constexpr uint64_t kRuntimeMsgBacklogWarnDepthDefault = 16;
constexpr uint64_t kRuntimeMsgBacklogRecoverDepth = 0;
constexpr uint64_t kRuntimeMsgBacklogTransitionTicksDefault = 25;

uint64_t last_heartbeat_log = 0;
uint64_t last_ap_wake_total = 0;
uint64_t last_ipi_observed_total = 0;
uint64_t last_ap_nonkick_interrupt_total = 0;
uint64_t last_ipi_scheduler_total = 0;
uint64_t last_ap_work_dispatch_total = 0;
uint64_t last_ap_work_execute_total = 0;
uint64_t last_ap_maintenance_dispatch_total = 0;
uint64_t last_ap_maintenance_execute_total = 0;
uint64_t last_ap_networking_dispatch_total = 0;
uint64_t last_ap_networking_execute_total = 0;
uint64_t last_port_ap_job_total = 0;
uint64_t last_networking_ap_job_total = 0;
uint64_t last_networking_packets_total = 0;
uint64_t last_networking_batches_total = 0;
uint64_t last_filesystem_ap_job_total = 0;
uint64_t last_usb_ap_job_total = 0;
uint64_t last_ap_work_skip_rate_limit_total = ~0ULL;
uint64_t last_ap_work_skip_pending_cap_total = ~0ULL;
uint64_t last_ap_work_drop_total = 0;
uint64_t last_ap_work_queue_depth_total = 0;
uint64_t last_per_cpu_detail_log_tick = 0;
uint64_t last_smp_health_snapshot_tick = 0;
uint64_t last_self_test_log_tick = 0;
bool self_test_logged = false;
bool last_ap_work_pass = false;
bool last_migration_pass = false;
uint64_t ap_work_pending_streak = 0;
uint64_t last_ipi_scheduler_by_cpu[kSmpCounterCap];
uint64_t last_ap_work_dispatch_by_cpu[kSmpCounterCap];
uint64_t last_ap_work_execute_by_cpu[kSmpCounterCap];
uint64_t last_ap_maintenance_dispatch_by_cpu[kSmpCounterCap];
uint64_t last_ap_maintenance_execute_by_cpu[kSmpCounterCap];
uint64_t last_ap_networking_dispatch_by_cpu[kSmpCounterCap];
uint64_t last_ap_networking_execute_by_cpu[kSmpCounterCap];
uint64_t last_port_ap_job_by_cpu[kSmpCounterCap];
uint64_t last_networking_ap_job_by_cpu[kSmpCounterCap];
uint64_t last_networking_packets_by_cpu[kSmpCounterCap];
uint64_t last_networking_batches_by_cpu[kSmpCounterCap];
uint64_t last_filesystem_ap_job_by_cpu[kSmpCounterCap];
uint64_t last_usb_ap_job_by_cpu[kSmpCounterCap];
uint64_t last_ap_work_drop_by_cpu[kSmpCounterCap];
bool smp_kick_observation_logged = false;
bool runtime_msg_input_backlog_warned = false;
bool runtime_msg_command_backlog_warned = false;
uint64_t runtime_msg_input_backlog_warn_since_tick = 0;
uint64_t runtime_msg_input_backlog_recover_since_tick = 0;
uint64_t runtime_msg_command_backlog_warn_since_tick = 0;
uint64_t runtime_msg_command_backlog_recover_since_tick = 0;
uint64_t runtime_msg_backlog_warn_depth = kRuntimeMsgBacklogWarnDepthDefault;
uint64_t runtime_msg_backlog_transition_ticks = kRuntimeMsgBacklogTransitionTicksDefault;

void poll_runtime_message_backlog_guardrail(const char *warn_label,
                                            const char *recover_label,
                                            uint64_t queue_depth,
                                            bool *backlog_warned,
                                            uint64_t *warn_since_tick,
                                            uint64_t *recover_since_tick,
                                            uint64_t ticks) {
    if (queue_depth >= runtime_msg_backlog_warn_depth) {
        *recover_since_tick = 0;
        if (!*backlog_warned) {
            if (*warn_since_tick == 0) {
                *warn_since_tick = ticks;
            } else if ((ticks - *warn_since_tick) >= runtime_msg_backlog_transition_ticks) {
                *backlog_warned = true;
                *warn_since_tick = 0;
                log_warn(warn_label);
            }
        }
        return;
    }

    *warn_since_tick = 0;
    if (*backlog_warned && queue_depth <= kRuntimeMsgBacklogRecoverDepth) {
        if (*recover_since_tick == 0) {
            *recover_since_tick = ticks;
        } else if ((ticks - *recover_since_tick) >= runtime_msg_backlog_transition_ticks) {
            *backlog_warned = false;
            *recover_since_tick = 0;
            log_info(recover_label);
        }
    } else {
        *recover_since_tick = 0;
    }
}
} // namespace

void kernel_set_runtime_message_backlog_guardrail(uint64_t warn_depth, uint64_t transition_ticks) {
    if (warn_depth == 0) {
        warn_depth = 1;
    }
    if (transition_ticks == 0) {
        transition_ticks = 1;
    }
    runtime_msg_backlog_warn_depth = warn_depth;
    runtime_msg_backlog_transition_ticks = transition_ticks;
    runtime_msg_input_backlog_warned = false;
    runtime_msg_command_backlog_warned = false;
    runtime_msg_input_backlog_warn_since_tick = 0;
    runtime_msg_input_backlog_recover_since_tick = 0;
    runtime_msg_command_backlog_warn_since_tick = 0;
    runtime_msg_command_backlog_recover_since_tick = 0;
}

uint64_t kernel_runtime_message_backlog_warn_depth(void) {
    return runtime_msg_backlog_warn_depth;
}

uint64_t kernel_runtime_message_backlog_transition_ticks(void) {
    return runtime_msg_backlog_transition_ticks;
}

void kernel_idle_poll_smp_observability(uint64_t ticks) {
    kernel_idle_ap_poll_result ap_poll = {};
    if (!kernel_idle_poll_ap_service(ticks, &ap_poll)) {
        return;
    }
    const uint64_t online = ap_poll.online;
    const uint64_t total = ap_poll.total;
    const uint64_t ipi_observed_aps = ap_poll.ipi_observed_aps;
    const uint64_t wake_total = smp_ap_wake_count_total();
    const uint64_t ipi_observed_total = smp_ipi_kick_observed_count();
    const uint64_t ap_nonkick_interrupt_total = smp_ap_nonkick_interrupt_count();
    const uint64_t ipi_scheduler_total = scheduler_ipi_preempt_requests_total();
    const uint64_t ap_work_dispatch_total = scheduler_ap_work_dispatch_total();
    const uint64_t ap_work_execute_total = scheduler_ap_work_execute_total();
    const uint64_t ap_maintenance_dispatch_total = scheduler_ap_work_maintenance_dispatch_total();
    const uint64_t ap_maintenance_execute_total = scheduler_ap_work_maintenance_execute_total();
    const uint64_t ap_networking_dispatch_total = scheduler_ap_work_networking_dispatch_total();
    const uint64_t ap_networking_execute_total = scheduler_ap_work_networking_execute_total();
    const uint64_t port_ap_job_total = port_manager_ap_deferred_job_total();
    const uint64_t networking_ap_job_total = networking_ap_deferred_job_total();
    const uint64_t networking_packets_total = networking_rx_packets_processed_total();
    const uint64_t networking_batches_total = networking_rx_batches_total();
    const uint64_t networking_irq_total = networking_rx_irq_total();
    const uint64_t filesystem_ap_job_total = filesystem_ap_deferred_job_total();
    const uint64_t usb_ap_job_total = usb_ap_deferred_job_total();
    const uint64_t storage_irq_total = filesystem_storage_irq_total();
    const uint64_t input_irq_total = input_keyboard_irq_total();
    const uint64_t usb_irq_total = usb_host_irq_total();
    const uint64_t audio_irq_total_count = audio_irq_total();
    const uint64_t audio_ap_job_total = audio_ap_deferred_job_total();
    const uint64_t ap_work_skip_rate_limit_total = scheduler_ap_work_skip_rate_limit_total();
    const uint64_t ap_work_skip_pending_cap_total = scheduler_ap_work_skip_pending_cap_total();
    const uint64_t ap_work_drop_total = scheduler_ap_work_drop_total();
    const uint64_t ap_pinned_active_total = scheduler_ap_pinned_task_active_count();
    const uint64_t ap_pinned_execute_total = scheduler_ap_pinned_task_execute_total();
    const uint64_t ap_pinned_migrations_total = scheduler_ap_pinned_task_migration_total();
    const uint64_t ap_pinned_seen_cpu_mask_total = scheduler_ap_pinned_task_seen_cpu_mask();
    const uint64_t ap_to_ap_switch_total = scheduler_ap_to_ap_switch_total();
    const uint64_t bsp_to_ap_switch_total = scheduler_bsp_to_ap_switch_total();
    const uint64_t runtime_msg_queue_high_water = runtime_message_router_queue_high_water();
    const uint64_t runtime_msg_input_queue_high_water =
        runtime_message_router_queue_high_water_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_msg_command_queue_high_water =
        runtime_message_router_queue_high_water_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_msg_dispatch_input_total = runtime_message_router_dispatch_input_total();
    const uint64_t runtime_msg_dispatch_command_total = runtime_message_router_dispatch_command_total();
    const uint64_t runtime_msg_route_drop_total = runtime_message_router_route_drop_total();
    const uint64_t runtime_msg_route_drop_invalid_source_total =
        runtime_message_router_route_drop_invalid_source_total();
    const uint64_t runtime_msg_route_drop_keyboard_suppressed_total =
        runtime_message_router_route_drop_keyboard_suppressed_total();
    const uint64_t runtime_msg_route_drop_unroutable_total =
        runtime_message_router_route_drop_unroutable_total();
    const uint64_t runtime_msg_input_queue_depth =
        runtime_message_router_queue_depth_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_msg_command_queue_depth =
        runtime_message_router_queue_depth_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_msg_input_queue_drop =
        runtime_message_router_drop_total_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_msg_command_queue_drop =
        runtime_message_router_drop_total_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_msg_input_queue_enqueued =
        runtime_message_router_enqueued_total_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_msg_input_queue_dequeued =
        runtime_message_router_dequeued_total_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_msg_command_queue_enqueued =
        runtime_message_router_enqueued_total_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_msg_command_queue_dequeued =
        runtime_message_router_dequeued_total_for_target(runtime_message_target::runtime_command);
    poll_runtime_message_backlog_guardrail(
        "smp: runtime message router input queue backlog warning.",
        "smp: runtime message router input queue backlog recovered.",
        runtime_msg_input_queue_depth,
        &runtime_msg_input_backlog_warned,
        &runtime_msg_input_backlog_warn_since_tick,
        &runtime_msg_input_backlog_recover_since_tick,
        ticks);
    poll_runtime_message_backlog_guardrail(
        "smp: runtime message router command queue backlog warning.",
        "smp: runtime message router command queue backlog recovered.",
        runtime_msg_command_queue_depth,
        &runtime_msg_command_backlog_warned,
        &runtime_msg_command_backlog_warn_since_tick,
        &runtime_msg_command_backlog_recover_since_tick,
        ticks);
    const bool log_detailed_counters =
        !self_test_logged ||
        !last_ap_work_pass ||
        (ticks - last_per_cpu_detail_log_tick) >= kPerCpuDetailLogTicks;
    if (log_detailed_counters) {
        last_per_cpu_detail_log_tick = ticks;
    }
    if (!smp_kick_observation_logged &&
        (ipi_observed_aps != 0 || ap_poll.kick_attempts_exhausted)) {
        smp_kick_observation_logged = true;
        if (wake_total == 0 && ipi_observed_aps == 0) {
            log_warn("smp: AP wake counter unchanged after BSP kick.");
        } else {
            last_ap_wake_total = wake_total;
            log_smp_ap_wake_update(wake_total);
        }
        if (ipi_observed_aps == 0) {
            log_warn("smp: no AP IPI-kick interrupt observed yet.");
            if (ipi_observed_total != 0) {
                log_warn("smp: IPI was observed only on non-AP path (likely BSP/self).");
            }
        } else {
            last_ipi_observed_total = ipi_observed_aps;
            log_smp_ipi_observed_update(ipi_observed_aps);
        }
    }
    if (wake_total != last_ap_wake_total) {
        last_ap_wake_total = wake_total;
        log_smp_ap_wake_update(wake_total);
    }
    if (ipi_observed_aps != last_ipi_observed_total) {
        last_ipi_observed_total = ipi_observed_aps;
        log_smp_ipi_observed_update(ipi_observed_aps);
    }
    if (ap_nonkick_interrupt_total != last_ap_nonkick_interrupt_total) {
        last_ap_nonkick_interrupt_total = ap_nonkick_interrupt_total;
        log_info("smp: AP observed non-kick interrupt activity.");
    }
    if (ipi_scheduler_total != last_ipi_scheduler_total) {
        last_ipi_scheduler_total = ipi_scheduler_total;
        if (log_detailed_counters) {
            log_smp_ipi_scheduler_update(ipi_scheduler_total);
        }
    }
    if (ap_work_dispatch_total != last_ap_work_dispatch_total) {
        last_ap_work_dispatch_total = ap_work_dispatch_total;
        if (log_detailed_counters) {
            log_smp_ap_work_dispatch_total(ap_work_dispatch_total);
        }
    }
    if (ap_work_execute_total != last_ap_work_execute_total) {
        last_ap_work_execute_total = ap_work_execute_total;
        if (log_detailed_counters) {
            log_smp_ap_work_execute_total(ap_work_execute_total);
        }
    }
    if (ap_maintenance_dispatch_total != last_ap_maintenance_dispatch_total) {
        last_ap_maintenance_dispatch_total = ap_maintenance_dispatch_total;
        if (log_detailed_counters) {
            log_smp_ap_maintenance_dispatch_total(ap_maintenance_dispatch_total);
        }
    }
    if (ap_maintenance_execute_total != last_ap_maintenance_execute_total) {
        last_ap_maintenance_execute_total = ap_maintenance_execute_total;
        if (log_detailed_counters) {
            log_smp_ap_maintenance_execute_total(ap_maintenance_execute_total);
        }
    }
    if (ap_networking_dispatch_total != last_ap_networking_dispatch_total) {
        last_ap_networking_dispatch_total = ap_networking_dispatch_total;
        if (log_detailed_counters) {
            log_smp_ap_networking_dispatch_total(ap_networking_dispatch_total);
        }
    }
    if (ap_networking_execute_total != last_ap_networking_execute_total) {
        last_ap_networking_execute_total = ap_networking_execute_total;
        if (log_detailed_counters) {
            log_smp_ap_networking_execute_total(ap_networking_execute_total);
        }
    }
    if (port_ap_job_total != last_port_ap_job_total) {
        last_port_ap_job_total = port_ap_job_total;
        if (log_detailed_counters) {
            log_smp_port_ap_job_total(port_ap_job_total);
        }
    }
    if (networking_ap_job_total != last_networking_ap_job_total) {
        last_networking_ap_job_total = networking_ap_job_total;
        if (log_detailed_counters) {
            log_smp_networking_ap_job_total(networking_ap_job_total);
        }
    }
    if (networking_packets_total != last_networking_packets_total) {
        last_networking_packets_total = networking_packets_total;
        log_smp_networking_rx_packets_total(networking_packets_total);
    }
    if (networking_batches_total != last_networking_batches_total) {
        last_networking_batches_total = networking_batches_total;
        log_smp_networking_rx_batches_total(networking_batches_total);
    }
    if (filesystem_ap_job_total != last_filesystem_ap_job_total) {
        last_filesystem_ap_job_total = filesystem_ap_job_total;
        if (log_detailed_counters) {
            log_smp_filesystem_ap_job_total(filesystem_ap_job_total);
        }
    }
    if (usb_ap_job_total != last_usb_ap_job_total) {
        last_usb_ap_job_total = usb_ap_job_total;
        if (log_detailed_counters) {
            log_smp_usb_ap_job_total(usb_ap_job_total);
        }
    }
    if (ap_work_skip_rate_limit_total != last_ap_work_skip_rate_limit_total) {
        last_ap_work_skip_rate_limit_total = ap_work_skip_rate_limit_total;
        if (log_detailed_counters) {
            log_smp_ap_work_skip_rate_limit_total(ap_work_skip_rate_limit_total);
        }
    }
    if (ap_work_skip_pending_cap_total != last_ap_work_skip_pending_cap_total) {
        last_ap_work_skip_pending_cap_total = ap_work_skip_pending_cap_total;
        if (log_detailed_counters) {
            log_smp_ap_work_skip_pending_cap_total(ap_work_skip_pending_cap_total);
        }
    }
    if (ap_work_drop_total != last_ap_work_drop_total) {
        last_ap_work_drop_total = ap_work_drop_total;
        log_smp_ap_work_drop_total(ap_work_drop_total);
    }
    const uint64_t cpu_limit = (total < kSmpCounterCap) ? total : kSmpCounterCap;
    uint64_t ap_expected = online > 0 ? (online - 1) : 0;
    const uint64_t max_ap_track = cpu_limit > 0 ? (cpu_limit - 1) : 0;
    if (ap_expected > max_ap_track) {
        ap_expected = max_ap_track;
    }
    uint64_t ap_work_ok = 0;
    bool ap_work_pass = (ap_work_drop_total == 0);
    uint64_t ap_work_queue_depth_total = 0;
    uint64_t ap_work_queue_nonzero_cpus = 0;
    for (uint64_t i = 1; i < cpu_limit; i++) {
        const uint64_t count = scheduler_ipi_preempt_requests_for_cpu(i);
        if (count != last_ipi_scheduler_by_cpu[i]) {
            last_ipi_scheduler_by_cpu[i] = count;
            if (log_detailed_counters) {
                log_smp_ipi_scheduler_cpu_update(i, count);
            }
        }
        const uint64_t dispatches = scheduler_ap_work_dispatch_for_cpu(i);
        if (dispatches != last_ap_work_dispatch_by_cpu[i]) {
            last_ap_work_dispatch_by_cpu[i] = dispatches;
            if (log_detailed_counters) {
                log_smp_ap_work_dispatch_cpu(i, dispatches);
            }
        }
        const uint64_t executes = scheduler_ap_work_execute_for_cpu(i);
        if (executes != last_ap_work_execute_by_cpu[i]) {
            last_ap_work_execute_by_cpu[i] = executes;
            if (log_detailed_counters) {
                log_smp_ap_work_execute_cpu(i, executes);
            }
        }
        const uint64_t maintenance_dispatches =
            scheduler_ap_work_maintenance_dispatch_for_cpu(i);
        if (maintenance_dispatches != last_ap_maintenance_dispatch_by_cpu[i]) {
            last_ap_maintenance_dispatch_by_cpu[i] = maintenance_dispatches;
            if (log_detailed_counters) {
                log_smp_ap_maintenance_dispatch_cpu(i, maintenance_dispatches);
            }
        }
        const uint64_t maintenance_executes =
            scheduler_ap_work_maintenance_execute_for_cpu(i);
        if (maintenance_executes != last_ap_maintenance_execute_by_cpu[i]) {
            last_ap_maintenance_execute_by_cpu[i] = maintenance_executes;
            if (log_detailed_counters) {
                log_smp_ap_maintenance_execute_cpu(i, maintenance_executes);
            }
        }
        const uint64_t networking_dispatches =
            scheduler_ap_work_networking_dispatch_for_cpu(i);
        if (networking_dispatches != last_ap_networking_dispatch_by_cpu[i]) {
            last_ap_networking_dispatch_by_cpu[i] = networking_dispatches;
            if (log_detailed_counters) {
                log_smp_ap_networking_dispatch_cpu(i, networking_dispatches);
            }
        }
        const uint64_t networking_executes =
            scheduler_ap_work_networking_execute_for_cpu(i);
        if (networking_executes != last_ap_networking_execute_by_cpu[i]) {
            last_ap_networking_execute_by_cpu[i] = networking_executes;
            if (log_detailed_counters) {
                log_smp_ap_networking_execute_cpu(i, networking_executes);
            }
        }
        const uint64_t port_ap_jobs = port_manager_ap_deferred_job_for_cpu(i);
        if (port_ap_jobs != last_port_ap_job_by_cpu[i]) {
            last_port_ap_job_by_cpu[i] = port_ap_jobs;
            if (log_detailed_counters) {
                log_smp_port_ap_job_cpu(i, port_ap_jobs);
            }
        }
        const uint64_t networking_ap_jobs = networking_ap_deferred_job_for_cpu(i);
        if (networking_ap_jobs != last_networking_ap_job_by_cpu[i]) {
            last_networking_ap_job_by_cpu[i] = networking_ap_jobs;
            if (log_detailed_counters) {
                log_smp_networking_ap_job_cpu(i, networking_ap_jobs);
            }
        }
        const uint64_t networking_packets = networking_rx_packets_processed_for_cpu(i);
        if (networking_packets != last_networking_packets_by_cpu[i]) {
            last_networking_packets_by_cpu[i] = networking_packets;
            log_smp_networking_rx_packets_cpu(i, networking_packets);
        }
        const uint64_t networking_batches = networking_rx_batches_for_cpu(i);
        if (networking_batches != last_networking_batches_by_cpu[i]) {
            last_networking_batches_by_cpu[i] = networking_batches;
            log_smp_networking_rx_batches_cpu(i, networking_batches);
        }
        const uint64_t filesystem_ap_jobs = filesystem_ap_deferred_job_for_cpu(i);
        if (filesystem_ap_jobs != last_filesystem_ap_job_by_cpu[i]) {
            last_filesystem_ap_job_by_cpu[i] = filesystem_ap_jobs;
            if (log_detailed_counters) {
                log_smp_filesystem_ap_job_cpu(i, filesystem_ap_jobs);
            }
        }
        const uint64_t usb_ap_jobs = usb_ap_deferred_job_for_cpu(i);
        if (usb_ap_jobs != last_usb_ap_job_by_cpu[i]) {
            last_usb_ap_job_by_cpu[i] = usb_ap_jobs;
            if (log_detailed_counters) {
                log_smp_usb_ap_job_cpu(i, usb_ap_jobs);
            }
        }
        const uint64_t drops = scheduler_ap_work_drop_for_cpu(i);
        if (drops != last_ap_work_drop_by_cpu[i]) {
            last_ap_work_drop_by_cpu[i] = drops;
            if (log_detailed_counters) {
                log_smp_ap_work_drop_cpu(i, drops);
            }
        }
        const uint64_t queue_depth = scheduler_ap_work_queue_depth_for_cpu(i);
        ap_work_queue_depth_total += queue_depth;
        if (queue_depth != 0) {
            ap_work_queue_nonzero_cpus++;
        }
        if (drops == 0 && dispatches == executes && executes != 0) {
            ap_work_ok++;
        }
    }
    ap_work_pass = (ap_expected == 0) || (ap_work_ok >= ap_expected);
    const bool aggregate_dispatch_execute_aligned =
        (ap_work_execute_total >= ap_work_dispatch_total);
    if (!aggregate_dispatch_execute_aligned) {
        ap_work_pass = false;
    }
    if (ap_work_queue_depth_total != last_ap_work_queue_depth_total) {
        last_ap_work_queue_depth_total = ap_work_queue_depth_total;
        if (log_detailed_counters) {
            log_smp_ap_work_queue_depth_total(ap_work_queue_depth_total, ap_work_queue_nonzero_cpus);
        }
    }
    if (ap_expected == 0) {
        ap_work_pass = true;
    } else {
        const bool producer_activity_observed =
            (port_ap_job_total != 0) ||
            (networking_irq_total != 0) ||
            (storage_irq_total != 0) ||
            (input_irq_total != 0) ||
            (usb_irq_total != 0) ||
            (audio_irq_total_count != 0);
        const bool maintenance_ok =
            !producer_activity_observed || (ap_maintenance_execute_total >= ap_expected);
        const bool networking_ok =
            (networking_irq_total == 0) || (ap_networking_execute_total >= ap_expected);
        const bool networking_jobs_ok =
            (networking_irq_total == 0) || (networking_ap_job_total >= ap_expected);
        const bool filesystem_jobs_ok =
            (storage_irq_total == 0) || (filesystem_ap_job_total >= ap_expected);
        const bool usb_jobs_ok =
            (usb_irq_total == 0) || (usb_ap_job_total >= ap_expected);
        const bool audio_jobs_ok =
            (audio_irq_total_count == 0) || (audio_ap_job_total >= ap_expected);
        const bool subsystem_jobs_ok =
            networking_jobs_ok &&
            filesystem_jobs_ok &&
            usb_jobs_ok &&
            audio_jobs_ok;
        if (!maintenance_ok || !networking_ok || !subsystem_jobs_ok) {
            ap_work_pass = false;
        }
    }
    if (ap_work_pass) {
        ap_work_pending_streak = 0;
    } else {
        ap_work_pending_streak++;
    }
    const bool ap_work_pending_debounced =
        !ap_work_pass && ap_work_pending_streak >= kApWorkPendingDebounceIntervals;
    const bool ap_work_pass_for_logs = !ap_work_pending_debounced;
    const uint64_t ap_work_ok_for_logs = ap_work_pass_for_logs ? ap_expected : ap_work_ok;
    uint64_t expected_ap_cpu_mask = 0;
    const uint64_t migration_expected_ap = (ap_expected < 63) ? ap_expected : 63;
    for (uint64_t cpu = 1; cpu <= migration_expected_ap; cpu++) {
        expected_ap_cpu_mask |= (1ULL << cpu);
    }
    const bool migration_gate_active =
        (ap_pinned_active_total != 0) || scheduler_ap_pinned_task_migration_enabled();
    bool migration_pass = !migration_gate_active;
    if (ap_pinned_active_total != 0 && expected_ap_cpu_mask != 0) {
        const bool mask_covered =
            (ap_pinned_seen_cpu_mask_total & expected_ap_cpu_mask) == expected_ap_cpu_mask;
        const bool migrations_observed =
            (ap_expected <= 1) || (ap_pinned_migrations_total != 0);
        migration_pass = mask_covered && migrations_observed;
    }
    const bool migration_pass_for_logs = migration_pass || last_migration_pass;
    const uint64_t migration_expected_mask_for_logs =
        migration_gate_active ? expected_ap_cpu_mask : 0;
    const bool self_test_pass_for_logs =
        ap_work_pass_for_logs && (!migration_gate_active || migration_pass_for_logs);
    const bool can_emit_initial_self_test = self_test_logged || (ap_work_execute_total != 0);
    if (can_emit_initial_self_test &&
        (!self_test_logged ||
        ap_work_pass_for_logs != last_ap_work_pass ||
        migration_pass_for_logs != last_migration_pass ||
        (ticks - last_self_test_log_tick) >= 500)) {
        log_smp_self_test_summary(ap_work_pass_for_logs, ap_work_ok_for_logs, ap_expected,
                                  ap_work_dispatch_total, ap_work_execute_total);
        log_smp_migration_self_test_summary(migration_pass_for_logs,
                                            ap_pinned_active_total,
                                            ap_pinned_migrations_total,
                                            ap_pinned_seen_cpu_mask_total,
                                            migration_expected_mask_for_logs);
        log_smp_ap_work_kinds_summary(ap_maintenance_execute_total,
                                      ap_networking_execute_total,
                                      port_ap_job_total,
                                      networking_ap_job_total,
                                      filesystem_ap_job_total,
                                      usb_ap_job_total,
                                      ap_pinned_execute_total);
        last_self_test_log_tick = ticks;
        last_ap_work_pass = ap_work_pass_for_logs;
        last_migration_pass = migration_pass_for_logs;
        self_test_logged = true;
    }
    if ((ticks - last_smp_health_snapshot_tick) >= kSmpHealthSnapshotTicks) {
        last_smp_health_snapshot_tick = ticks;
        const uint64_t trigger_profile =
            __atomic_load_n(&g_ap_trigger_profile, __ATOMIC_RELAXED);
        log_smp_health_snapshot(online,
                                total,
                                ipi_observed_aps,
                                ipi_scheduler_total,
                                ap_work_dispatch_total,
                                ap_work_execute_total,
                                ap_work_queue_depth_total,
                                ap_work_drop_total,
                                ap_work_skip_rate_limit_total,
                                ap_work_skip_pending_cap_total,
                                ap_pinned_active_total,
                                ap_pinned_execute_total,
                                ap_pinned_migrations_total,
                                ap_pinned_seen_cpu_mask_total,
                                ap_to_ap_switch_total,
                                bsp_to_ap_switch_total,
                                runtime_msg_queue_high_water,
                                runtime_msg_input_queue_high_water,
                                runtime_msg_command_queue_high_water,
                                runtime_msg_dispatch_input_total,
                                runtime_msg_dispatch_command_total,
                                runtime_msg_route_drop_total,
                                runtime_msg_route_drop_invalid_source_total,
                                runtime_msg_route_drop_keyboard_suppressed_total,
                                runtime_msg_route_drop_unroutable_total,
                                runtime_msg_input_queue_depth,
                                runtime_msg_command_queue_depth,
                                runtime_msg_input_queue_drop,
                                runtime_msg_command_queue_drop,
                                runtime_msg_input_queue_enqueued,
                                runtime_msg_input_queue_dequeued,
                                runtime_msg_command_queue_enqueued,
                                runtime_msg_command_queue_dequeued,
                                trigger_profile,
                                self_test_pass_for_logs);
    }
    if ((ticks - last_heartbeat_log) >= kHeartbeatLogTicks) {
        last_heartbeat_log = ticks;
        log_info("timer: heartbeat.");
    }
}
