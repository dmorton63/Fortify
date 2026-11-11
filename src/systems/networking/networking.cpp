#include "fortify/systems/logging/log.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

namespace {
constexpr uint64_t kMaxCpus = 64;
#if defined(FORTIFY_AP_STRESS)
constexpr uint32_t kRxQueueCapacity = 128;
#else
constexpr uint32_t kRxQueueCapacity = 32;
#endif
constexpr uint64_t kRxBatchPerApJobMin = 8;
constexpr uint64_t kRxBatchPerApJobMax = 24;
constexpr uint64_t kWebHeadBatchPerApJobMax = 2;
constexpr uint64_t kWebHeadRequestCap = 8;
constexpr uint64_t kWebHeadRequestStateFree = 0;
constexpr uint64_t kWebHeadRequestStatePending = 1;
constexpr uint64_t kWebHeadRequestStateRunning = 2;
constexpr uint64_t kWebHeadRequestStateDoneOk = 3;
constexpr uint64_t kWebHeadRequestStateDoneErr = 4;
constexpr uint64_t kWebHeadRequestStateClaimed = 5;
volatile uint64_t g_cleanup_poll_stride_ticks = 5;
volatile uint64_t g_cleanup_backlog_trigger = 3;
volatile uint64_t g_ap_deferred_job_total = 0;
volatile uint64_t g_ap_deferred_job_by_cpu[kMaxCpus] = {};
volatile uint64_t g_rx_packets_processed_total = 0;
volatile uint64_t g_rx_packets_processed_irq_total = 0;
volatile uint64_t g_rx_packets_processed_injected_total = 0;
volatile uint64_t g_rx_batches_total = 0;
volatile uint64_t g_rx_packets_processed_by_cpu[kMaxCpus] = {};
volatile uint64_t g_rx_batches_by_cpu[kMaxCpus] = {};
volatile uint64_t g_rx_irq_total = 0;
volatile uint64_t g_rx_irq_drop_total = 0;
volatile uint64_t g_next_target_ap = 1;
volatile uint64_t g_web_head_request_next_id = 0;
volatile uint64_t g_web_head_request_submit_total = 0;
volatile uint64_t g_web_head_request_done_total = 0;
volatile uint64_t g_web_head_request_done_ok_total = 0;
volatile uint64_t g_web_head_request_done_err_total = 0;
volatile uint32_t g_rx_head_by_cpu[kMaxCpus] = {};
volatile uint32_t g_rx_tail_by_cpu[kMaxCpus] = {};
volatile uint32_t g_rx_queue_high_water_by_cpu[kMaxCpus] = {};

struct rx_event {
    uint8_t source;
    uint16_t length;
};

struct web_head_request {
    volatile uint64_t state;
    uint64_t request_id;
    uint64_t owner_cpu;
    char target[64];
    uint64_t max_bytes;
    uint64_t bytes;
    uint64_t checksum;
};

rx_event g_rx_queue_by_cpu[kMaxCpus][kRxQueueCapacity] = {};
web_head_request g_web_head_requests[kWebHeadRequestCap] = {};

uint64_t bounded_text_length(const char *text, uint64_t max_length) {
    if (text == 0 || max_length == 0) {
        return 0;
    }
    uint64_t length = 0;
    while (length < max_length && text[length] != '\0') {
        length++;
    }
    return length;
}

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
    const uint32_t head = __atomic_load_n(&g_rx_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_rx_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (tail >= head) {
        return tail - head;
    }
    return (kRxQueueCapacity - head) + tail;
}

bool enqueue_rx_event(uint64_t cpu_index, uint8_t source, uint16_t length) {
    if (cpu_index >= kMaxCpus) {
        return false;
    }
    const uint32_t tail = __atomic_load_n(&g_rx_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t next_tail = (tail + 1) % kRxQueueCapacity;
    const uint32_t head = __atomic_load_n(&g_rx_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (next_tail == head) {
        return false;
    }
    g_rx_queue_by_cpu[cpu_index][tail].source = source;
    g_rx_queue_by_cpu[cpu_index][tail].length = length;
    __atomic_store_n(&g_rx_tail_by_cpu[cpu_index], next_tail, __ATOMIC_RELAXED);
    const uint32_t depth_after_enqueue = queue_depth_for_cpu(cpu_index);
    const uint32_t high_water = __atomic_load_n(&g_rx_queue_high_water_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (depth_after_enqueue > high_water) {
        __atomic_store_n(&g_rx_queue_high_water_by_cpu[cpu_index], depth_after_enqueue, __ATOMIC_RELAXED);
    }
    return true;
}

bool dequeue_rx_event(uint64_t cpu_index, rx_event *out_event) {
    if (cpu_index >= kMaxCpus || out_event == 0) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_rx_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_rx_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (head == tail) {
        return false;
    }
    *out_event = g_rx_queue_by_cpu[cpu_index][head];
    __atomic_store_n(&g_rx_head_by_cpu[cpu_index], (head + 1) % kRxQueueCapacity, __ATOMIC_RELAXED);
    return true;
}

bool copy_target_text(char *out_target, uint64_t out_size, const char *target) {
    if (out_target == 0 || out_size < 2 || target == 0 || target[0] == '\0') {
        return false;
    }
    uint64_t i = 0;
    for (; i + 1 < out_size && target[i] != '\0'; i++) {
        out_target[i] = target[i];
    }
    out_target[i] = '\0';
    return target[i] == '\0';
}

uint64_t checksum_text_head(const char *text, uint64_t max_bytes, uint64_t *out_bytes) {
    if (text == 0 || out_bytes == 0 || max_bytes == 0) {
        return 0;
    }
    const uint64_t bytes = bounded_text_length(text, max_bytes);
    uint64_t checksum = 0;
    for (uint64_t i = 0; i < bytes; i++) {
        checksum = (checksum * 131ULL) + static_cast<uint64_t>(static_cast<uint8_t>(text[i]));
    }
    *out_bytes = bytes;
    return checksum;
}

bool process_web_head_request_for_cpu(uint64_t cpu_index) {
    for (uint64_t i = 0; i < kWebHeadRequestCap; i++) {
        if (__atomic_load_n(&g_web_head_requests[i].owner_cpu, __ATOMIC_RELAXED) != cpu_index) {
            continue;
        }
        uint64_t expected_pending = kWebHeadRequestStatePending;
        if (!__atomic_compare_exchange_n(&g_web_head_requests[i].state,
                                         &expected_pending,
                                         kWebHeadRequestStateRunning,
                                         false,
                                         __ATOMIC_ACQ_REL,
                                         __ATOMIC_RELAXED)) {
            continue;
        }
        uint64_t bytes = 0;
        const uint64_t checksum =
            checksum_text_head(g_web_head_requests[i].target, g_web_head_requests[i].max_bytes, &bytes);
        __atomic_store_n(&g_web_head_requests[i].bytes, bytes, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].checksum, checksum, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].state, kWebHeadRequestStateDoneOk, __ATOMIC_RELEASE);
        __atomic_fetch_add(&g_web_head_request_done_total, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_web_head_request_done_ok_total, 1, __ATOMIC_RELAXED);
        return true;
    }
    return false;
}
}

bool networking_init(void) {
    __atomic_store_n(&g_ap_deferred_job_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_rx_irq_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_rx_irq_drop_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_next_target_ap, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&g_rx_packets_processed_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_rx_packets_processed_irq_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_rx_packets_processed_injected_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_rx_batches_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_web_head_request_next_id, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_web_head_request_submit_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_web_head_request_done_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_web_head_request_done_ok_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_web_head_request_done_err_total, 0, __ATOMIC_RELAXED);
    for (uint64_t i = 0; i < kMaxCpus; i++) {
        __atomic_store_n(&g_ap_deferred_job_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_rx_packets_processed_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_rx_batches_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_rx_head_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_rx_tail_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_rx_queue_high_water_by_cpu[i], 0, __ATOMIC_RELAXED);
    }
    for (uint64_t i = 0; i < kWebHeadRequestCap; i++) {
        __atomic_store_n(&g_web_head_requests[i].state, kWebHeadRequestStateFree, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].request_id, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].owner_cpu, 0, __ATOMIC_RELAXED);
        g_web_head_requests[i].target[0] = '\0';
        __atomic_store_n(&g_web_head_requests[i].max_bytes, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].bytes, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].checksum, 0, __ATOMIC_RELAXED);
    }
    log_info("networking: RX IRQ queue + AP deferred processing ready.");
    return true;
}

