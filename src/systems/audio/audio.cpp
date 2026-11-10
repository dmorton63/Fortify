#include "fortify/systems/logging/log.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

namespace {
constexpr uint64_t kMaxCpus = 64;
constexpr uint32_t kAudioQueueCapacity = 32;
volatile uint64_t g_poll_stride_ticks = 1;
volatile uint64_t g_queue_trigger = 1;

struct audio_event {
    uint8_t source;
};

volatile uint64_t g_audio_irq_total = 0;
volatile uint64_t g_audio_irq_drop_total = 0;
volatile uint64_t g_audio_ap_deferred_job_total = 0;
volatile uint64_t g_next_target_ap = 1;
volatile uint32_t g_audio_head_by_cpu[kMaxCpus] = {};
volatile uint32_t g_audio_tail_by_cpu[kMaxCpus] = {};
audio_event g_audio_queue_by_cpu[kMaxCpus][kAudioQueueCapacity] = {};

bool pick_target_ap(uint64_t *out_cpu) {
    if (out_cpu == 0) {
        return false;
    }
    const uint64_t total = smp_total_cpu_count();
    if (total <= 1) {
        return false;
    }
    uint64_t next = __atomic_load_n(&g_next_target_ap, __ATOMIC_RELAXED);
    if (next >= total || next == 0) {
        next = 1;
    }
    *out_cpu = next;
    next++;
    if (next >= total) {
        next = 1;
    }
    __atomic_store_n(&g_next_target_ap, next, __ATOMIC_RELAXED);
    return true;
}

uint32_t queue_depth_for_cpu(uint64_t cpu_index) {
    const uint32_t head = __atomic_load_n(&g_audio_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_audio_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (tail >= head) {
        return tail - head;
    }
    return (kAudioQueueCapacity - head) + tail;
}

bool enqueue_audio_event(uint64_t cpu_index, uint8_t source) {
    if (cpu_index >= kMaxCpus) {
        return false;
    }
    const uint32_t tail = __atomic_load_n(&g_audio_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t next_tail = (tail + 1) % kAudioQueueCapacity;
    const uint32_t head = __atomic_load_n(&g_audio_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (next_tail == head) {
        return false;
    }
    g_audio_queue_by_cpu[cpu_index][tail].source = source;
    __atomic_store_n(&g_audio_tail_by_cpu[cpu_index], next_tail, __ATOMIC_RELAXED);
    return true;
}

bool dequeue_audio_event(uint64_t cpu_index, audio_event *out_event) {
    if (cpu_index >= kMaxCpus || out_event == 0) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_audio_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_audio_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (head == tail) {
        return false;
    }
    *out_event = g_audio_queue_by_cpu[cpu_index][head];
    __atomic_store_n(&g_audio_head_by_cpu[cpu_index], (head + 1) % kAudioQueueCapacity, __ATOMIC_RELAXED);
    return true;
}
}

bool audio_init(void) {
    __atomic_store_n(&g_audio_irq_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_audio_irq_drop_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_audio_ap_deferred_job_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_next_target_ap, 1, __ATOMIC_RELAXED);
    for (uint64_t i = 0; i < kMaxCpus; i++) {
        __atomic_store_n(&g_audio_head_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_audio_tail_by_cpu[i], 0, __ATOMIC_RELAXED);
    }
    log_info("audio: IRQ queue + AP deferred buffer processing ready.");
    return true;
}

void audio_on_irq(void) {
    __atomic_fetch_add(&g_audio_irq_total, 1, __ATOMIC_RELAXED);
    uint64_t target_cpu = 0;
    if (!pick_target_ap(&target_cpu) || !enqueue_audio_event(target_cpu, 1)) {
        __atomic_fetch_add(&g_audio_irq_drop_total, 1, __ATOMIC_RELAXED);
    }
}

uint64_t audio_inject_events(uint64_t count) {
    uint64_t injected = 0;
    for (uint64_t i = 0; i < count; i++) {
        __atomic_fetch_add(&g_audio_irq_total, 1, __ATOMIC_RELAXED);
        uint64_t target_cpu = 0;
        if (!pick_target_ap(&target_cpu) || !enqueue_audio_event(target_cpu, 2)) {
            __atomic_fetch_add(&g_audio_irq_drop_total, 1, __ATOMIC_RELAXED);
            continue;
        }
        injected++;
    }
    return injected;
}

bool audio_poll_ap_buffer_trigger(uint64_t cpu_index, uint64_t tick) {
    if (cpu_index == 0 || cpu_index >= kMaxCpus) {
        return false;
    }
    const uint64_t stride = __atomic_load_n(&g_poll_stride_ticks, __ATOMIC_RELAXED);
    if (stride == 0 || (tick % stride) != 0) {
        return false;
    }
    const uint64_t trigger = __atomic_load_n(&g_queue_trigger, __ATOMIC_RELAXED);
    if (trigger == 0 || queue_depth_for_cpu(cpu_index) < trigger) {
        return false;
    }
    return scheduler_enqueue_ap_maintenance_job(cpu_index);
}

bool audio_set_ap_buffer_trigger_config(uint64_t poll_stride_ticks, uint64_t queue_trigger) {
    if (poll_stride_ticks == 0 || queue_trigger == 0) {
        return false;
    }
    __atomic_store_n(&g_poll_stride_ticks, poll_stride_ticks, __ATOMIC_RELAXED);
    __atomic_store_n(&g_queue_trigger, queue_trigger, __ATOMIC_RELAXED);
    return true;
}

void audio_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence) {
    (void)sequence;
    audio_event event = {};
    if (!dequeue_audio_event(cpu_index, &event)) {
        return;
    }
    (void)event.source;
    __atomic_fetch_add(&g_audio_ap_deferred_job_total, 1, __ATOMIC_RELAXED);
}

uint64_t audio_irq_total(void) {
    return __atomic_load_n(&g_audio_irq_total, __ATOMIC_RELAXED);
}

uint64_t audio_irq_drop_total(void) {
    return __atomic_load_n(&g_audio_irq_drop_total, __ATOMIC_RELAXED);
}

uint64_t audio_ap_deferred_job_total(void) {
    return __atomic_load_n(&g_audio_ap_deferred_job_total, __ATOMIC_RELAXED);
}

uint64_t audio_queue_depth_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return static_cast<uint64_t>(queue_depth_for_cpu(cpu_index));
}
