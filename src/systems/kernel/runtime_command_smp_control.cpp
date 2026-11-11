#include "fortify/systems/kernel/runtime_command_smp_control.h"

#include "fortify/systems/kernel/kernel_idle_observability.h"
#include "fortify/systems/kernel/kernel_string.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

extern "C" volatile uint64_t g_ap_trigger_profile;

bool kernel_handle_smp_control_runtime_command(const char *command) {
    if (command == 0) {
        return false;
    }
    if (kernel_string_equals(command, "ap-task-start")) {
        const bool started = scheduler_start_ap_pinned_demo_tasks();
        serial_write(started
                         ? "[INFO] smp: serial cmd ap-task-start enabled AP-pinned runnable tasks\n"
                         : "[WARN] smp: serial cmd ap-task-start found no AP targets\n");
        console_write(started
                          ? "[INFO] smp: serial cmd ap-task-start enabled AP-pinned runnable tasks\n"
                          : "[WARN] smp: serial cmd ap-task-start found no AP targets\n");
        return true;
    }
    if (kernel_string_equals(command, "ap-task-stop")) {
        scheduler_stop_ap_pinned_demo_tasks();
        serial_write("[INFO] smp: serial cmd ap-task-stop disabled AP-pinned runnable tasks\n");
        console_write("[INFO] smp: serial cmd ap-task-stop disabled AP-pinned runnable tasks\n");
        return true;
    }
    if (kernel_string_equals(command, "ap-task-migrate")) {
        const bool enabled = scheduler_ap_pinned_task_migration_enabled();
        serial_write("[INFO] smp: serial cmd ap-task-migrate current=");
        serial_write(enabled ? "1\n" : "0\n");
        console_write("[INFO] smp: serial cmd ap-task-migrate current=");
        console_write(enabled ? "1\n" : "0\n");
        return true;
    }
    if (kernel_string_starts_with(command, "ap-task-migrate ")) {
        const char *value_text = command + 16;
        uint64_t enabled = 0;
        if (!kernel_parse_u64_decimal(value_text, &enabled) || enabled > 1) {
            serial_write("[WARN] smp: serial cmd ap-task-migrate expects 0 or 1\n");
            console_write("[WARN] smp: serial cmd ap-task-migrate expects 0 or 1\n");
            return true;
        }
        scheduler_set_ap_pinned_task_migration_enabled(enabled != 0);
        serial_write("[INFO] smp: serial cmd ap-task-migrate set=");
        serial_write(enabled != 0 ? "1\n" : "0\n");
        console_write("[INFO] smp: serial cmd ap-task-migrate set=");
        console_write(enabled != 0 ? "1\n" : "0\n");
        return true;
    }
    if (kernel_string_equals(command, "smp-log")) {
        const uint64_t profile = kernel_smp_log_profile();
        char quiet_health_ticks_hex[19];
        kernel_format_hex64(quiet_health_ticks_hex, kernel_smp_quiet_health_snapshot_ticks());
        serial_write("[INFO] smp: serial cmd smp-log current=");
        serial_write(profile == 1 ? "quiet\n" : "default\n");
        serial_write("[INFO] smp: serial cmd smp-log quiet-health-ticks=");
        serial_write(quiet_health_ticks_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd smp-log current=");
        console_write(profile == 1 ? "quiet\n" : "default\n");
        console_write("[INFO] smp: serial cmd smp-log quiet-health-ticks=");
        console_write(quiet_health_ticks_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "smp-log ")) {
        const char *profile_text = command + 8;
        uint64_t profile = 0;
        if (kernel_string_equals(profile_text, "default")) {
            profile = 0;
        } else if (kernel_string_equals(profile_text, "quiet")) {
            profile = 1;
        } else {
            serial_write("[WARN] smp: serial cmd smp-log expects default|quiet\n");
            console_write("[WARN] smp: serial cmd smp-log expects default|quiet\n");
            return true;
        }
        kernel_set_smp_log_profile(profile);
        serial_write("[INFO] smp: serial cmd smp-log set=");
        serial_write(profile == 1 ? "quiet\n" : "default\n");
        console_write("[INFO] smp: serial cmd smp-log set=");
        console_write(profile == 1 ? "quiet\n" : "default\n");
        return true;
    }
    if (kernel_string_equals(command, "smp-log-health")) {
        char ticks_hex[19];
        kernel_format_hex64(ticks_hex, kernel_smp_quiet_health_snapshot_ticks());
        serial_write("[INFO] smp: serial cmd smp-log-health current=");
        serial_write(ticks_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd smp-log-health current=");
        console_write(ticks_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "smp-log-health ")) {
        uint64_t ticks = 0;
        if (!kernel_parse_u64_decimal(command + 15, &ticks) || ticks > 60000) {
            serial_write("[WARN] smp: serial cmd smp-log-health expects ticks 0..60000\n");
            console_write("[WARN] smp: serial cmd smp-log-health expects ticks 0..60000\n");
            return true;
        }
        kernel_set_smp_quiet_health_snapshot_ticks(ticks);
        char ticks_hex[19];
        kernel_format_hex64(ticks_hex, ticks);
        serial_write("[INFO] smp: serial cmd smp-log-health set=");
        serial_write(ticks_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd smp-log-health set=");
        console_write(ticks_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "net-rx-inject ")) {
        const char *count_text = command + 14;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd net-rx-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd net-rx-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = networking_inject_rx_events(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd net-rx-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd net-rx-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "net-rx-stats")) {
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += networking_queue_depth_for_cpu(cpu);
        }
        const uint64_t queue_high_water_total = networking_queue_high_water_total();
        const uint64_t irq_total = networking_rx_irq_total();
        const uint64_t drop_total = networking_rx_irq_drop_total();
        const uint64_t processed_total = networking_rx_packets_processed_total();
        const uint64_t processed_irq_total = networking_rx_packets_processed_irq_total();
        const uint64_t processed_injected_total = networking_rx_packets_processed_injected_total();
        const uint64_t batches_total = networking_rx_batches_total();
        const uint64_t jobs_total = networking_ap_deferred_job_total();

        char queue_depth_hex[19];
        char queue_high_water_hex[19];
        char irq_hex[19];
        char drop_hex[19];
        char processed_hex[19];
        char processed_irq_hex[19];
        char processed_injected_hex[19];
        char batches_hex[19];
        char jobs_hex[19];
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(queue_high_water_hex, queue_high_water_total);
        kernel_format_hex64(irq_hex, irq_total);
        kernel_format_hex64(drop_hex, drop_total);
        kernel_format_hex64(processed_hex, processed_total);
        kernel_format_hex64(processed_irq_hex, processed_irq_total);
        kernel_format_hex64(processed_injected_hex, processed_injected_total);
        kernel_format_hex64(batches_hex, batches_total);
        kernel_format_hex64(jobs_hex, jobs_total);

        serial_write("[INFO] smp: serial cmd net-rx-stats q(depth/hwm)=");
        serial_write(queue_depth_hex);
        serial_write("/");
        serial_write(queue_high_water_hex);
        serial_write(" irq/drop=");
        serial_write(irq_hex);
        serial_write("/");
        serial_write(drop_hex);
        serial_write(" processed(total/irq/inject)=");
        serial_write(processed_hex);
        serial_write("/");
        serial_write(processed_irq_hex);
        serial_write("/");
        serial_write(processed_injected_hex);
        serial_write(" batch/jobs=");
        serial_write(batches_hex);
        serial_write("/");
        serial_write(jobs_hex);
        serial_write("\n");

        console_write("[INFO] smp: serial cmd net-rx-stats q(depth/hwm)=");
        console_write(queue_depth_hex);
        console_write("/");
        console_write(queue_high_water_hex);
        console_write(" irq/drop=");
        console_write(irq_hex);
        console_write("/");
        console_write(drop_hex);
        console_write(" processed(total/irq/inject)=");
        console_write(processed_hex);
        console_write("/");
        console_write(processed_irq_hex);
        console_write("/");
        console_write(processed_injected_hex);
        console_write(" batch/jobs=");
        console_write(batches_hex);
        console_write("/");
        console_write(jobs_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "net-rx-smoke") || kernel_string_starts_with(command, "net-rx-smoke ")) {
        uint64_t count = 32;
        if (kernel_string_starts_with(command, "net-rx-smoke ")) {
            if (!kernel_parse_u64_decimal(command + 13, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd net-rx-smoke expects [count] in 1..4096\n");
                console_write("[WARN] smp: serial cmd net-rx-smoke expects [count] in 1..4096\n");
                return true;
            }
        }
        const uint64_t before_irq = networking_rx_irq_total();
        const uint64_t before_drop = networking_rx_irq_drop_total();
        const uint64_t before_processed = networking_rx_packets_processed_total();
        const uint64_t injected = networking_inject_rx_events(count);
        const uint64_t after_irq = networking_rx_irq_total();
        const uint64_t after_drop = networking_rx_irq_drop_total();
        const uint64_t after_processed = networking_rx_packets_processed_total();
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += networking_queue_depth_for_cpu(cpu);
        }
        const uint64_t delta_irq = after_irq - before_irq;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_irq == count;

        char count_hex[19];
        char injected_hex[19];
        char delta_irq_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_irq_hex, delta_irq);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);

        serial_write(pass ? "[INFO] smp: serial cmd net-rx-smoke pass requested/injected="
                          : "[WARN] smp: serial cmd net-rx-smoke fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(irq/drop/processed)=");
        serial_write(delta_irq_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd net-rx-smoke pass requested/injected="
                           : "[WARN] smp: serial cmd net-rx-smoke fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(irq/drop/processed)=");
        console_write(delta_irq_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "net-rx-smoke-wait") || kernel_string_starts_with(command, "net-rx-smoke-wait ")) {
        uint64_t count = 32;
        uint64_t timeout_ticks = 600;
        if (kernel_string_starts_with(command, "net-rx-smoke-wait ")) {
            char count_token[16];
            char timeout_token[16];
            uint64_t cursor = 18;
            if (!kernel_parse_token(command, cursor, count_token, sizeof(count_token), &cursor) ||
                !kernel_parse_u64_decimal(count_token, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd net-rx-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                console_write("[WARN] smp: serial cmd net-rx-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                return true;
            }
            if (kernel_parse_token(command, cursor, timeout_token, sizeof(timeout_token), &cursor)) {
                if (!kernel_parse_u64_decimal(timeout_token, &timeout_ticks) || timeout_ticks == 0 || timeout_ticks > 10000) {
                    serial_write("[WARN] smp: serial cmd net-rx-smoke-wait timeout expects 1..10000 ticks\n");
                    console_write("[WARN] smp: serial cmd net-rx-smoke-wait timeout expects 1..10000 ticks\n");
                    return true;
                }
                char extra_token[4];
                if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                    serial_write("[WARN] smp: serial cmd net-rx-smoke-wait accepts only [count] [timeout-ticks]\n");
                    console_write("[WARN] smp: serial cmd net-rx-smoke-wait accepts only [count] [timeout-ticks]\n");
                    return true;
                }
            }
        }
        const uint64_t before_irq = networking_rx_irq_total();
        const uint64_t before_drop = networking_rx_irq_drop_total();
        const uint64_t before_processed = networking_rx_packets_processed_total();
        const uint64_t injected = networking_inject_rx_events(count);

        const uint64_t start_tick = timer_ticks();
        uint64_t queue_depth_total = 0;
        bool drained = false;
        while ((timer_ticks() - start_tick) <= timeout_ticks) {
            const uint64_t total = smp_total_cpu_count();
            queue_depth_total = 0;
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                queue_depth_total += networking_queue_depth_for_cpu(cpu);
            }
            if (queue_depth_total == 0) {
                drained = true;
                break;
            }
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                networking_process_ap_deferred_job(cpu, 0);
            }
            (void)scheduler_service_deferred_switch();
        }

        const uint64_t after_irq = networking_rx_irq_total();
        const uint64_t after_drop = networking_rx_irq_drop_total();
        const uint64_t after_processed = networking_rx_packets_processed_total();
        const uint64_t delta_irq = after_irq - before_irq;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_irq == count && drained;

        char count_hex[19];
        char injected_hex[19];
        char delta_irq_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        char timeout_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_irq_hex, delta_irq);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(timeout_hex, timeout_ticks);

        serial_write(pass ? "[INFO] smp: serial cmd net-rx-smoke-wait pass requested/injected="
                          : "[WARN] smp: serial cmd net-rx-smoke-wait fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(irq/drop/processed)=");
        serial_write(delta_irq_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" drained=");
        serial_write(drained ? "1" : "0");
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write(" timeout=");
        serial_write(timeout_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd net-rx-smoke-wait pass requested/injected="
                           : "[WARN] smp: serial cmd net-rx-smoke-wait fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(irq/drop/processed)=");
        console_write(delta_irq_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" drained=");
        console_write(drained ? "1" : "0");
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write(" timeout=");
        console_write(timeout_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "web-head ")) {
        char target[64];
        char max_bytes_token[16];
        uint64_t cursor = 9;
        uint64_t max_bytes = 256;
        if (!kernel_parse_token(command, cursor, target, sizeof(target), &cursor)) {
            serial_write("[WARN] webaccess: serial cmd web-head expects <target> [max-bytes]\n");
            console_write("[WARN] webaccess: serial cmd web-head expects <target> [max-bytes]\n");
            return true;
        }
        if (kernel_parse_token(command, cursor, max_bytes_token, sizeof(max_bytes_token), &cursor)) {
            if (!kernel_parse_u64_decimal(max_bytes_token, &max_bytes) || max_bytes == 0 || max_bytes > 4096) {
                serial_write("[WARN] webaccess: serial cmd web-head max-bytes expects 1..4096\n");
                console_write("[WARN] webaccess: serial cmd web-head max-bytes expects 1..4096\n");
                return true;
            }
            char extra_token[4];
            if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                serial_write("[WARN] webaccess: serial cmd web-head accepts only <target> [max-bytes]\n");
                console_write("[WARN] webaccess: serial cmd web-head accepts only <target> [max-bytes]\n");
                return true;
            }
        }

        uint64_t request_id = 0;
        if (!webaccess_head_request_submit(target, max_bytes, &request_id)) {
            serial_write("[WARN] webaccess: serial cmd web-head queue full/unavailable\n");
            console_write("[WARN] webaccess: serial cmd web-head queue full/unavailable\n");
            return true;
        }
        char request_hex[19];
        kernel_format_hex64(request_hex, request_id);
        serial_write("[INFO] webaccess: serial cmd web-head queued id=");
        serial_write(request_hex);
        serial_write(" target=");
        serial_write(target);
        serial_write("\n");
        console_write("[INFO] webaccess: serial cmd web-head queued id=");
        console_write(request_hex);
        console_write(" target=");
        console_write(target);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "web-head-status ")) {
        constexpr uint64_t kWebHeadRequestStatePending = 1;
        constexpr uint64_t kWebHeadRequestStateRunning = 2;
        constexpr uint64_t kWebHeadRequestStateDoneOk = 3;
        constexpr uint64_t kWebHeadRequestStateDoneErr = 4;
        uint64_t request_id = 0;
        if (!kernel_parse_u64_decimal(command + 16, &request_id) || request_id == 0) {
            serial_write("[WARN] webaccess: serial cmd web-head-status expects request-id\n");
            console_write("[WARN] webaccess: serial cmd web-head-status expects request-id\n");
            return true;
        }
        uint64_t state = 0;
        uint64_t bytes = 0;
        uint64_t checksum = 0;
        if (!webaccess_head_request_status(request_id, &state, &bytes, &checksum)) {
            serial_write("[WARN] webaccess: serial cmd web-head-status request not found\n");
            console_write("[WARN] webaccess: serial cmd web-head-status request not found\n");
            return true;
        }
        char id_hex[19];
        char bytes_hex[19];
        char checksum_hex[19];
        kernel_format_hex64(id_hex, request_id);
        kernel_format_hex64(bytes_hex, bytes);
        kernel_format_hex64(checksum_hex, checksum);
        serial_write("[INFO] webaccess: serial cmd web-head-status id=");
        serial_write(id_hex);
        serial_write(" state=");
        console_write("[INFO] webaccess: serial cmd web-head-status id=");
        console_write(id_hex);
        console_write(" state=");

        const char *state_text = "unknown";
        if (state == kWebHeadRequestStatePending) state_text = "pending";
        else if (state == kWebHeadRequestStateRunning) state_text = "running";
        else if (state == kWebHeadRequestStateDoneOk) state_text = "done-ok";
        else if (state == kWebHeadRequestStateDoneErr) state_text = "done-err";
        serial_write(state_text);
        serial_write(" bytes=");
        serial_write(bytes_hex);
        serial_write(" checksum=");
        serial_write(checksum_hex);
        serial_write("\n");

        console_write(state_text);
        console_write(" bytes=");
        console_write(bytes_hex);
        console_write(" checksum=");
        console_write(checksum_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "usb-inject ")) {
        const char *count_text = command + 11;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd usb-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd usb-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = usb_inject_events(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd usb-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd usb-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "usb-stats")) {
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += usb_queue_depth_for_cpu(cpu);
        }
        const uint64_t queue_high_water_total = usb_queue_high_water_total();
        const uint64_t irq_total = usb_host_irq_total();
        const uint64_t irq_drop_total = usb_host_irq_drop_total();
        const uint64_t processed_total = usb_processed_events_total();
        const uint64_t processed_irq_total = usb_processed_irq_events_total();
        const uint64_t processed_injected_total = usb_processed_injected_events_total();
        const uint64_t batches_total = usb_batches_total();
        const uint64_t jobs_total = usb_ap_deferred_job_total();

        char queue_depth_hex[19];
        char queue_high_water_hex[19];
        char irq_hex[19];
        char drop_hex[19];
        char processed_hex[19];
        char processed_irq_hex[19];
        char processed_injected_hex[19];
        char batches_hex[19];
        char jobs_hex[19];
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(queue_high_water_hex, queue_high_water_total);
        kernel_format_hex64(irq_hex, irq_total);
        kernel_format_hex64(drop_hex, irq_drop_total);
        kernel_format_hex64(processed_hex, processed_total);
        kernel_format_hex64(processed_irq_hex, processed_irq_total);
        kernel_format_hex64(processed_injected_hex, processed_injected_total);
        kernel_format_hex64(batches_hex, batches_total);
        kernel_format_hex64(jobs_hex, jobs_total);

        serial_write("[INFO] smp: serial cmd usb-stats q(depth/hwm)=");
        serial_write(queue_depth_hex);
        serial_write("/");
        serial_write(queue_high_water_hex);
        serial_write(" irq/drop=");
        serial_write(irq_hex);
        serial_write("/");
        serial_write(drop_hex);
        serial_write(" processed(total/irq/inject)=");
        serial_write(processed_hex);
        serial_write("/");
        serial_write(processed_irq_hex);
        serial_write("/");
        serial_write(processed_injected_hex);
        serial_write(" batch/jobs=");
        serial_write(batches_hex);
        serial_write("/");
        serial_write(jobs_hex);
        serial_write("\n");

        console_write("[INFO] smp: serial cmd usb-stats q(depth/hwm)=");
        console_write(queue_depth_hex);
        console_write("/");
        console_write(queue_high_water_hex);
        console_write(" irq/drop=");
        console_write(irq_hex);
        console_write("/");
        console_write(drop_hex);
        console_write(" processed(total/irq/inject)=");
        console_write(processed_hex);
        console_write("/");
        console_write(processed_irq_hex);
        console_write("/");
        console_write(processed_injected_hex);
        console_write(" batch/jobs=");
        console_write(batches_hex);
        console_write("/");
        console_write(jobs_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "usb-smoke") || kernel_string_starts_with(command, "usb-smoke ")) {
        uint64_t count = 32;
        if (kernel_string_starts_with(command, "usb-smoke ")) {
            if (!kernel_parse_u64_decimal(command + 10, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd usb-smoke expects [count] in 1..4096\n");
                console_write("[WARN] smp: serial cmd usb-smoke expects [count] in 1..4096\n");
                return true;
            }
        }
        const uint64_t before_irq = usb_host_irq_total();
        const uint64_t before_drop = usb_host_irq_drop_total();
        const uint64_t before_processed = usb_processed_events_total();
        const uint64_t injected = usb_inject_events(count);
        const uint64_t after_irq = usb_host_irq_total();
        const uint64_t after_drop = usb_host_irq_drop_total();
        const uint64_t after_processed = usb_processed_events_total();
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += usb_queue_depth_for_cpu(cpu);
        }
        const uint64_t delta_irq = after_irq - before_irq;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_irq == count;

        char count_hex[19];
        char injected_hex[19];
        char delta_irq_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_irq_hex, delta_irq);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);

        serial_write(pass ? "[INFO] smp: serial cmd usb-smoke pass requested/injected="
                          : "[WARN] smp: serial cmd usb-smoke fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(irq/drop/processed)=");
        serial_write(delta_irq_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd usb-smoke pass requested/injected="
                           : "[WARN] smp: serial cmd usb-smoke fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(irq/drop/processed)=");
        console_write(delta_irq_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "usb-smoke-wait") || kernel_string_starts_with(command, "usb-smoke-wait ")) {
        uint64_t count = 32;
        uint64_t timeout_ticks = 600;
        if (kernel_string_starts_with(command, "usb-smoke-wait ")) {
            char count_token[16];
            char timeout_token[16];
            uint64_t cursor = 15;
            if (!kernel_parse_token(command, cursor, count_token, sizeof(count_token), &cursor) ||
                !kernel_parse_u64_decimal(count_token, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd usb-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                console_write("[WARN] smp: serial cmd usb-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                return true;
            }
            if (kernel_parse_token(command, cursor, timeout_token, sizeof(timeout_token), &cursor)) {
                if (!kernel_parse_u64_decimal(timeout_token, &timeout_ticks) || timeout_ticks == 0 || timeout_ticks > 10000) {
                    serial_write("[WARN] smp: serial cmd usb-smoke-wait timeout expects 1..10000 ticks\n");
                    console_write("[WARN] smp: serial cmd usb-smoke-wait timeout expects 1..10000 ticks\n");
                    return true;
                }
                char extra_token[4];
                if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                    serial_write("[WARN] smp: serial cmd usb-smoke-wait accepts only [count] [timeout-ticks]\n");
                    console_write("[WARN] smp: serial cmd usb-smoke-wait accepts only [count] [timeout-ticks]\n");
                    return true;
                }
            }
        }
        const uint64_t before_irq = usb_host_irq_total();
        const uint64_t before_drop = usb_host_irq_drop_total();
        const uint64_t before_processed = usb_processed_events_total();
        const uint64_t injected = usb_inject_events(count);

        const uint64_t start_tick = timer_ticks();
        uint64_t queue_depth_total = 0;
        bool drained = false;
        while ((timer_ticks() - start_tick) <= timeout_ticks) {
            const uint64_t total = smp_total_cpu_count();
            queue_depth_total = 0;
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                queue_depth_total += usb_queue_depth_for_cpu(cpu);
            }
            if (queue_depth_total == 0) {
                drained = true;
                break;
            }
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                usb_process_ap_deferred_job(cpu, 0);
            }
            (void)scheduler_service_deferred_switch();
        }

        const uint64_t after_irq = usb_host_irq_total();
        const uint64_t after_drop = usb_host_irq_drop_total();
        const uint64_t after_processed = usb_processed_events_total();
        const uint64_t delta_irq = after_irq - before_irq;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_irq == count && drained;

        char count_hex[19];
        char injected_hex[19];
        char delta_irq_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        char timeout_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_irq_hex, delta_irq);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(timeout_hex, timeout_ticks);

        serial_write(pass ? "[INFO] smp: serial cmd usb-smoke-wait pass requested/injected="
                          : "[WARN] smp: serial cmd usb-smoke-wait fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(irq/drop/processed)=");
        serial_write(delta_irq_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" drained=");
        serial_write(drained ? "1" : "0");
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write(" timeout=");
        serial_write(timeout_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd usb-smoke-wait pass requested/injected="
                           : "[WARN] smp: serial cmd usb-smoke-wait fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(irq/drop/processed)=");
        console_write(delta_irq_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" drained=");
        console_write(drained ? "1" : "0");
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write(" timeout=");
        console_write(timeout_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "storage-stats")) {
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += filesystem_queue_depth_for_cpu(cpu);
        }
        const uint64_t queue_high_water_total = filesystem_queue_high_water_total();
        const uint64_t irq_total = filesystem_storage_irq_total();
        const uint64_t drop_total = filesystem_storage_irq_drop_total();
        const uint64_t processed_total = filesystem_processed_events_total();
        const uint64_t processed_irq_total = filesystem_processed_irq_events_total();
        const uint64_t processed_injected_total = filesystem_processed_injected_events_total();
        const uint64_t batches_total = filesystem_batches_total();
        const uint64_t jobs_total = filesystem_ap_deferred_job_total();

        char queue_depth_hex[19];
        char queue_high_water_hex[19];
        char irq_hex[19];
        char drop_hex[19];
        char processed_hex[19];
        char processed_irq_hex[19];
        char processed_injected_hex[19];
        char batches_hex[19];
        char jobs_hex[19];
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(queue_high_water_hex, queue_high_water_total);
        kernel_format_hex64(irq_hex, irq_total);
        kernel_format_hex64(drop_hex, drop_total);
        kernel_format_hex64(processed_hex, processed_total);
        kernel_format_hex64(processed_irq_hex, processed_irq_total);
        kernel_format_hex64(processed_injected_hex, processed_injected_total);
        kernel_format_hex64(batches_hex, batches_total);
        kernel_format_hex64(jobs_hex, jobs_total);

        serial_write("[INFO] smp: serial cmd storage-stats q(depth/hwm)=");
        serial_write(queue_depth_hex);
        serial_write("/");
        serial_write(queue_high_water_hex);
        serial_write(" irq/drop=");
        serial_write(irq_hex);
        serial_write("/");
        serial_write(drop_hex);
        serial_write(" processed(total/irq/inject)=");
        serial_write(processed_hex);
        serial_write("/");
        serial_write(processed_irq_hex);
        serial_write("/");
        serial_write(processed_injected_hex);
        serial_write(" batch/jobs=");
        serial_write(batches_hex);
        serial_write("/");
        serial_write(jobs_hex);
        serial_write("\n");

        console_write("[INFO] smp: serial cmd storage-stats q(depth/hwm)=");
        console_write(queue_depth_hex);
        console_write("/");
        console_write(queue_high_water_hex);
        console_write(" irq/drop=");
        console_write(irq_hex);
        console_write("/");
        console_write(drop_hex);
        console_write(" processed(total/irq/inject)=");
        console_write(processed_hex);
        console_write("/");
        console_write(processed_irq_hex);
        console_write("/");
        console_write(processed_injected_hex);
        console_write(" batch/jobs=");
        console_write(batches_hex);
        console_write("/");
        console_write(jobs_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "storage-smoke") || kernel_string_starts_with(command, "storage-smoke ")) {
        uint64_t count = 32;
        if (kernel_string_starts_with(command, "storage-smoke ")) {
            if (!kernel_parse_u64_decimal(command + 14, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd storage-smoke expects [count] in 1..4096\n");
                console_write("[WARN] smp: serial cmd storage-smoke expects [count] in 1..4096\n");
                return true;
            }
        }
        const uint64_t before_irq = filesystem_storage_irq_total();
        const uint64_t before_drop = filesystem_storage_irq_drop_total();
        const uint64_t before_processed = filesystem_processed_events_total();
        const uint64_t injected = filesystem_inject_storage_events(count);
        const uint64_t after_irq = filesystem_storage_irq_total();
        const uint64_t after_drop = filesystem_storage_irq_drop_total();
        const uint64_t after_processed = filesystem_processed_events_total();
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += filesystem_queue_depth_for_cpu(cpu);
        }
        const uint64_t delta_irq = after_irq - before_irq;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_irq == count;

        char count_hex[19];
        char injected_hex[19];
        char delta_irq_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_irq_hex, delta_irq);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);

        serial_write(pass ? "[INFO] smp: serial cmd storage-smoke pass requested/injected="
                          : "[WARN] smp: serial cmd storage-smoke fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(irq/drop/processed)=");
        serial_write(delta_irq_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd storage-smoke pass requested/injected="
                           : "[WARN] smp: serial cmd storage-smoke fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(irq/drop/processed)=");
        console_write(delta_irq_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "storage-smoke-wait") || kernel_string_starts_with(command, "storage-smoke-wait ")) {
        uint64_t count = 32;
        uint64_t timeout_ticks = 600;
        if (kernel_string_starts_with(command, "storage-smoke-wait ")) {
            char count_token[16];
            char timeout_token[16];
            uint64_t cursor = 19;
            if (!kernel_parse_token(command, cursor, count_token, sizeof(count_token), &cursor) ||
                !kernel_parse_u64_decimal(count_token, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd storage-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                console_write("[WARN] smp: serial cmd storage-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                return true;
            }
            if (kernel_parse_token(command, cursor, timeout_token, sizeof(timeout_token), &cursor)) {
                if (!kernel_parse_u64_decimal(timeout_token, &timeout_ticks) || timeout_ticks == 0 || timeout_ticks > 10000) {
                    serial_write("[WARN] smp: serial cmd storage-smoke-wait timeout expects 1..10000 ticks\n");
                    console_write("[WARN] smp: serial cmd storage-smoke-wait timeout expects 1..10000 ticks\n");
                    return true;
                }
                char extra_token[4];
                if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                    serial_write("[WARN] smp: serial cmd storage-smoke-wait accepts only [count] [timeout-ticks]\n");
                    console_write("[WARN] smp: serial cmd storage-smoke-wait accepts only [count] [timeout-ticks]\n");
                    return true;
                }
            }
        }
        const uint64_t before_irq = filesystem_storage_irq_total();
        const uint64_t before_drop = filesystem_storage_irq_drop_total();
        const uint64_t before_processed = filesystem_processed_events_total();
        const uint64_t injected = filesystem_inject_storage_events(count);

        const uint64_t start_tick = timer_ticks();
        uint64_t queue_depth_total = 0;
        bool drained = false;
        while ((timer_ticks() - start_tick) <= timeout_ticks) {
            const uint64_t total = smp_total_cpu_count();
            queue_depth_total = 0;
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                queue_depth_total += filesystem_queue_depth_for_cpu(cpu);
            }
            if (queue_depth_total == 0) {
                drained = true;
                break;
            }
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                filesystem_process_ap_deferred_job(cpu, 0);
            }
            (void)scheduler_service_deferred_switch();
        }

        const uint64_t after_irq = filesystem_storage_irq_total();
        const uint64_t after_drop = filesystem_storage_irq_drop_total();
        const uint64_t after_processed = filesystem_processed_events_total();
        const uint64_t delta_irq = after_irq - before_irq;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_irq == count && drained;

        char count_hex[19];
        char injected_hex[19];
        char delta_irq_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        char timeout_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_irq_hex, delta_irq);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(timeout_hex, timeout_ticks);

        serial_write(pass ? "[INFO] smp: serial cmd storage-smoke-wait pass requested/injected="
                          : "[WARN] smp: serial cmd storage-smoke-wait fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(irq/drop/processed)=");
        serial_write(delta_irq_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" drained=");
        serial_write(drained ? "1" : "0");
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write(" timeout=");
        serial_write(timeout_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd storage-smoke-wait pass requested/injected="
                           : "[WARN] smp: serial cmd storage-smoke-wait fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(irq/drop/processed)=");
        console_write(delta_irq_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" drained=");
        console_write(drained ? "1" : "0");
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write(" timeout=");
        console_write(timeout_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "storage-inject ")) {
        const char *count_text = command + 15;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd storage-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd storage-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = filesystem_inject_storage_events(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd storage-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd storage-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "audio-stats")) {
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += audio_queue_depth_for_cpu(cpu);
        }
        const uint64_t queue_high_water_total = audio_queue_high_water_total();
        const uint64_t irq_total = audio_irq_total();
        const uint64_t drop_total = audio_irq_drop_total();
        const uint64_t processed_total = audio_processed_events_total();
        const uint64_t processed_irq_total = audio_processed_irq_events_total();
        const uint64_t processed_injected_total = audio_processed_injected_events_total();
        const uint64_t batches_total = audio_batches_total();
        const uint64_t jobs_total = audio_ap_deferred_job_total();

        char queue_depth_hex[19];
        char queue_high_water_hex[19];
        char irq_hex[19];
        char drop_hex[19];
        char processed_hex[19];
        char processed_irq_hex[19];
        char processed_injected_hex[19];
        char batches_hex[19];
        char jobs_hex[19];
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(queue_high_water_hex, queue_high_water_total);
        kernel_format_hex64(irq_hex, irq_total);
        kernel_format_hex64(drop_hex, drop_total);
        kernel_format_hex64(processed_hex, processed_total);
        kernel_format_hex64(processed_irq_hex, processed_irq_total);
        kernel_format_hex64(processed_injected_hex, processed_injected_total);
        kernel_format_hex64(batches_hex, batches_total);
        kernel_format_hex64(jobs_hex, jobs_total);

        serial_write("[INFO] smp: serial cmd audio-stats q(depth/hwm)=");
        serial_write(queue_depth_hex);
        serial_write("/");
        serial_write(queue_high_water_hex);
        serial_write(" irq/drop=");
        serial_write(irq_hex);
        serial_write("/");
        serial_write(drop_hex);
        serial_write(" processed(total/irq/inject)=");
        serial_write(processed_hex);
        serial_write("/");
        serial_write(processed_irq_hex);
        serial_write("/");
        serial_write(processed_injected_hex);
        serial_write(" batch/jobs=");
        serial_write(batches_hex);
        serial_write("/");
        serial_write(jobs_hex);
        serial_write("\n");

        console_write("[INFO] smp: serial cmd audio-stats q(depth/hwm)=");
        console_write(queue_depth_hex);
        console_write("/");
        console_write(queue_high_water_hex);
        console_write(" irq/drop=");
        console_write(irq_hex);
        console_write("/");
        console_write(drop_hex);
        console_write(" processed(total/irq/inject)=");
        console_write(processed_hex);
        console_write("/");
        console_write(processed_irq_hex);
        console_write("/");
        console_write(processed_injected_hex);
        console_write(" batch/jobs=");
        console_write(batches_hex);
        console_write("/");
        console_write(jobs_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "audio-smoke") || kernel_string_starts_with(command, "audio-smoke ")) {
        uint64_t count = 32;
        if (kernel_string_starts_with(command, "audio-smoke ")) {
            if (!kernel_parse_u64_decimal(command + 12, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd audio-smoke expects [count] in 1..4096\n");
                console_write("[WARN] smp: serial cmd audio-smoke expects [count] in 1..4096\n");
                return true;
            }
        }
        const uint64_t before_irq = audio_irq_total();
        const uint64_t before_drop = audio_irq_drop_total();
        const uint64_t before_processed = audio_processed_events_total();
        const uint64_t injected = audio_inject_events(count);
        const uint64_t after_irq = audio_irq_total();
        const uint64_t after_drop = audio_irq_drop_total();
        const uint64_t after_processed = audio_processed_events_total();
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += audio_queue_depth_for_cpu(cpu);
        }
        const uint64_t delta_irq = after_irq - before_irq;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_irq == count;

        char count_hex[19];
        char injected_hex[19];
        char delta_irq_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_irq_hex, delta_irq);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);

        serial_write(pass ? "[INFO] smp: serial cmd audio-smoke pass requested/injected="
                          : "[WARN] smp: serial cmd audio-smoke fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(irq/drop/processed)=");
        serial_write(delta_irq_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd audio-smoke pass requested/injected="
                           : "[WARN] smp: serial cmd audio-smoke fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(irq/drop/processed)=");
        console_write(delta_irq_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "audio-smoke-wait") || kernel_string_starts_with(command, "audio-smoke-wait ")) {
        uint64_t count = 32;
        uint64_t timeout_ticks = 600;
        if (kernel_string_starts_with(command, "audio-smoke-wait ")) {
            char count_token[16];
            char timeout_token[16];
            uint64_t cursor = 17;
            if (!kernel_parse_token(command, cursor, count_token, sizeof(count_token), &cursor) ||
                !kernel_parse_u64_decimal(count_token, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd audio-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                console_write("[WARN] smp: serial cmd audio-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                return true;
            }
            if (kernel_parse_token(command, cursor, timeout_token, sizeof(timeout_token), &cursor)) {
                if (!kernel_parse_u64_decimal(timeout_token, &timeout_ticks) || timeout_ticks == 0 || timeout_ticks > 10000) {
                    serial_write("[WARN] smp: serial cmd audio-smoke-wait timeout expects 1..10000 ticks\n");
                    console_write("[WARN] smp: serial cmd audio-smoke-wait timeout expects 1..10000 ticks\n");
                    return true;
                }
                char extra_token[4];
                if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                    serial_write("[WARN] smp: serial cmd audio-smoke-wait accepts only [count] [timeout-ticks]\n");
                    console_write("[WARN] smp: serial cmd audio-smoke-wait accepts only [count] [timeout-ticks]\n");
                    return true;
                }
            }
        }
        const uint64_t before_irq = audio_irq_total();
        const uint64_t before_drop = audio_irq_drop_total();
        const uint64_t before_processed = audio_processed_events_total();
        const uint64_t injected = audio_inject_events(count);

        const uint64_t start_tick = timer_ticks();
        uint64_t queue_depth_total = 0;
        bool drained = false;
        while ((timer_ticks() - start_tick) <= timeout_ticks) {
            const uint64_t total = smp_total_cpu_count();
            queue_depth_total = 0;
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                queue_depth_total += audio_queue_depth_for_cpu(cpu);
            }
            if (queue_depth_total == 0) {
                drained = true;
                break;
            }
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                audio_process_ap_deferred_job(cpu, 0);
            }
            (void)scheduler_service_deferred_switch();
        }

        const uint64_t after_irq = audio_irq_total();
        const uint64_t after_drop = audio_irq_drop_total();
        const uint64_t after_processed = audio_processed_events_total();
        const uint64_t delta_irq = after_irq - before_irq;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_irq == count && drained;

        char count_hex[19];
        char injected_hex[19];
        char delta_irq_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        char timeout_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_irq_hex, delta_irq);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(timeout_hex, timeout_ticks);

        serial_write(pass ? "[INFO] smp: serial cmd audio-smoke-wait pass requested/injected="
                          : "[WARN] smp: serial cmd audio-smoke-wait fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(irq/drop/processed)=");
        serial_write(delta_irq_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" drained=");
        serial_write(drained ? "1" : "0");
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write(" timeout=");
        serial_write(timeout_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd audio-smoke-wait pass requested/injected="
                           : "[WARN] smp: serial cmd audio-smoke-wait fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(irq/drop/processed)=");
        console_write(delta_irq_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" drained=");
        console_write(drained ? "1" : "0");
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write(" timeout=");
        console_write(timeout_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "audio-inject ")) {
        const char *count_text = command + 13;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd audio-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd audio-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = audio_inject_events(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd audio-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd audio-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "webcam-stats")) {
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += webcam_queue_depth_for_cpu(cpu);
        }
        const uint64_t queue_high_water_total = webcam_queue_high_water_total();
        const uint64_t frame_total = webcam_frame_total();
        const uint64_t drop_total = webcam_frame_drop_total();
        const uint64_t processed_total = webcam_processed_events_total();
        const uint64_t processed_irq_total = webcam_processed_irq_events_total();
        const uint64_t processed_injected_total = webcam_processed_injected_events_total();
        const uint64_t batches_total = webcam_batches_total();
        const uint64_t jobs_total = webcam_ap_deferred_job_total();

        char queue_depth_hex[19];
        char queue_high_water_hex[19];
        char frame_hex[19];
        char drop_hex[19];
        char processed_hex[19];
        char processed_irq_hex[19];
        char processed_injected_hex[19];
        char batches_hex[19];
        char jobs_hex[19];
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(queue_high_water_hex, queue_high_water_total);
        kernel_format_hex64(frame_hex, frame_total);
        kernel_format_hex64(drop_hex, drop_total);
        kernel_format_hex64(processed_hex, processed_total);
        kernel_format_hex64(processed_irq_hex, processed_irq_total);
        kernel_format_hex64(processed_injected_hex, processed_injected_total);
        kernel_format_hex64(batches_hex, batches_total);
        kernel_format_hex64(jobs_hex, jobs_total);

        serial_write("[INFO] smp: serial cmd webcam-stats q(depth/hwm)=");
        serial_write(queue_depth_hex);
        serial_write("/");
        serial_write(queue_high_water_hex);
        serial_write(" frame/drop=");
        serial_write(frame_hex);
        serial_write("/");
        serial_write(drop_hex);
        serial_write(" processed(total/irq/inject)=");
        serial_write(processed_hex);
        serial_write("/");
        serial_write(processed_irq_hex);
        serial_write("/");
        serial_write(processed_injected_hex);
        serial_write(" batch/jobs=");
        serial_write(batches_hex);
        serial_write("/");
        serial_write(jobs_hex);
        serial_write("\n");

        console_write("[INFO] smp: serial cmd webcam-stats q(depth/hwm)=");
        console_write(queue_depth_hex);
        console_write("/");
        console_write(queue_high_water_hex);
        console_write(" frame/drop=");
        console_write(frame_hex);
        console_write("/");
        console_write(drop_hex);
        console_write(" processed(total/irq/inject)=");
        console_write(processed_hex);
        console_write("/");
        console_write(processed_irq_hex);
        console_write("/");
        console_write(processed_injected_hex);
        console_write(" batch/jobs=");
        console_write(batches_hex);
        console_write("/");
        console_write(jobs_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "webcam-smoke") || kernel_string_starts_with(command, "webcam-smoke ")) {
        uint64_t count = 32;
        if (kernel_string_starts_with(command, "webcam-smoke ")) {
            if (!kernel_parse_u64_decimal(command + 13, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd webcam-smoke expects [count] in 1..4096\n");
                console_write("[WARN] smp: serial cmd webcam-smoke expects [count] in 1..4096\n");
                return true;
            }
        }
        const uint64_t before_frame = webcam_frame_total();
        const uint64_t before_drop = webcam_frame_drop_total();
        const uint64_t before_processed = webcam_processed_events_total();
        const uint64_t injected = webcam_inject_frames(count);
        const uint64_t after_frame = webcam_frame_total();
        const uint64_t after_drop = webcam_frame_drop_total();
        const uint64_t after_processed = webcam_processed_events_total();
        const uint64_t total = smp_total_cpu_count();
        uint64_t queue_depth_total = 0;
        for (uint64_t cpu = 1; cpu < total; cpu++) {
            queue_depth_total += webcam_queue_depth_for_cpu(cpu);
        }
        const uint64_t delta_frame = after_frame - before_frame;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_frame == count;

        char count_hex[19];
        char injected_hex[19];
        char delta_frame_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_frame_hex, delta_frame);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);

        serial_write(pass ? "[INFO] smp: serial cmd webcam-smoke pass requested/injected="
                          : "[WARN] smp: serial cmd webcam-smoke fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(frame/drop/processed)=");
        serial_write(delta_frame_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd webcam-smoke pass requested/injected="
                           : "[WARN] smp: serial cmd webcam-smoke fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(frame/drop/processed)=");
        console_write(delta_frame_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "webcam-smoke-wait") || kernel_string_starts_with(command, "webcam-smoke-wait ")) {
        uint64_t count = 32;
        uint64_t timeout_ticks = 600;
        if (kernel_string_starts_with(command, "webcam-smoke-wait ")) {
            char count_token[16];
            char timeout_token[16];
            uint64_t cursor = 18;
            if (!kernel_parse_token(command, cursor, count_token, sizeof(count_token), &cursor) ||
                !kernel_parse_u64_decimal(count_token, &count) || count == 0 || count > 4096) {
                serial_write("[WARN] smp: serial cmd webcam-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                console_write("[WARN] smp: serial cmd webcam-smoke-wait expects [count] [timeout-ticks] (count 1..4096)\n");
                return true;
            }
            if (kernel_parse_token(command, cursor, timeout_token, sizeof(timeout_token), &cursor)) {
                if (!kernel_parse_u64_decimal(timeout_token, &timeout_ticks) || timeout_ticks == 0 || timeout_ticks > 10000) {
                    serial_write("[WARN] smp: serial cmd webcam-smoke-wait timeout expects 1..10000 ticks\n");
                    console_write("[WARN] smp: serial cmd webcam-smoke-wait timeout expects 1..10000 ticks\n");
                    return true;
                }
                char extra_token[4];
                if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                    serial_write("[WARN] smp: serial cmd webcam-smoke-wait accepts only [count] [timeout-ticks]\n");
                    console_write("[WARN] smp: serial cmd webcam-smoke-wait accepts only [count] [timeout-ticks]\n");
                    return true;
                }
            }
        }
        const uint64_t before_frame = webcam_frame_total();
        const uint64_t before_drop = webcam_frame_drop_total();
        const uint64_t before_processed = webcam_processed_events_total();
        const uint64_t injected = webcam_inject_frames(count);

        const uint64_t start_tick = timer_ticks();
        uint64_t queue_depth_total = 0;
        bool drained = false;
        while ((timer_ticks() - start_tick) <= timeout_ticks) {
            const uint64_t total = smp_total_cpu_count();
            queue_depth_total = 0;
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                queue_depth_total += webcam_queue_depth_for_cpu(cpu);
            }
            if (queue_depth_total == 0) {
                drained = true;
                break;
            }
            for (uint64_t cpu = 1; cpu < total; cpu++) {
                webcam_process_ap_deferred_job(cpu, 0);
            }
            (void)scheduler_service_deferred_switch();
        }

        const uint64_t after_frame = webcam_frame_total();
        const uint64_t after_drop = webcam_frame_drop_total();
        const uint64_t after_processed = webcam_processed_events_total();
        const uint64_t delta_frame = after_frame - before_frame;
        const uint64_t delta_drop = after_drop - before_drop;
        const uint64_t delta_processed = after_processed - before_processed;
        const bool pass = injected == count && delta_frame == count && drained;

        char count_hex[19];
        char injected_hex[19];
        char delta_frame_hex[19];
        char delta_drop_hex[19];
        char delta_processed_hex[19];
        char queue_depth_hex[19];
        char timeout_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        kernel_format_hex64(delta_frame_hex, delta_frame);
        kernel_format_hex64(delta_drop_hex, delta_drop);
        kernel_format_hex64(delta_processed_hex, delta_processed);
        kernel_format_hex64(queue_depth_hex, queue_depth_total);
        kernel_format_hex64(timeout_hex, timeout_ticks);

        serial_write(pass ? "[INFO] smp: serial cmd webcam-smoke-wait pass requested/injected="
                          : "[WARN] smp: serial cmd webcam-smoke-wait fail requested/injected=");
        serial_write(count_hex);
        serial_write("/");
        serial_write(injected_hex);
        serial_write(" delta(frame/drop/processed)=");
        serial_write(delta_frame_hex);
        serial_write("/");
        serial_write(delta_drop_hex);
        serial_write("/");
        serial_write(delta_processed_hex);
        serial_write(" drained=");
        serial_write(drained ? "1" : "0");
        serial_write(" queue-depth=");
        serial_write(queue_depth_hex);
        serial_write(" timeout=");
        serial_write(timeout_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd webcam-smoke-wait pass requested/injected="
                           : "[WARN] smp: serial cmd webcam-smoke-wait fail requested/injected=");
        console_write(count_hex);
        console_write("/");
        console_write(injected_hex);
        console_write(" delta(frame/drop/processed)=");
        console_write(delta_frame_hex);
        console_write("/");
        console_write(delta_drop_hex);
        console_write("/");
        console_write(delta_processed_hex);
        console_write(" drained=");
        console_write(drained ? "1" : "0");
        console_write(" queue-depth=");
        console_write(queue_depth_hex);
        console_write(" timeout=");
        console_write(timeout_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "webcam-inject ")) {
        const char *count_text = command + 14;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd webcam-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd webcam-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = webcam_inject_frames(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd webcam-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd webcam-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "inject-all ")) {
        const char *count_text = command + 11;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd inject-all expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd inject-all expects count 1..4096\n");
            return true;
        }
        const uint64_t net_injected = networking_inject_rx_events(count);
        const uint64_t usb_injected = usb_inject_events(count);
        const uint64_t storage_injected = filesystem_inject_storage_events(count);
        const uint64_t audio_injected = audio_inject_events(count);
        const uint64_t webcam_injected = webcam_inject_frames(count);
        char count_hex[19];
        char net_hex[19];
        char usb_hex[19];
        char storage_hex[19];
        char audio_hex[19];
        char webcam_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(net_hex, net_injected);
        kernel_format_hex64(usb_hex, usb_injected);
        kernel_format_hex64(storage_hex, storage_injected);
        kernel_format_hex64(audio_hex, audio_injected);
        kernel_format_hex64(webcam_hex, webcam_injected);
        serial_write("[INFO] smp: serial cmd inject-all requested=");
        serial_write(count_hex);
        serial_write(" net/usb/storage/audio/webcam=");
        serial_write(net_hex);
        serial_write("/");
        serial_write(usb_hex);
        serial_write("/");
        serial_write(storage_hex);
        serial_write("/");
        serial_write(audio_hex);
        serial_write("/");
        serial_write(webcam_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd inject-all requested=");
        console_write(count_hex);
        console_write(" net/usb/storage/audio/webcam=");
        console_write(net_hex);
        console_write("/");
        console_write(usb_hex);
        console_write("/");
        console_write(storage_hex);
        console_write("/");
        console_write(audio_hex);
        console_write("/");
        console_write(webcam_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "ap-trigger 0") ||
        kernel_string_equals(command, "ap-trigger 1") ||
        kernel_string_equals(command, "ap-trigger 2")) {
        const uint64_t profile = static_cast<uint64_t>(command[11] - '0');
        __atomic_store_n(&g_ap_trigger_profile, profile, __ATOMIC_RELAXED);
        char profile_hex[19];
        kernel_format_hex64(profile_hex, profile);
        serial_write("[INFO] smp: serial cmd ap-trigger set request=");
        serial_write(profile_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd ap-trigger set request=");
        console_write(profile_hex);
        console_write("\n");
        return true;
    }

    return false;
}
