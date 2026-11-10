#include "fortify/systems/kernel/smp_diagnostics.h"

#include "fortify/systems/kernel/ap_trigger_profile.h"
#include "fortify/systems/kernel/kernel_string.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

void log_smp_online_update(uint64_t online, uint64_t total, uint64_t scheduler_online) {
    char online_hex[19];
    char total_hex[19];
    char sched_hex[19];
    kernel_format_hex64(online_hex, online);
    kernel_format_hex64(total_hex, total);
    kernel_format_hex64(sched_hex, scheduler_online);

    serial_write("[INFO] smp: online cpus=");
    serial_write(online_hex);
    serial_write("/");
    serial_write(total_hex);
    serial_write(" scheduler-online=");
    serial_write(sched_hex);
    serial_write("\n");

    console_write("[INFO] smp: online cpus=");
    console_write(online_hex);
    console_write("/");
    console_write(total_hex);
    console_write(" scheduler-online=");
    console_write(sched_hex);
    console_write("\n");
}

void log_smp_ap_wake_update(uint64_t total_wakes) {
    char wakes_hex[19];
    kernel_format_hex64(wakes_hex, total_wakes);

    serial_write("[INFO] smp: AP wake count total=");
    serial_write(wakes_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP wake count total=");
    console_write(wakes_hex);
    console_write("\n");
}

void log_smp_ap_ready_update(uint64_t ready, uint64_t expected) {
    char ready_hex[19];
    char expected_hex[19];
    kernel_format_hex64(ready_hex, ready);
    kernel_format_hex64(expected_hex, expected);

    serial_write("[INFO] smp: AP interruptible-ready=");
    serial_write(ready_hex);
    serial_write("/");
    serial_write(expected_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP interruptible-ready=");
    console_write(ready_hex);
    console_write("/");
    console_write(expected_hex);
    console_write("\n");
}

void log_smp_ipi_observed_update(uint64_t observed_total) {
    char observed_hex[19];
    kernel_format_hex64(observed_hex, observed_total);

    serial_write("[INFO] smp: IPI kick interrupt observed total=");
    serial_write(observed_hex);
    serial_write("\n");

    console_write("[INFO] smp: IPI kick interrupt observed total=");
    console_write(observed_hex);
    console_write("\n");
}

void log_smp_ipi_scheduler_update(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: IPI scheduler preempt requests total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: IPI scheduler preempt requests total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ipi_scheduler_cpu_update(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] smp: IPI scheduler preempt cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] smp: IPI scheduler preempt cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_ap_work_dispatch_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: AP work dispatch total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP work dispatch total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_work_dispatch_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] smp: AP work dispatch cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP work dispatch cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_ap_work_execute_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: AP work execute total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP work execute total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_work_execute_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] smp: AP work execute cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP work execute cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_ap_maintenance_dispatch_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: AP maintenance dispatch total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP maintenance dispatch total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_maintenance_dispatch_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] smp: AP maintenance dispatch cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP maintenance dispatch cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_ap_maintenance_execute_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: AP maintenance execute total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP maintenance execute total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_maintenance_execute_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] smp: AP maintenance execute cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP maintenance execute cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_ap_networking_dispatch_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: AP networking dispatch total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP networking dispatch total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_networking_dispatch_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] smp: AP networking dispatch cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP networking dispatch cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_ap_networking_execute_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: AP networking execute total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP networking execute total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_networking_execute_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] smp: AP networking execute cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP networking execute cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_port_ap_job_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] ports: AP deferred jobs total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] ports: AP deferred jobs total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_port_ap_job_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] ports: AP deferred job cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] ports: AP deferred job cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_networking_ap_job_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] networking: AP deferred jobs total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] networking: AP deferred jobs total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_networking_rx_packets_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] networking: AP RX packets total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] networking: AP RX packets total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_networking_rx_batches_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] networking: AP RX batches total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] networking: AP RX batches total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_filesystem_ap_job_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] filesystem: AP deferred jobs total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] filesystem: AP deferred jobs total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_filesystem_ap_job_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] filesystem: AP deferred job cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] filesystem: AP deferred job cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_usb_ap_job_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] usb: AP deferred jobs total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] usb: AP deferred jobs total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_networking_ap_job_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] networking: AP deferred job cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] networking: AP deferred job cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_networking_rx_packets_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] networking: AP RX packets cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] networking: AP RX packets cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_networking_rx_batches_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] networking: AP RX batches cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] networking: AP RX batches cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_usb_ap_job_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[INFO] usb: AP deferred job cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[INFO] usb: AP deferred job cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_ap_work_queue_depth_total(uint64_t depth_total, uint64_t nonzero_cpus) {
    char depth_hex[19];
    char cpus_hex[19];
    kernel_format_hex64(depth_hex, depth_total);
    kernel_format_hex64(cpus_hex, nonzero_cpus);

    serial_write("[INFO] smp: AP work queue depth total=");
    serial_write(depth_hex);
    serial_write(" nonzero-cpus=");
    serial_write(cpus_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP work queue depth total=");
    console_write(depth_hex);
    console_write(" nonzero-cpus=");
    console_write(cpus_hex);
    console_write("\n");
}

void log_smp_ap_work_drop_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[WARN] smp: AP work drop total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[WARN] smp: AP work drop total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_work_skip_rate_limit_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: AP work enqueue skip rate-limit total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP work enqueue skip rate-limit total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_work_skip_pending_cap_total(uint64_t total) {
    char total_hex[19];
    kernel_format_hex64(total_hex, total);

    serial_write("[INFO] smp: AP work enqueue skip pending-cap total=");
    serial_write(total_hex);
    serial_write("\n");

    console_write("[INFO] smp: AP work enqueue skip pending-cap total=");
    console_write(total_hex);
    console_write("\n");
}

void log_smp_ap_work_drop_cpu(uint64_t cpu_index, uint64_t count) {
    char cpu_hex[19];
    char count_hex[19];
    kernel_format_hex64(cpu_hex, cpu_index);
    kernel_format_hex64(count_hex, count);

    serial_write("[WARN] smp: AP work drop cpu=");
    serial_write(cpu_hex);
    serial_write(" count=");
    serial_write(count_hex);
    serial_write("\n");

    console_write("[WARN] smp: AP work drop cpu=");
    console_write(cpu_hex);
    console_write(" count=");
    console_write(count_hex);
    console_write("\n");
}

void log_smp_self_test_summary(bool pass, uint64_t ap_ok, uint64_t ap_expected,
                                      uint64_t dispatch_total, uint64_t execute_total) {
    char ap_ok_hex[19];
    char ap_expected_hex[19];
    char dispatch_hex[19];
    char execute_hex[19];
    kernel_format_hex64(ap_ok_hex, ap_ok);
    kernel_format_hex64(ap_expected_hex, ap_expected);
    kernel_format_hex64(dispatch_hex, dispatch_total);
    kernel_format_hex64(execute_hex, execute_total);

    serial_write("[INFO] smp: self-test ap-work=");
    serial_write(pass ? "pass" : "pending");
    serial_write(" ap-ok=");
    serial_write(ap_ok_hex);
    serial_write("/");
    serial_write(ap_expected_hex);
    serial_write(" dispatch=");
    serial_write(dispatch_hex);
    serial_write(" execute=");
    serial_write(execute_hex);
    serial_write("\n");

    console_write("[INFO] smp: self-test ap-work=");
    console_write(pass ? "pass" : "pending");
    console_write(" ap-ok=");
    console_write(ap_ok_hex);
    console_write("/");
    console_write(ap_expected_hex);
    console_write(" dispatch=");
    console_write(dispatch_hex);
    console_write(" execute=");
    console_write(execute_hex);
    console_write("\n");
}

void log_smp_migration_self_test_summary(bool pass,
                                                uint64_t active_tasks,
                                                uint64_t migration_total,
                                                uint64_t seen_cpu_mask,
                                                uint64_t expected_cpu_mask) {
    char active_hex[19];
    char migration_hex[19];
    char seen_hex[19];
    char expected_hex[19];
    kernel_format_hex64(active_hex, active_tasks);
    kernel_format_hex64(migration_hex, migration_total);
    kernel_format_hex64(seen_hex, seen_cpu_mask);
    kernel_format_hex64(expected_hex, expected_cpu_mask);

    serial_write("[INFO] smp: self-test migration=");
    serial_write(pass ? "pass" : "pending");
    serial_write(" active=");
    serial_write(active_hex);
    serial_write(" moves=");
    serial_write(migration_hex);
    serial_write(" mask=");
    serial_write(seen_hex);
    serial_write(" expected=");
    serial_write(expected_hex);
    serial_write("\n");

    console_write("[INFO] smp: self-test migration=");
    console_write(pass ? "pass" : "pending");
    console_write(" active=");
    console_write(active_hex);
    console_write(" moves=");
    console_write(migration_hex);
    console_write(" mask=");
    console_write(seen_hex);
    console_write(" expected=");
    console_write(expected_hex);
    console_write("\n");
}

void log_smp_ap_work_kinds_summary(uint64_t maintenance_execute_total,
                                          uint64_t networking_execute_total,
                                          uint64_t port_ap_job_total,
                                          uint64_t networking_ap_job_total,
                                          uint64_t filesystem_ap_job_total,
                                          uint64_t usb_ap_job_total,
                                          uint64_t ap_pinned_execute_total) {
    char maintenance_hex[19];
    char networking_hex[19];
    char port_hex[19];
    char net_job_hex[19];
    char fs_job_hex[19];
    char usb_job_hex[19];
    char pinned_hex[19];
    kernel_format_hex64(maintenance_hex, maintenance_execute_total);
    kernel_format_hex64(networking_hex, networking_execute_total);
    kernel_format_hex64(port_hex, port_ap_job_total);
    kernel_format_hex64(net_job_hex, networking_ap_job_total);
    kernel_format_hex64(fs_job_hex, filesystem_ap_job_total);
    kernel_format_hex64(usb_job_hex, usb_ap_job_total);
    kernel_format_hex64(pinned_hex, ap_pinned_execute_total);

    serial_write("[INFO] smp: ap-work kinds maintenance/net=");
    serial_write(maintenance_hex);
    serial_write("/");
    serial_write(networking_hex);
    serial_write(" subsystem-jobs ports/net/fs/usb=");
    serial_write(port_hex);
    serial_write("/");
    serial_write(net_job_hex);
    serial_write("/");
    serial_write(fs_job_hex);
    serial_write("/");
    serial_write(usb_job_hex);
    serial_write(" ap-task=");
    serial_write(pinned_hex);
    serial_write("\n");

    console_write("[INFO] smp: ap-work kinds maintenance/net=");
    console_write(maintenance_hex);
    console_write("/");
    console_write(networking_hex);
    console_write(" subsystem-jobs ports/net/fs/usb=");
    console_write(port_hex);
    console_write("/");
    console_write(net_job_hex);
    console_write("/");
    console_write(fs_job_hex);
    console_write("/");
    console_write(usb_job_hex);
    console_write(" ap-task=");
    console_write(pinned_hex);
    console_write("\n");
}

void log_smp_health_snapshot(uint64_t online,
                                    uint64_t total,
                                    uint64_t ipi_observed,
                                    uint64_t ipi_scheduler,
                                    uint64_t ap_dispatch,
                                    uint64_t ap_execute,
                                    uint64_t queue_depth,
                                    uint64_t drops,
                                    uint64_t skip_rate_limit,
                                    uint64_t skip_pending_cap,
                                    uint64_t ap_pinned_active,
                                    uint64_t ap_pinned_execute,
                                    uint64_t ap_pinned_migrations,
                                    uint64_t ap_pinned_seen_cpu_mask,
                                    uint64_t ap_to_ap_switch,
                                    uint64_t bsp_to_ap_switch,
                                    uint64_t runtime_msg_queue_high_water,
                                    uint64_t runtime_msg_input_queue_high_water,
                                    uint64_t runtime_msg_command_queue_high_water,
                                    uint64_t runtime_msg_dispatch_input,
                                    uint64_t runtime_msg_dispatch_command,
                                    uint64_t runtime_msg_route_drop,
                                    uint64_t runtime_msg_route_drop_invalid_source,
                                    uint64_t runtime_msg_route_drop_keyboard_suppressed,
                                    uint64_t runtime_msg_route_drop_unroutable,
                                    uint64_t runtime_msg_input_queue_depth,
                                    uint64_t runtime_msg_command_queue_depth,
                                    uint64_t runtime_msg_input_queue_drop,
                                    uint64_t runtime_msg_command_queue_drop,
                                    uint64_t runtime_msg_input_queue_enqueued,
                                    uint64_t runtime_msg_input_queue_dequeued,
                                    uint64_t runtime_msg_command_queue_enqueued,
                                    uint64_t runtime_msg_command_queue_dequeued,
                                    uint64_t trigger_profile,
                                    bool self_test_pass) {
    const char *profile_name = ap_trigger_profile_name(trigger_profile);
    char online_hex[19];
    char total_hex[19];
    char ipi_observed_hex[19];
    char ipi_scheduler_hex[19];
    char dispatch_hex[19];
    char execute_hex[19];
    char depth_hex[19];
    char drops_hex[19];
    char skip_rl_hex[19];
    char skip_pc_hex[19];
    char pinned_active_hex[19];
    char pinned_execute_hex[19];
    char pinned_migrations_hex[19];
    char pinned_seen_cpu_mask_hex[19];
    char ap_to_ap_switch_hex[19];
    char bsp_to_ap_switch_hex[19];
    char runtime_msg_queue_high_water_hex[19];
    char runtime_msg_input_queue_high_water_hex[19];
    char runtime_msg_command_queue_high_water_hex[19];
    char runtime_msg_dispatch_input_hex[19];
    char runtime_msg_dispatch_command_hex[19];
    char runtime_msg_route_drop_hex[19];
    char runtime_msg_route_drop_invalid_source_hex[19];
    char runtime_msg_route_drop_keyboard_suppressed_hex[19];
    char runtime_msg_route_drop_unroutable_hex[19];
    char runtime_msg_input_queue_depth_hex[19];
    char runtime_msg_command_queue_depth_hex[19];
    char runtime_msg_input_queue_drop_hex[19];
    char runtime_msg_command_queue_drop_hex[19];
    char runtime_msg_input_queue_enqueued_hex[19];
    char runtime_msg_input_queue_dequeued_hex[19];
    char runtime_msg_command_queue_enqueued_hex[19];
    char runtime_msg_command_queue_dequeued_hex[19];
    char profile_hex[19];
    kernel_format_hex64(online_hex, online);
    kernel_format_hex64(total_hex, total);
    kernel_format_hex64(ipi_observed_hex, ipi_observed);
    kernel_format_hex64(ipi_scheduler_hex, ipi_scheduler);
    kernel_format_hex64(dispatch_hex, ap_dispatch);
    kernel_format_hex64(execute_hex, ap_execute);
    kernel_format_hex64(depth_hex, queue_depth);
    kernel_format_hex64(drops_hex, drops);
    kernel_format_hex64(skip_rl_hex, skip_rate_limit);
    kernel_format_hex64(skip_pc_hex, skip_pending_cap);
    kernel_format_hex64(pinned_active_hex, ap_pinned_active);
    kernel_format_hex64(pinned_execute_hex, ap_pinned_execute);
    kernel_format_hex64(pinned_migrations_hex, ap_pinned_migrations);
    kernel_format_hex64(pinned_seen_cpu_mask_hex, ap_pinned_seen_cpu_mask);
    kernel_format_hex64(ap_to_ap_switch_hex, ap_to_ap_switch);
    kernel_format_hex64(bsp_to_ap_switch_hex, bsp_to_ap_switch);
    kernel_format_hex64(runtime_msg_queue_high_water_hex, runtime_msg_queue_high_water);
    kernel_format_hex64(runtime_msg_input_queue_high_water_hex, runtime_msg_input_queue_high_water);
    kernel_format_hex64(runtime_msg_command_queue_high_water_hex, runtime_msg_command_queue_high_water);
    kernel_format_hex64(runtime_msg_dispatch_input_hex, runtime_msg_dispatch_input);
    kernel_format_hex64(runtime_msg_dispatch_command_hex, runtime_msg_dispatch_command);
    kernel_format_hex64(runtime_msg_route_drop_hex, runtime_msg_route_drop);
    kernel_format_hex64(runtime_msg_route_drop_invalid_source_hex, runtime_msg_route_drop_invalid_source);
    kernel_format_hex64(runtime_msg_route_drop_keyboard_suppressed_hex,
                        runtime_msg_route_drop_keyboard_suppressed);
    kernel_format_hex64(runtime_msg_route_drop_unroutable_hex, runtime_msg_route_drop_unroutable);
    kernel_format_hex64(runtime_msg_input_queue_depth_hex, runtime_msg_input_queue_depth);
    kernel_format_hex64(runtime_msg_command_queue_depth_hex, runtime_msg_command_queue_depth);
    kernel_format_hex64(runtime_msg_input_queue_drop_hex, runtime_msg_input_queue_drop);
    kernel_format_hex64(runtime_msg_command_queue_drop_hex, runtime_msg_command_queue_drop);
    kernel_format_hex64(runtime_msg_input_queue_enqueued_hex, runtime_msg_input_queue_enqueued);
    kernel_format_hex64(runtime_msg_input_queue_dequeued_hex, runtime_msg_input_queue_dequeued);
    kernel_format_hex64(runtime_msg_command_queue_enqueued_hex, runtime_msg_command_queue_enqueued);
    kernel_format_hex64(runtime_msg_command_queue_dequeued_hex, runtime_msg_command_queue_dequeued);
    kernel_format_hex64(profile_hex, trigger_profile);

    serial_write("[INFO] smp: health online=");
    serial_write(online_hex);
    serial_write("/");
    serial_write(total_hex);
    serial_write(" ipi(obs/req)=");
    serial_write(ipi_observed_hex);
    serial_write("/");
    serial_write(ipi_scheduler_hex);
    serial_write(" work(d/e)=");
    serial_write(dispatch_hex);
    serial_write("/");
    serial_write(execute_hex);
    serial_write(" q=");
    serial_write(depth_hex);
    serial_write(" drop=");
    serial_write(drops_hex);
    serial_write(" skip(rl/pc)=");
    serial_write(skip_rl_hex);
    serial_write("/");
    serial_write(skip_pc_hex);
    serial_write(" ap-task(act/exec)=");
    serial_write(pinned_active_hex);
    serial_write("/");
    serial_write(pinned_execute_hex);
    serial_write(" mig/mask=");
    serial_write(pinned_migrations_hex);
    serial_write("/");
    serial_write(pinned_seen_cpu_mask_hex);
    serial_write(" ctx(ap2ap/bsp2ap)=");
    serial_write(ap_to_ap_switch_hex);
    serial_write("/");
    serial_write(bsp_to_ap_switch_hex);
    serial_write(" rmsg(in/cmd/drop)=");
    serial_write(runtime_msg_dispatch_input_hex);
    serial_write("/");
    serial_write(runtime_msg_dispatch_command_hex);
    serial_write("/");
    serial_write(runtime_msg_route_drop_hex);
    serial_write(" rmsg-drop-reason(src/kbd/unroute)=");
    serial_write(runtime_msg_route_drop_invalid_source_hex);
    serial_write("/");
    serial_write(runtime_msg_route_drop_keyboard_suppressed_hex);
    serial_write("/");
    serial_write(runtime_msg_route_drop_unroutable_hex);
    serial_write(" rmsg-hwm=");
    serial_write(runtime_msg_queue_high_water_hex);
    serial_write(" rmsg-target-q(in/cmd)=");
    serial_write(runtime_msg_input_queue_depth_hex);
    serial_write("/");
    serial_write(runtime_msg_command_queue_depth_hex);
    serial_write(" rmsg-target-hwm(in/cmd)=");
    serial_write(runtime_msg_input_queue_high_water_hex);
    serial_write("/");
    serial_write(runtime_msg_command_queue_high_water_hex);
    serial_write(" rmsg-target-drop(in/cmd)=");
    serial_write(runtime_msg_input_queue_drop_hex);
    serial_write("/");
    serial_write(runtime_msg_command_queue_drop_hex);
    serial_write(" rmsg-target-flow(in e/d cmd e/d)=");
    serial_write(runtime_msg_input_queue_enqueued_hex);
    serial_write("/");
    serial_write(runtime_msg_input_queue_dequeued_hex);
    serial_write(" ");
    serial_write(runtime_msg_command_queue_enqueued_hex);
    serial_write("/");
    serial_write(runtime_msg_command_queue_dequeued_hex);
    serial_write(" profile=");
    serial_write(profile_hex);
    serial_write("/");
    serial_write(profile_name);
    serial_write(" selftest=");
    serial_write(self_test_pass ? "pass" : "pending");
    serial_write("\n");

    console_write("[INFO] smp: health online=");
    console_write(online_hex);
    console_write("/");
    console_write(total_hex);
    console_write(" ipi(obs/req)=");
    console_write(ipi_observed_hex);
    console_write("/");
    console_write(ipi_scheduler_hex);
    console_write(" work(d/e)=");
    console_write(dispatch_hex);
    console_write("/");
    console_write(execute_hex);
    console_write(" q=");
    console_write(depth_hex);
    console_write(" drop=");
    console_write(drops_hex);
    console_write(" skip(rl/pc)=");
    console_write(skip_rl_hex);
    console_write("/");
    console_write(skip_pc_hex);
    console_write(" ap-task(act/exec)=");
    console_write(pinned_active_hex);
    console_write("/");
    console_write(pinned_execute_hex);
    console_write(" mig/mask=");
    console_write(pinned_migrations_hex);
    console_write("/");
    console_write(pinned_seen_cpu_mask_hex);
    console_write(" ctx(ap2ap/bsp2ap)=");
    console_write(ap_to_ap_switch_hex);
    console_write("/");
    console_write(bsp_to_ap_switch_hex);
    console_write(" rmsg(in/cmd/drop)=");
    console_write(runtime_msg_dispatch_input_hex);
    console_write("/");
    console_write(runtime_msg_dispatch_command_hex);
    console_write("/");
    console_write(runtime_msg_route_drop_hex);
    console_write(" rmsg-drop-reason(src/kbd/unroute)=");
    console_write(runtime_msg_route_drop_invalid_source_hex);
    console_write("/");
    console_write(runtime_msg_route_drop_keyboard_suppressed_hex);
    console_write("/");
    console_write(runtime_msg_route_drop_unroutable_hex);
    console_write(" rmsg-hwm=");
    console_write(runtime_msg_queue_high_water_hex);
    console_write(" rmsg-target-q(in/cmd)=");
    console_write(runtime_msg_input_queue_depth_hex);
    console_write("/");
    console_write(runtime_msg_command_queue_depth_hex);
    console_write(" rmsg-target-hwm(in/cmd)=");
    console_write(runtime_msg_input_queue_high_water_hex);
    console_write("/");
    console_write(runtime_msg_command_queue_high_water_hex);
    console_write(" rmsg-target-drop(in/cmd)=");
    console_write(runtime_msg_input_queue_drop_hex);
    console_write("/");
    console_write(runtime_msg_command_queue_drop_hex);
    console_write(" rmsg-target-flow(in e/d cmd e/d)=");
    console_write(runtime_msg_input_queue_enqueued_hex);
    console_write("/");
    console_write(runtime_msg_input_queue_dequeued_hex);
    console_write(" ");
    console_write(runtime_msg_command_queue_enqueued_hex);
    console_write("/");
    console_write(runtime_msg_command_queue_dequeued_hex);
    console_write(" profile=");
    console_write(profile_hex);
    console_write("/");
    console_write(profile_name);
    console_write(" selftest=");
    console_write(self_test_pass ? "pass" : "pending");
    console_write("\n");
}

void log_smp_kick_mode(uint64_t mode) {
    char mode_hex[19];
    kernel_format_hex64(mode_hex, mode);
    serial_write("[INFO] smp: IPI kick mode=");
    serial_write(mode_hex);
    serial_write(" (1=directed,2=bcast-excl-self,3=bcast-incl-self)\n");
    console_write("[INFO] smp: IPI kick mode=");
    console_write(mode_hex);
    console_write(" (1=directed,2=bcast-excl-self,3=bcast-incl-self)\n");
}

void log_smp_ipi_diag_snapshot(void) {
    char mode_hex[19];
    char path_hex[19];
    char target_hex[19];
    char icr_low_hex[19];
    char icr_high_hex[19];
    char esr_before_hex[19];
    char esr_after_hex[19];
    char result_hex[19];

    kernel_format_hex64(mode_hex, smp_ipi_kick_diag_mode());
    kernel_format_hex64(path_hex, smp_ipi_kick_diag_path());
    kernel_format_hex64(target_hex, smp_ipi_kick_diag_target());
    kernel_format_hex64(icr_low_hex, smp_ipi_kick_diag_icr_low());
    kernel_format_hex64(icr_high_hex, smp_ipi_kick_diag_icr_high());
    kernel_format_hex64(esr_before_hex, smp_ipi_kick_diag_esr_before());
    kernel_format_hex64(esr_after_hex, smp_ipi_kick_diag_esr_after());
    kernel_format_hex64(result_hex, smp_ipi_kick_diag_result());

    serial_write("[INFO] smp: IPI diag mode=");
    serial_write(mode_hex);
    serial_write(" path=");
    serial_write(path_hex);
    serial_write(" target=");
    serial_write(target_hex);
    serial_write(" icr.low=");
    serial_write(icr_low_hex);
    serial_write(" icr.high=");
    serial_write(icr_high_hex);
    serial_write(" esr.before=");
    serial_write(esr_before_hex);
    serial_write(" esr.after=");
    serial_write(esr_after_hex);
    serial_write(" result=");
    serial_write(result_hex);
    serial_write(" (path:1=x2apic,2=mmio)\n");

    console_write("[INFO] smp: IPI diag mode=");
    console_write(mode_hex);
    console_write(" path=");
    console_write(path_hex);
    console_write(" target=");
    console_write(target_hex);
    console_write(" icr.low=");
    console_write(icr_low_hex);
    console_write(" icr.high=");
    console_write(icr_high_hex);
    console_write(" esr.before=");
    console_write(esr_before_hex);
    console_write(" esr.after=");
    console_write(esr_after_hex);
    console_write(" result=");
    console_write(result_hex);
    console_write(" (path:1=x2apic,2=mmio)\n");
}