bool webaccess_init(void) {
    log_info("webaccess: AP deferred head-request queue ready.");
    return true;
}

bool webaccess_head_request_submit(const char *target, uint64_t max_bytes, uint64_t *out_request_id) {
    if (target == 0 || target[0] == '\0' || out_request_id == 0 || max_bytes == 0 || max_bytes > 4096) {
        return false;
    }
    uint64_t owner_cpu = 0;
    if (!pick_target_ap(&owner_cpu)) {
        return false;
    }
    for (uint64_t i = 0; i < kWebHeadRequestCap; i++) {
        uint64_t expected_free = kWebHeadRequestStateFree;
        uint64_t expected_done_ok = kWebHeadRequestStateDoneOk;
        uint64_t expected_done_err = kWebHeadRequestStateDoneErr;
        bool claimed = __atomic_compare_exchange_n(&g_web_head_requests[i].state,
                                                   &expected_free,
                                                   kWebHeadRequestStateClaimed,
                                                   false,
                                                   __ATOMIC_ACQ_REL,
                                                   __ATOMIC_RELAXED) ||
                       __atomic_compare_exchange_n(&g_web_head_requests[i].state,
                                                   &expected_done_ok,
                                                   kWebHeadRequestStateClaimed,
                                                   false,
                                                   __ATOMIC_ACQ_REL,
                                                   __ATOMIC_RELAXED) ||
                       __atomic_compare_exchange_n(&g_web_head_requests[i].state,
                                                   &expected_done_err,
                                                   kWebHeadRequestStateClaimed,
                                                   false,
                                                   __ATOMIC_ACQ_REL,
                                                   __ATOMIC_RELAXED);
        if (!claimed) {
            continue;
        }
        if (!copy_target_text(g_web_head_requests[i].target, sizeof(g_web_head_requests[i].target), target)) {
            __atomic_store_n(&g_web_head_requests[i].request_id, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&g_web_head_requests[i].owner_cpu, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&g_web_head_requests[i].max_bytes, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&g_web_head_requests[i].bytes, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&g_web_head_requests[i].checksum, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&g_web_head_requests[i].state, kWebHeadRequestStateFree, __ATOMIC_RELEASE);
            return false;
        }
        const uint64_t request_id =
            __atomic_add_fetch(&g_web_head_request_next_id, 1, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].request_id, request_id, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].owner_cpu, owner_cpu, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].max_bytes, max_bytes, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].bytes, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].checksum, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_web_head_requests[i].state, kWebHeadRequestStatePending, __ATOMIC_RELEASE);
        __atomic_fetch_add(&g_web_head_request_submit_total, 1, __ATOMIC_RELAXED);
        *out_request_id = request_id;
        (void)networking_request_ap_buffer_cleanup(owner_cpu);
        return true;
    }
    return false;
}

