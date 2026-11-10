#include "fortify/systems/logging/log.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

namespace {
constexpr uint64_t kMaxCpus = 64;
constexpr uint32_t kPortQueueCapacity = 32;
constexpr uint64_t kPortBatchPerApJob = 8;
volatile uint64_t g_maintenance_poll_stride_ticks = 9;
volatile uint64_t g_deferred_jobs_trigger = 2;
volatile uint64_t g_ap_deferred_job_total = 0;
volatile uint64_t g_ap_deferred_job_by_cpu[kMaxCpus] = {};
volatile uint64_t g_deferred_jobs_waiting_by_cpu[kMaxCpus] = {};
volatile uint32_t g_port_head_by_cpu[kMaxCpus] = {};
volatile uint32_t g_port_tail_by_cpu[kMaxCpus] = {};

struct port_event {
    uint8_t source;
};

port_event g_port_queue_by_cpu[kMaxCpus][kPortQueueCapacity] = {};

uint32_t queue_depth_for_cpu(uint64_t cpu_index) {
    const uint32_t head = __atomic_load_n(&g_port_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_port_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (tail >= head) {
        return tail - head;
    }
    return (kPortQueueCapacity - head) + tail;
}

bool enqueue_port_event(uint64_t cpu_index, uint8_t source) {
    if (cpu_index >= kMaxCpus) {
        return false;
    }
    const uint32_t tail = __atomic_load_n(&g_port_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t next_tail = (tail + 1) % kPortQueueCapacity;
    const uint32_t head = __atomic_load_n(&g_port_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (next_tail == head) {
        return false;
    }
    g_port_queue_by_cpu[cpu_index][tail].source = source;
    __atomic_store_n(&g_port_tail_by_cpu[cpu_index], next_tail, __ATOMIC_RELAXED);
    return true;
}

bool dequeue_port_event(uint64_t cpu_index, port_event *out_event) {
    if (cpu_index >= kMaxCpus || out_event == 0) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_port_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_port_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (head == tail) {
        return false;
    }
    *out_event = g_port_queue_by_cpu[cpu_index][head];
    __atomic_store_n(&g_port_head_by_cpu[cpu_index], (head + 1) % kPortQueueCapacity, __ATOMIC_RELAXED);
    return true;
}
}

bool port_manager_init(void) {
    __atomic_store_n(&g_ap_deferred_job_total, 0, __ATOMIC_RELAXED);
    for (uint64_t i = 0; i < kMaxCpus; i++) {
        __atomic_store_n(&g_ap_deferred_job_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_deferred_jobs_waiting_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_port_head_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_port_tail_by_cpu[i], 0, __ATOMIC_RELAXED);
    }
    log_info("ports: AP maintenance queue + deferred processing ready.");
    return true;
}

bool port_manager_request_ap_maintenance(uint64_t cpu_index) {
    if (cpu_index == 0 || cpu_index >= kMaxCpus) {
        return false;
    }
    if (!enqueue_port_event(cpu_index, 1)) {
        return false;
    }
    (void)scheduler_enqueue_ap_maintenance_job(cpu_index);
    return true;
}

bool port_manager_poll_ap_maintenance_trigger(uint64_t cpu_index, uint64_t tick) {
    if (cpu_index == 0 || cpu_index >= kMaxCpus) {
        return false;
    }
    const uint64_t stride = __atomic_load_n(&g_maintenance_poll_stride_ticks, __ATOMIC_RELAXED);
    if (stride == 0 || (tick % stride) != 0) {
        return false;
    }
    const uint64_t trigger = __atomic_load_n(&g_deferred_jobs_trigger, __ATOMIC_RELAXED);
    const uint64_t waiting_jobs =
        __atomic_add_fetch(&g_deferred_jobs_waiting_by_cpu[cpu_index], 1, __ATOMIC_RELAXED);
    if (trigger == 0 || waiting_jobs < trigger) {
        return false;
    }
    if (queue_depth_for_cpu(cpu_index) >= kPortQueueCapacity - 1) {
        return false;
    }
    if (!port_manager_request_ap_maintenance(cpu_index)) {
        return false;
    }
    __atomic_store_n(&g_deferred_jobs_waiting_by_cpu[cpu_index], 0, __ATOMIC_RELAXED);
    return true;
}

bool port_manager_set_ap_maintenance_trigger_config(
    uint64_t poll_stride_ticks,
    uint64_t deferred_jobs_trigger
) {
    if (poll_stride_ticks == 0 || deferred_jobs_trigger == 0) {
        return false;
    }
    __atomic_store_n(&g_maintenance_poll_stride_ticks, poll_stride_ticks, __ATOMIC_RELAXED);
    __atomic_store_n(&g_deferred_jobs_trigger, deferred_jobs_trigger, __ATOMIC_RELAXED);
    return true;
}

void port_manager_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence) {
    (void)sequence;
    if (cpu_index >= kMaxCpus) {
        return;
    }
    uint64_t processed = 0;
    for (uint64_t i = 0; i < kPortBatchPerApJob; i++) {
        port_event event = {};
        if (!dequeue_port_event(cpu_index, &event)) {
            break;
        }
        (void)event.source;
        processed++;
    }
    if (processed == 0) {
        return;
    }
    __atomic_fetch_add(&g_ap_deferred_job_total, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_ap_deferred_job_by_cpu[cpu_index], 1, __ATOMIC_RELAXED);
}

uint64_t port_manager_ap_deferred_job_total(void) {
    return __atomic_load_n(&g_ap_deferred_job_total, __ATOMIC_RELAXED);
}

uint64_t port_manager_ap_deferred_job_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_deferred_job_by_cpu[cpu_index], __ATOMIC_RELAXED);
}
