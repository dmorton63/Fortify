#include "fortify/systems/kernel/runtime_message_router.h"

namespace {
constexpr uint64_t kRuntimeEventQueueCapacity = 256;
struct runtime_target_queue {
    runtime_message entries[kRuntimeEventQueueCapacity];
    uint64_t head;
    uint64_t tail;
    uint64_t drop_total;
    uint64_t high_water;
    uint64_t enqueued_total;
    uint64_t dequeued_total;
};

constexpr uint64_t kRuntimeMessageQueueTargetCount = 2;
constexpr uint64_t kInputQueueIndex = 0;
constexpr uint64_t kCommandQueueIndex = 1;

runtime_target_queue g_runtime_target_queues[kRuntimeMessageQueueTargetCount] = {};
uint64_t g_runtime_last_dequeue_queue = kCommandQueueIndex;
uint64_t g_runtime_event_dispatch_input_total = 0;
uint64_t g_runtime_event_dispatch_command_total = 0;
uint64_t g_runtime_event_route_drop_total = 0;
uint64_t g_runtime_event_route_drop_invalid_source_total = 0;
uint64_t g_runtime_event_route_drop_keyboard_suppressed_total = 0;
uint64_t g_runtime_event_route_drop_unroutable_total = 0;

uint64_t queue_depth_internal(const runtime_target_queue &queue) {
    if (queue.tail >= queue.head) {
        return queue.tail - queue.head;
    }
    return (kRuntimeEventQueueCapacity - queue.head) + queue.tail;
}

runtime_target_queue *queue_for_target(runtime_message_target target) {
    if (target == runtime_message_target::compositor_input) {
        return &g_runtime_target_queues[kInputQueueIndex];
    }
    return &g_runtime_target_queues[kCommandQueueIndex];
}

const runtime_target_queue *queue_for_target_const(runtime_message_target target) {
    if (target == runtime_message_target::compositor_input) {
        return &g_runtime_target_queues[kInputQueueIndex];
    }
    return &g_runtime_target_queues[kCommandQueueIndex];
}

bool queue_enqueue(runtime_target_queue *queue, const runtime_message &message) {
    if (queue == 0) {
        return false;
    }
    const uint64_t next_tail = (queue->tail + 1) % kRuntimeEventQueueCapacity;
    if (next_tail == queue->head) {
        queue->drop_total++;
        return false;
    }
    queue->entries[queue->tail] = message;
    queue->tail = next_tail;
    const uint64_t depth = queue_depth_internal(*queue);
    if (depth > queue->high_water) {
        queue->high_water = depth;
    }
    queue->enqueued_total++;
    return true;
}

bool queue_dequeue(runtime_target_queue *queue, runtime_message *out_message) {
    if (queue == 0 || out_message == 0 || queue->head == queue->tail) {
        return false;
    }
    *out_message = queue->entries[queue->head];
    queue->head = (queue->head + 1) % kRuntimeEventQueueCapacity;
    queue->dequeued_total++;
    return true;
}

bool queue_has_data(const runtime_target_queue &queue) {
    return queue.head != queue.tail;
}
} // namespace

bool runtime_message_router_enqueue(const runtime_message &message) {
    runtime_target_queue *queue = queue_for_target(message.target);
    return queue_enqueue(queue, message);
}

bool runtime_message_router_dequeue(runtime_message *out_message) {
    if (out_message == 0) {
        return false;
    }
    const bool input_has_data = queue_has_data(g_runtime_target_queues[kInputQueueIndex]);
    const bool command_has_data = queue_has_data(g_runtime_target_queues[kCommandQueueIndex]);
    if (!input_has_data && !command_has_data) {
        return false;
    }

    uint64_t preferred_queue = kInputQueueIndex;
    if (input_has_data && command_has_data) {
        preferred_queue =
            (g_runtime_last_dequeue_queue == kInputQueueIndex) ? kCommandQueueIndex : kInputQueueIndex;
    } else if (command_has_data) {
        preferred_queue = kCommandQueueIndex;
    }

    runtime_target_queue *preferred = &g_runtime_target_queues[preferred_queue];
    if (!queue_dequeue(preferred, out_message)) {
        const uint64_t fallback_queue =
            (preferred_queue == kInputQueueIndex) ? kCommandQueueIndex : kInputQueueIndex;
        runtime_target_queue *fallback = &g_runtime_target_queues[fallback_queue];
        if (!queue_dequeue(fallback, out_message)) {
            return false;
        }
        g_runtime_last_dequeue_queue = fallback_queue;
        return true;
    }

    g_runtime_last_dequeue_queue = preferred_queue;
    return true;
}

uint64_t runtime_message_router_queue_depth(void) {
    uint64_t depth = 0;
    for (uint64_t i = 0; i < kRuntimeMessageQueueTargetCount; i++) {
        depth += queue_depth_internal(g_runtime_target_queues[i]);
    }
    return depth;
}

uint64_t runtime_message_router_queue_high_water(void) {
    uint64_t high_water = 0;
    for (uint64_t i = 0; i < kRuntimeMessageQueueTargetCount; i++) {
        high_water += g_runtime_target_queues[i].high_water;
    }
    return high_water;
}