bool webaccess_head_request_status(uint64_t request_id,
                                   uint64_t *out_state,
                                   uint64_t *out_bytes,
                                   uint64_t *out_checksum) {
    if (request_id == 0 || out_state == 0 || out_bytes == 0 || out_checksum == 0) {
        return false;
    }
    for (uint64_t i = 0; i < kWebHeadRequestCap; i++) {
        if (__atomic_load_n(&g_web_head_requests[i].request_id, __ATOMIC_RELAXED) != request_id) {
            continue;
        }
        *out_state = __atomic_load_n(&g_web_head_requests[i].state, __ATOMIC_ACQUIRE);
        *out_bytes = __atomic_load_n(&g_web_head_requests[i].bytes, __ATOMIC_RELAXED);
        *out_checksum = __atomic_load_n(&g_web_head_requests[i].checksum, __ATOMIC_RELAXED);
        return true;
    }
    return false;
}

uint64_t webaccess_head_request_submit_total(void) {
    return __atomic_load_n(&g_web_head_request_submit_total, __ATOMIC_RELAXED);
}

uint64_t webaccess_head_request_done_total(void) {
    return __atomic_load_n(&g_web_head_request_done_total, __ATOMIC_RELAXED);
}

uint64_t webaccess_head_request_done_ok_total(void) {
    return __atomic_load_n(&g_web_head_request_done_ok_total, __ATOMIC_RELAXED);
}

uint64_t webaccess_head_request_done_err_total(void) {
    return __atomic_load_n(&g_web_head_request_done_err_total, __ATOMIC_RELAXED);
}

void networking_on_rx_irq(void) {
    __atomic_fetch_add(&g_rx_irq_total, 1, __ATOMIC_RELAXED);
    uint64_t target_cpu = 0;
    if (!pick_target_ap(&target_cpu) || !enqueue_rx_event(target_cpu, 1, 1500)) {
        __atomic_fetch_add(&g_rx_irq_drop_total, 1, __ATOMIC_RELAXED);
    }
}

uint64_t networking_inject_rx_events(uint64_t count) {
    uint64_t injected = 0;
    for (uint64_t i = 0; i < count; i++) {
        __atomic_fetch_add(&g_rx_irq_total, 1, __ATOMIC_RELAXED);
        uint64_t target_cpu = 0;
        if (!pick_target_ap(&target_cpu) || !enqueue_rx_event(target_cpu, 2, 1500)) {
            __atomic_fetch_add(&g_rx_irq_drop_total, 1, __ATOMIC_RELAXED);
            continue;
        }
        injected++;
    }
    return injected;
}

bool networking_request_ap_buffer_cleanup(uint64_t cpu_index) {
    return scheduler_enqueue_ap_networking_job(cpu_index);
}

