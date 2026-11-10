#ifndef FORTIFY_KERNEL_RUNTIME_MESSAGE_ROUTER_H
#define FORTIFY_KERNEL_RUNTIME_MESSAGE_ROUTER_H

#include <stdint.h>

#include "fortify/systems/subsystems/runtime_messages.h"

enum class runtime_route_decision : uint8_t {
    drop = 0,
    dispatch_input = 1,
    dispatch_command = 2,
};

enum class runtime_route_drop_probe : uint8_t {
    invalid_source = 0,
    keyboard_suppressed = 1,
    unroutable = 2,
};

bool runtime_message_router_enqueue(const runtime_message &message);
bool runtime_message_router_dequeue(runtime_message *out_message);
uint64_t runtime_message_router_queue_depth(void);
uint64_t runtime_message_router_queue_high_water(void);
uint64_t runtime_message_router_drop_total(void);
uint64_t runtime_message_router_enqueued_total(void);
uint64_t runtime_message_router_dequeued_total(void);
uint64_t runtime_message_router_queue_depth_for_target(runtime_message_target target);
uint64_t runtime_message_router_queue_high_water_for_target(runtime_message_target target);
uint64_t runtime_message_router_drop_total_for_target(runtime_message_target target);
uint64_t runtime_message_router_enqueued_total_for_target(runtime_message_target target);
uint64_t runtime_message_router_dequeued_total_for_target(runtime_message_target target);
uint64_t runtime_message_router_dispatch_input_total(void);
uint64_t runtime_message_router_dispatch_command_total(void);
uint64_t runtime_message_router_route_drop_total(void);
uint64_t runtime_message_router_route_drop_invalid_source_total(void);
uint64_t runtime_message_router_route_drop_keyboard_suppressed_total(void);
uint64_t runtime_message_router_route_drop_unroutable_total(void);
bool runtime_message_router_probe_route_drop(runtime_route_drop_probe probe);
runtime_route_decision runtime_message_router_route(const runtime_message &message,
                                                    bool suppress_keyboard,
                                                    bool keyboard_events_enabled);

#endif