uint64_t runtime_message_router_drop_total(void) {
    uint64_t drops = 0;
    for (uint64_t i = 0; i < kRuntimeMessageQueueTargetCount; i++) {
        drops += g_runtime_target_queues[i].drop_total;
    }
    return drops;
}

uint64_t runtime_message_router_enqueued_total(void) {
    uint64_t enqueued = 0;
    for (uint64_t i = 0; i < kRuntimeMessageQueueTargetCount; i++) {
        enqueued += g_runtime_target_queues[i].enqueued_total;
    }
    return enqueued;
}

uint64_t runtime_message_router_dequeued_total(void) {
    uint64_t dequeued = 0;
    for (uint64_t i = 0; i < kRuntimeMessageQueueTargetCount; i++) {
        dequeued += g_runtime_target_queues[i].dequeued_total;
    }
    return dequeued;
}

uint64_t runtime_message_router_queue_depth_for_target(runtime_message_target target) {
    const runtime_target_queue *queue = queue_for_target_const(target);
    return queue_depth_internal(*queue);
}

uint64_t runtime_message_router_queue_high_water_for_target(runtime_message_target target) {
    const runtime_target_queue *queue = queue_for_target_const(target);
    return queue->high_water;
}

uint64_t runtime_message_router_drop_total_for_target(runtime_message_target target) {
    const runtime_target_queue *queue = queue_for_target_const(target);
    return queue->drop_total;
}

uint64_t runtime_message_router_enqueued_total_for_target(runtime_message_target target) {
    const runtime_target_queue *queue = queue_for_target_const(target);
    return queue->enqueued_total;
}

uint64_t runtime_message_router_dequeued_total_for_target(runtime_message_target target) {
    const runtime_target_queue *queue = queue_for_target_const(target);
    return queue->dequeued_total;
}

uint64_t runtime_message_router_dispatch_input_total(void) {
    return g_runtime_event_dispatch_input_total;
}

uint64_t runtime_message_router_dispatch_command_total(void) {
    return g_runtime_event_dispatch_command_total;
}

uint64_t runtime_message_router_route_drop_total(void) {
    return g_runtime_event_route_drop_total;
}

uint64_t runtime_message_router_route_drop_invalid_source_total(void) {
    return g_runtime_event_route_drop_invalid_source_total;
}

uint64_t runtime_message_router_route_drop_keyboard_suppressed_total(void) {
    return g_runtime_event_route_drop_keyboard_suppressed_total;
}

uint64_t runtime_message_router_route_drop_unroutable_total(void) {
    return g_runtime_event_route_drop_unroutable_total;
}

bool runtime_message_router_probe_route_drop(runtime_route_drop_probe probe) {
    runtime_message message = {};
    bool suppress_keyboard = false;
    bool keyboard_events_enabled = true;

    switch (probe) {
        case runtime_route_drop_probe::invalid_source:
            message.category = runtime_message_category::input;
            message.target = runtime_message_target::compositor_input;
            message.target_id = runtime_message_target_id::compositor_primary;
            message.input_event.source = static_cast<runtime_input_source>(0xff);
            break;
        case runtime_route_drop_probe::keyboard_suppressed:
            message.category = runtime_message_category::input;
            message.target = runtime_message_target::compositor_input;
            message.target_id = runtime_message_target_id::compositor_primary;
            message.input_event.source = runtime_input_source::keyboard;
            suppress_keyboard = true;
            break;
        case runtime_route_drop_probe::unroutable:
            message.category = runtime_message_category::command;
            message.target = runtime_message_target::compositor_input;
            message.target_id = runtime_message_target_id::compositor_primary;
            break;
        default:
            return false;
    }

    return runtime_message_router_route(message, suppress_keyboard, keyboard_events_enabled) ==
           runtime_route_decision::drop;
}

runtime_route_decision runtime_message_router_route(const runtime_message &message,
                                                    bool suppress_keyboard,
                                                    bool keyboard_events_enabled) {
    if (message.category == runtime_message_category::input &&
        message.target == runtime_message_target::compositor_input &&
        message.target_id == runtime_message_target_id::compositor_primary) {
        if (message.input_event.source != runtime_input_source::serial &&
            message.input_event.source != runtime_input_source::keyboard) {
            g_runtime_event_route_drop_total++;
            g_runtime_event_route_drop_invalid_source_total++;
            return runtime_route_decision::drop;
        }
        if (message.input_event.source == runtime_input_source::keyboard &&
            (suppress_keyboard || !keyboard_events_enabled)) {
            g_runtime_event_route_drop_total++;
            g_runtime_event_route_drop_keyboard_suppressed_total++;
            return runtime_route_decision::drop;
        }
        g_runtime_event_dispatch_input_total++;
        return runtime_route_decision::dispatch_input;
    }

    if (message.category == runtime_message_category::command &&
        message.target == runtime_message_target::runtime_command &&
        message.target_id == runtime_message_target_id::runtime_command_primary) {
        g_runtime_event_dispatch_command_total++;
        return runtime_route_decision::dispatch_command;
    }

    g_runtime_event_route_drop_total++;
    g_runtime_event_route_drop_unroutable_total++;
    return runtime_route_decision::drop;
}
