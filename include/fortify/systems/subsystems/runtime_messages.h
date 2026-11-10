#ifndef FORTIFY_SUBSYSTEMS_RUNTIME_MESSAGES_H
#define FORTIFY_SUBSYSTEMS_RUNTIME_MESSAGES_H

#include <stdint.h>

#include "fortify/systems/subsystems/subsystems.h"

enum class runtime_input_source : uint8_t {
    serial = 0,
    keyboard = 1,
};

struct runtime_input_event {
    input_message message;
    runtime_input_source source;
};

enum class runtime_message_category : uint8_t {
    input = 0,
    command = 1,
};

enum class runtime_message_target : uint8_t {
    compositor_input = 0,
    runtime_command = 1,
};

enum class runtime_message_target_id : uint16_t {
    compositor_primary = 1,
    runtime_command_primary = 2,
};

struct runtime_message {
    runtime_message_category category;
    runtime_message_target target;
    runtime_message_target_id target_id;
    runtime_input_event input_event;
    char command_text[64];
};

#endif
