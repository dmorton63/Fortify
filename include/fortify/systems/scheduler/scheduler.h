#ifndef FORTIFY_SCHEDULER_H
#define FORTIFY_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "fortify/systems/interrupts/interrupts.h"

bool scheduler_init(void);
void scheduler_start(void);
interrupt_frame *scheduler_on_timer_tick(interrupt_frame *frame);
interrupt_frame *scheduler_on_ipi_kick(interrupt_frame *frame);
bool scheduler_service_deferred_switch(void);
bool scheduler_yield(void);
bool scheduler_enqueue_ap_maintenance_job(uint64_t cpu_index);
bool scheduler_enqueue_ap_networking_job(uint64_t cpu_index);
bool scheduler_start_ap_pinned_demo_tasks(void);
void scheduler_stop_ap_pinned_demo_tasks(void);
bool scheduler_dispatch_ap_pinned_tasks(void);
uint64_t scheduler_ap_pinned_task_execute_total(void);
uint64_t scheduler_ap_pinned_task_execute_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_pinned_task_active_count(void);
void scheduler_set_ap_pinned_task_migration_enabled(bool enabled);
bool scheduler_ap_pinned_task_migration_enabled(void);
uint64_t scheduler_ap_pinned_task_migration_total(void);
uint64_t scheduler_ap_pinned_task_seen_cpu_mask(void);
uint64_t scheduler_ap_pinned_task_assigned_cpu_mask(void);
bool scheduler_force_ap_pinned_task_migration_step(void);
uint64_t scheduler_ap_to_ap_switch_total(void);
uint64_t scheduler_ap_to_ap_switch_for_cpu(uint64_t cpu_index);
uint64_t scheduler_bsp_to_ap_switch_total(void);
uint64_t scheduler_bsp_to_ap_switch_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_worker_threads_provisioned_total(void);
uint64_t scheduler_ap_pinned_threads_provisioned_total(void);
uint64_t scheduler_ap_threads_expected_total(void);

uint64_t scheduler_tick_count(void);
uint64_t scheduler_preempt_count(void);
uint64_t scheduler_ipi_preempt_requests_total(void);
uint64_t scheduler_ipi_preempt_requests_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_dispatch_total(void);
uint64_t scheduler_ap_work_dispatch_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_execute_total(void);
uint64_t scheduler_ap_work_execute_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_maintenance_dispatch_total(void);
uint64_t scheduler_ap_work_maintenance_dispatch_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_maintenance_execute_total(void);
uint64_t scheduler_ap_work_maintenance_execute_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_networking_dispatch_total(void);
uint64_t scheduler_ap_work_networking_dispatch_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_networking_execute_total(void);
uint64_t scheduler_ap_work_networking_execute_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_skip_rate_limit_total(void);
uint64_t scheduler_ap_work_skip_rate_limit_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_skip_pending_cap_total(void);
uint64_t scheduler_ap_work_skip_pending_cap_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_drop_total(void);
uint64_t scheduler_ap_work_drop_for_cpu(uint64_t cpu_index);
uint64_t scheduler_ap_work_queue_depth_for_cpu(uint64_t cpu_index);
uint64_t scheduler_current_thread_index(void);
const char *scheduler_current_thread_name(void);
uint64_t scheduler_stack_owner_index_for_address(uint64_t address);
void scheduler_mark_current_cpu_online(void);
uint64_t scheduler_online_cpu_count(void);

#endif
