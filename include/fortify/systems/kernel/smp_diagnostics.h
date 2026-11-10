#ifndef FORTIFY_KERNEL_SMP_DIAGNOSTICS_H
#define FORTIFY_KERNEL_SMP_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

void log_smp_online_update(uint64_t online, uint64_t total, uint64_t scheduler_online);
void log_smp_ap_wake_update(uint64_t total_wakes);
void log_smp_ap_ready_update(uint64_t ready, uint64_t expected);
void log_smp_ipi_observed_update(uint64_t observed_total);
void log_smp_ipi_scheduler_update(uint64_t total);
void log_smp_ipi_scheduler_cpu_update(uint64_t cpu_index, uint64_t count);
void log_smp_ap_work_dispatch_total(uint64_t total);
void log_smp_ap_work_dispatch_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_ap_work_execute_total(uint64_t total);
void log_smp_ap_work_execute_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_ap_maintenance_dispatch_total(uint64_t total);
void log_smp_ap_maintenance_dispatch_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_ap_maintenance_execute_total(uint64_t total);
void log_smp_ap_maintenance_execute_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_ap_networking_dispatch_total(uint64_t total);
void log_smp_ap_networking_dispatch_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_ap_networking_execute_total(uint64_t total);
void log_smp_ap_networking_execute_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_port_ap_job_total(uint64_t total);
void log_smp_port_ap_job_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_networking_ap_job_total(uint64_t total);
void log_smp_networking_rx_packets_total(uint64_t total);
void log_smp_networking_rx_batches_total(uint64_t total);
void log_smp_filesystem_ap_job_total(uint64_t total);
void log_smp_filesystem_ap_job_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_usb_ap_job_total(uint64_t total);
void log_smp_networking_ap_job_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_networking_rx_packets_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_networking_rx_batches_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_usb_ap_job_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_ap_work_queue_depth_total(uint64_t depth_total, uint64_t nonzero_cpus);
void log_smp_ap_work_drop_total(uint64_t total);
void log_smp_ap_work_skip_rate_limit_total(uint64_t total);
void log_smp_ap_work_skip_pending_cap_total(uint64_t total);
void log_smp_ap_work_drop_cpu(uint64_t cpu_index, uint64_t count);
void log_smp_self_test_summary(bool pass, uint64_t ap_ok, uint64_t ap_expected,
                               uint64_t dispatch_total, uint64_t execute_total);
void log_smp_migration_self_test_summary(bool pass,
                                         uint64_t active_total,
                                         uint64_t migration_total,
                                         uint64_t seen_cpu_mask,
                                         uint64_t expected_cpu_mask);
void log_smp_ap_work_kinds_summary(uint64_t maintenance_execute_total,
                                   uint64_t networking_execute_total,
                                   uint64_t ports_jobs_total,
                                   uint64_t networking_jobs_total,
                                   uint64_t filesystem_jobs_total,
                                   uint64_t usb_jobs_total,
                                   uint64_t ap_task_execute_total);
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
                             bool self_test_pass);
void log_smp_kick_mode(uint64_t mode);
void log_smp_ipi_diag_snapshot(void);

#endif