bool networking_poll_ap_cleanup_trigger(uint64_t cpu_index, uint64_t tick) {
    if (cpu_index >= kMaxCpus) {
        return false;
    }
    const uint64_t stride = __atomic_load_n(&g_cleanup_poll_stride_ticks, __ATOMIC_RELAXED);
    if (stride == 0 || (tick % stride) != 0) {
        return false;
    }
    const uint64_t trigger = __atomic_load_n(&g_cleanup_backlog_trigger, __ATOMIC_RELAXED);
    if (trigger == 0 || queue_depth_for_cpu(cpu_index) < trigger) {
        return false;
    }
    return networking_request_ap_buffer_cleanup(cpu_index);
}

bool networking_set_ap_cleanup_trigger_config(uint64_t poll_stride_ticks, uint64_t backlog_trigger) {
    if (poll_stride_ticks == 0 || backlog_trigger == 0) {
        return false;
    }
    __atomic_store_n(&g_cleanup_poll_stride_ticks, poll_stride_ticks, __ATOMIC_RELAXED);
    __atomic_store_n(&g_cleanup_backlog_trigger, backlog_trigger, __ATOMIC_RELAXED);
    return true;
}

void networking_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence) {
    (void)sequence;
    if (cpu_index >= kMaxCpus) {
        return;
    }
    const uint64_t depth = static_cast<uint64_t>(queue_depth_for_cpu(cpu_index));
    uint64_t target_batch = kRxBatchPerApJobMin + (depth / 2);
    if (target_batch > kRxBatchPerApJobMax) {
        target_batch = kRxBatchPerApJobMax;
    }
    uint64_t processed = 0;
    uint64_t processed_irq = 0;
    uint64_t processed_injected = 0;
    for (uint64_t i = 0; i < target_batch; i++) {
        rx_event event = {};
        if (!dequeue_rx_event(cpu_index, &event)) {
            break;
        }
        if (event.source == 1) {
            processed_irq++;
        } else {
            processed_injected++;
        }
        (void)event.length;
        processed++;
    }
    bool web_processed = false;
    for (uint64_t i = 0; i < kWebHeadBatchPerApJobMax; i++) {
        if (!process_web_head_request_for_cpu(cpu_index)) {
            break;
        }
        web_processed = true;
    }

    if (processed == 0 && !web_processed) {
        return;
    }
    if (processed != 0) {
        __atomic_fetch_add(&g_rx_packets_processed_total, processed, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_rx_packets_processed_irq_total, processed_irq, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_rx_packets_processed_injected_total, processed_injected, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_rx_batches_total, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_rx_packets_processed_by_cpu[cpu_index], processed, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_rx_batches_by_cpu[cpu_index], 1, __ATOMIC_RELAXED);
    }
    __atomic_fetch_add(&g_ap_deferred_job_total, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_ap_deferred_job_by_cpu[cpu_index], 1, __ATOMIC_RELAXED);
}

uint64_t networking_ap_deferred_job_total(void) {
    return __atomic_load_n(&g_ap_deferred_job_total, __ATOMIC_RELAXED);
}

uint64_t networking_ap_deferred_job_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_deferred_job_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t networking_rx_irq_total(void) {
    return __atomic_load_n(&g_rx_irq_total, __ATOMIC_RELAXED);
}

uint64_t networking_rx_irq_drop_total(void) {
    return __atomic_load_n(&g_rx_irq_drop_total, __ATOMIC_RELAXED);
}

uint64_t networking_queue_depth_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return static_cast<uint64_t>(queue_depth_for_cpu(cpu_index));
}

uint64_t networking_queue_high_water_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return static_cast<uint64_t>(__atomic_load_n(&g_rx_queue_high_water_by_cpu[cpu_index], __ATOMIC_RELAXED));
}

uint64_t networking_queue_high_water_total(void) {
    uint64_t total = smp_total_cpu_count();
    if (total > kMaxCpus) {
        total = kMaxCpus;
    }
    uint64_t high_water_total = 0;
    for (uint64_t cpu = 1; cpu < total; cpu++) {
        high_water_total += static_cast<uint64_t>(__atomic_load_n(&g_rx_queue_high_water_by_cpu[cpu], __ATOMIC_RELAXED));
    }
    return high_water_total;
}

uint64_t networking_rx_packets_processed_total(void) {
    return __atomic_load_n(&g_rx_packets_processed_total, __ATOMIC_RELAXED);
}

uint64_t networking_rx_packets_processed_irq_total(void) {
    return __atomic_load_n(&g_rx_packets_processed_irq_total, __ATOMIC_RELAXED);
}

uint64_t networking_rx_packets_processed_injected_total(void) {
    return __atomic_load_n(&g_rx_packets_processed_injected_total, __ATOMIC_RELAXED);
}

uint64_t networking_rx_batches_total(void) {
    return __atomic_load_n(&g_rx_batches_total, __ATOMIC_RELAXED);
}

uint64_t networking_rx_packets_processed_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_rx_packets_processed_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t networking_rx_batches_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_rx_batches_by_cpu[cpu_index], __ATOMIC_RELAXED);
}
