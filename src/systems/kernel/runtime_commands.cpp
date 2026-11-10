#include "fortify/systems/kernel/runtime_commands.h"

#include "fortify/systems/kernel/ap_trigger_profile.h"
#include "fortify/systems/kernel/kernel_string.h"
#include "fortify/systems/kernel/runtime_command_ap_stats.h"
#include "fortify/systems/kernel/runtime_command_filesystem_surface.h"
#include "fortify/systems/kernel/runtime_command_smp_control.h"
#include "fortify/systems/kernel/runtime_command_video_input.h"
#include "fortify/systems/kernel/runtime_message_router.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

extern "C" volatile uint64_t g_ap_trigger_profile;

namespace {
static bool g_runtime_keyboard_events_to_commands = true;
static uint64_t g_runtime_router_dequeue_pause_until_tick = 0;

static void log_ap_trigger_command_help(void) {
    log_info("smp: serial cmd: ap-trigger [0|1|2], ap-trigger, ap-stats, ap-task-start, ap-task-stop, ap-task-migrate [0|1], video-demo [0|1], video-style [0|1|2], video-speed <1..32>, video-font [0|1|2], video-text-scale <1..4>, input-kbd [0|1], net-rx-inject <count>, web-head <target> [max-bytes], web-head-status <request-id>, usb-inject <count>, storage-inject <count>, audio-inject <count>, webcam-inject <count>, inject-all <count>, fs-root-ls [source], fs-root-head <name.EXT> [source], fs-9p-probe, fs-9p-head <path> [max-bytes], fs-9p-head-status <request-id>, surface-create <type> <title>, surface-ls, rmsg-drop-test <src|kbd|unroute>, rmsg-selftest, rmsg-backlog-test <count> [input|cmd|both], help");
}

static void log_ap_stats_snapshot(void) {
    kernel_log_ap_stats_snapshot();
}

static void handle_serial_runtime_command(const char *command) {
    if (command == 0 || command[0] == '\0') {
        return;
    }
    if (kernel_string_equals(command, "help")) {
        log_ap_trigger_command_help();
        return;
    }
    if (kernel_string_equals(command, "ap-trigger")) {
        char profile_hex[19];
        const uint64_t profile = __atomic_load_n(&g_ap_trigger_profile, __ATOMIC_RELAXED);
        kernel_format_hex64(profile_hex, profile);
        serial_write("[INFO] smp: serial cmd ap-trigger current=");
        serial_write(profile_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd ap-trigger current=");
        console_write(profile_hex);
        console_write("\n");
        return;
    }
    if (kernel_string_equals(command, "ap-stats")) {
        log_ap_stats_snapshot();
        return;
    }
    if (kernel_string_equals(command, "rmsg-drop-test src")) {
        if (runtime_message_router_probe_route_drop(runtime_route_drop_probe::invalid_source)) {
            serial_write("[INFO] smp: serial cmd rmsg-drop-test injected=src\n");
            console_write("[INFO] smp: serial cmd rmsg-drop-test injected=src\n");
        } else {
            serial_write("[WARN] smp: serial cmd rmsg-drop-test failed=src\n");
            console_write("[WARN] smp: serial cmd rmsg-drop-test failed=src\n");
        }
        return;
    }
    if (kernel_string_equals(command, "rmsg-drop-test kbd")) {
        if (runtime_message_router_probe_route_drop(runtime_route_drop_probe::keyboard_suppressed)) {
            serial_write("[INFO] smp: serial cmd rmsg-drop-test injected=kbd\n");
            console_write("[INFO] smp: serial cmd rmsg-drop-test injected=kbd\n");
        } else {
            serial_write("[WARN] smp: serial cmd rmsg-drop-test failed=kbd\n");
            console_write("[WARN] smp: serial cmd rmsg-drop-test failed=kbd\n");
        }
        return;
    }
    if (kernel_string_equals(command, "rmsg-drop-test unroute")) {
        if (runtime_message_router_probe_route_drop(runtime_route_drop_probe::unroutable)) {
            serial_write("[INFO] smp: serial cmd rmsg-drop-test injected=unroute\n");
            console_write("[INFO] smp: serial cmd rmsg-drop-test injected=unroute\n");
        } else {
            serial_write("[WARN] smp: serial cmd rmsg-drop-test failed=unroute\n");
            console_write("[WARN] smp: serial cmd rmsg-drop-test failed=unroute\n");
        }
        return;
    }
    if (kernel_string_equals(command, "rmsg-selftest")) {
        const uint64_t before_src = runtime_message_router_route_drop_invalid_source_total();
        const uint64_t before_kbd = runtime_message_router_route_drop_keyboard_suppressed_total();
        const uint64_t before_unroute = runtime_message_router_route_drop_unroutable_total();
        const uint64_t before_total = runtime_message_router_route_drop_total();
        const bool probe_src = runtime_message_router_probe_route_drop(runtime_route_drop_probe::invalid_source);
        const bool probe_kbd = runtime_message_router_probe_route_drop(runtime_route_drop_probe::keyboard_suppressed);
        const bool probe_unroute = runtime_message_router_probe_route_drop(runtime_route_drop_probe::unroutable);
        const uint64_t delta_src = runtime_message_router_route_drop_invalid_source_total() - before_src;
        const uint64_t delta_kbd = runtime_message_router_route_drop_keyboard_suppressed_total() - before_kbd;
        const uint64_t delta_unroute = runtime_message_router_route_drop_unroutable_total() - before_unroute;
        const uint64_t delta_total = runtime_message_router_route_drop_total() - before_total;
        const bool pass = probe_src && probe_kbd && probe_unroute && delta_src == 1 && delta_kbd == 1 &&
                          delta_unroute == 1 && delta_total == 3;
        char delta_src_hex[19];
        char delta_kbd_hex[19];
        char delta_unroute_hex[19];
        char delta_total_hex[19];
        kernel_format_hex64(delta_src_hex, delta_src);
        kernel_format_hex64(delta_kbd_hex, delta_kbd);
        kernel_format_hex64(delta_unroute_hex, delta_unroute);
        kernel_format_hex64(delta_total_hex, delta_total);
        serial_write(pass ? "[INFO] smp: serial cmd rmsg-selftest pass delta(src/kbd/unroute/total)="
                          : "[WARN] smp: serial cmd rmsg-selftest fail delta(src/kbd/unroute/total)=");
        serial_write(delta_src_hex);
        serial_write("/");
        serial_write(delta_kbd_hex);
        serial_write("/");
        serial_write(delta_unroute_hex);
        serial_write("/");
        serial_write(delta_total_hex);
        serial_write("\n");
        console_write(pass ? "[INFO] smp: serial cmd rmsg-selftest pass delta(src/kbd/unroute/total)="
                           : "[WARN] smp: serial cmd rmsg-selftest fail delta(src/kbd/unroute/total)=");
        console_write(delta_src_hex);
        console_write("/");
        console_write(delta_kbd_hex);
        console_write("/");
        console_write(delta_unroute_hex);
        console_write("/");
        console_write(delta_total_hex);
        console_write("\n");
        return;
    }
    if (kernel_string_starts_with(command, "rmsg-backlog-test ")) {
        char count_token[16];
        char mode_token[8];
        uint64_t cursor = 18;
        if (!kernel_parse_token(command, cursor, count_token, sizeof(count_token), &cursor)) {
            serial_write("[WARN] smp: serial cmd rmsg-backlog-test expects <count> [input|cmd|both]\n");
            console_write("[WARN] smp: serial cmd rmsg-backlog-test expects <count> [input|cmd|both]\n");
            return;
        }
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_token, &count) || count == 0 || count > 1024) {
            serial_write("[WARN] smp: serial cmd rmsg-backlog-test count expects 1..1024\n");
            console_write("[WARN] smp: serial cmd rmsg-backlog-test count expects 1..1024\n");
            return;
        }
        bool inject_input = true;
        bool inject_command = true;
        if (kernel_parse_token(command, cursor, mode_token, sizeof(mode_token), &cursor)) {
            if (kernel_string_equals(mode_token, "input")) {
                inject_command = false;
            } else if (kernel_string_equals(mode_token, "cmd")) {
                inject_input = false;
            } else if (!kernel_string_equals(mode_token, "both")) {
                serial_write("[WARN] smp: serial cmd rmsg-backlog-test mode expects input|cmd|both\n");
                console_write("[WARN] smp: serial cmd rmsg-backlog-test mode expects input|cmd|both\n");
                return;
            }
            char extra_token[4];
            if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                serial_write("[WARN] smp: serial cmd rmsg-backlog-test accepts only <count> [input|cmd|both]\n");
                console_write("[WARN] smp: serial cmd rmsg-backlog-test accepts only <count> [input|cmd|both]\n");
                return;
            }
        }

        uint64_t input_enqueued = 0;
        uint64_t command_enqueued = 0;
        for (uint64_t i = 0; i < count; i++) {
            if (inject_input) {
                runtime_message input_msg = {};
                input_msg.category = runtime_message_category::input;
                input_msg.target = runtime_message_target::compositor_input;
                input_msg.target_id = runtime_message_target_id::compositor_primary;
                input_msg.input_event.source = runtime_input_source::serial;
                input_msg.input_event.message.type = input_message_type::key_char;
                input_msg.input_event.message.value = 'a';
                input_msg.input_event.message.source = input_message_source::keyboard_irq;
                input_msg.input_event.message.tick = timer_ticks();
                input_msg.input_event.message.sequence = i;
                if (runtime_message_router_enqueue(input_msg)) {
                    input_enqueued++;
                }
            }
            if (inject_command) {
                runtime_message command_msg = {};
                command_msg.category = runtime_message_category::command;
                command_msg.target = runtime_message_target::runtime_command;
                command_msg.target_id = runtime_message_target_id::runtime_command_primary;
                if (runtime_message_router_enqueue(command_msg)) {
                    command_enqueued++;
                }
            }
        }

        g_runtime_router_dequeue_pause_until_tick = timer_ticks() + 300;
        char requested_hex[19];
        char input_enqueued_hex[19];
        char command_enqueued_hex[19];
        kernel_format_hex64(requested_hex, count);
        kernel_format_hex64(input_enqueued_hex, input_enqueued);
        kernel_format_hex64(command_enqueued_hex, command_enqueued);
        serial_write("[INFO] smp: serial cmd rmsg-backlog-test requested=");
        serial_write(requested_hex);
        serial_write(" enqueued(input/cmd)=");
        serial_write(input_enqueued_hex);
        serial_write("/");
        serial_write(command_enqueued_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd rmsg-backlog-test requested=");
        console_write(requested_hex);
        console_write(" enqueued(input/cmd)=");
        console_write(input_enqueued_hex);
        console_write("/");
        console_write(command_enqueued_hex);
        console_write("\n");
        return;
    }
    if (kernel_handle_video_input_runtime_command(command, &g_runtime_keyboard_events_to_commands)) {
        return;
    }
    if (kernel_handle_filesystem_surface_runtime_command(command)) {
        return;
    }
    if (kernel_handle_smp_control_runtime_command(command)) {
        return;
    }
    serial_write("[WARN] smp: serial cmd unknown='");
    serial_write(command);
    serial_write("'. type 'help'\n");
    console_write("[WARN] smp: serial cmd unknown='");
    console_write(command);
    console_write("'. type 'help'\n");
}

void kernel_service_runtime_commands_impl(void) {
    constexpr uint64_t kMaxCommandLength = 63;
    static constexpr char kCubeEasterSequence[] = "fortify-cube";
    static constexpr uint64_t kCubeEasterDoubleWindowTicks = 300;
    static constexpr uint64_t kCubeEasterPulseTicks = 600;
    static constexpr uint64_t kCubeEasterGuardTicks = 120;
    static constexpr uint64_t kSerialKeyboardArbitrationTicks = 30;
    static uint64_t command_length = 0;
    static uint64_t last_easter_match_tick = 0;
    static uint64_t easter_guard_until_tick = 0;
    static uint64_t last_serial_input_tick = 0;
    bool suppress_keyboard = false;
    auto copy_command_text = [](char *out_text, uint64_t out_size, const char *input) {
        if (out_text == 0 || out_size < 2 || input == 0 || input[0] == '\0') {
            return false;
        }
        uint64_t i = 0;
        for (; i + 1 < out_size && input[i] != '\0'; i++) {
            out_text[i] = input[i];
        }
        out_text[i] = '\0';
        return input[i] == '\0';
    };
    auto enqueue_runtime_command_message = [&](const char *command_text) {
        runtime_message message = {};
        message.category = runtime_message_category::command;
        message.target = runtime_message_target::runtime_command;
        message.target_id = runtime_message_target_id::runtime_command_primary;
        if (!copy_command_text(message.command_text, sizeof(message.command_text), command_text)) {
            return false;
        }
        return runtime_message_router_enqueue(message);
    };
    auto enqueue_submitted_command_messages = [&]() {
        char submitted_command[kMaxCommandLength + 1] = {};
        bool queued = false;
        while (compositor_take_submitted_command(submitted_command, sizeof(submitted_command))) {
            queued = enqueue_runtime_command_message(submitted_command) || queued;
        }
        return queued;
    };
    auto consume_runtime_command_message = [&](const char *submitted_command) {
        if (submitted_command == 0 || submitted_command[0] == '\0') {
            return;
        }
        if (kernel_string_equals(submitted_command, kCubeEasterSequence)) {
            const uint64_t now = timer_ticks();
            if (now < easter_guard_until_tick) {
                return;
            }
            const bool quick_repeat =
                (last_easter_match_tick != 0) && ((now - last_easter_match_tick) <= kCubeEasterDoubleWindowTicks);
            last_easter_match_tick = now;
            easter_guard_until_tick = now + kCubeEasterGuardTicks;
            if (quick_repeat) {
                bool enabled = video_cube_easter_mode();
                if (!enabled) {
                    (void)video_toggle_cube_easter_mode(&enabled);
                }
                if (video_trigger_cube_easter_pulse(kCubeEasterPulseTicks)) {
                    video_set_command_center_last_command(kCubeEasterSequence);
                    video_set_command_center_status("easter-pulse");
                    video_set_input_surface_draft("ready");
                    serial_write("[INFO] video: easter pulse stage triggered\n");
                    console_write("[INFO] video: easter pulse stage triggered\n");
                }
            } else {
                bool enabled = false;
                if (video_toggle_cube_easter_mode(&enabled)) {
                    video_set_command_center_last_command(kCubeEasterSequence);
                    video_set_command_center_status(enabled ? "easter-on" : "easter-off");
                    video_set_input_surface_draft("ready");
                    serial_write("[INFO] video: easter key sequence toggled ");
                    serial_write(enabled ? "on\n" : "off\n");
                    console_write("[INFO] video: easter key sequence toggled ");
                    console_write(enabled ? "on\n" : "off\n");
                }
            }
            return;
        }
        handle_serial_runtime_command(submitted_command);
        video_set_command_center_status("ready");
    };
    auto consume_runtime_input_message = [&](const input_message &message) {
        if (message.type == input_message_type::key_submit) {
            compositor_route_event(surface_event_type::key_submit, '\0');
            enqueue_submitted_command_messages();
            command_length = 0;
            return;
        }
        if (message.type == input_message_type::key_backspace) {
            if (command_length != 0) {
                command_length--;
            }
            compositor_route_event(surface_event_type::key_backspace, '\0');
            return;
        }
        char normalized = message.value;
        if (normalized >= 'A' && normalized <= 'Z') {
            normalized = static_cast<char>(normalized + ('a' - 'A'));
        }
        if (normalized < 0x20 || normalized > 0x7e) {
            return;
        }
        if (command_length < kMaxCommandLength) {
            command_length++;
            compositor_route_event(surface_event_type::key_char, normalized);
        }
    };
    auto decode_serial_char_to_runtime_message = [&](char ch, input_message *out_message) {
        if (out_message == 0) {
            return false;
        }
        if (ch == '\r' || ch == '\n') {
            out_message->type = input_message_type::key_submit;
            out_message->value = '\0';
            return true;
        }
        if (ch == '\b' || static_cast<unsigned char>(ch) == 127) {
            out_message->type = input_message_type::key_backspace;
            out_message->value = '\0';
            return true;
        }
        if (ch < 0x20 || ch > 0x7e) {
            return false;
        }
        out_message->type = input_message_type::key_char;
        out_message->value = ch;
        return true;
    };
    auto dispatch_runtime_message = [&](const runtime_message &message) {
        switch (runtime_message_router_route(
            message,
            suppress_keyboard,
            g_runtime_keyboard_events_to_commands)) {
            case runtime_route_decision::dispatch_input:
                consume_runtime_input_message(message.input_event.message);
                return;
            case runtime_route_decision::dispatch_command:
                consume_runtime_command_message(message.command_text);
                return;
            case runtime_route_decision::drop:
            default:
                return;
        }
    };

    char ch = '\0';
    bool saw_serial_input = false;
    while (serial_read_char(&ch)) {
        saw_serial_input = true;
        input_message message = {};
        if (decode_serial_char_to_runtime_message(ch, &message)) {
            runtime_message runtime_message = {};
            runtime_message.category = runtime_message_category::input;
            runtime_message.target = runtime_message_target::compositor_input;
            runtime_message.target_id = runtime_message_target_id::compositor_primary;
            runtime_message.input_event.message = message;
            runtime_message.input_event.source = runtime_input_source::serial;
            runtime_message_router_enqueue(runtime_message);
        }
    }
    const uint64_t now = timer_ticks();
    if (saw_serial_input) {
        last_serial_input_tick = now;
    }
    suppress_keyboard = (now - last_serial_input_tick) <= kSerialKeyboardArbitrationTicks;
    input_message message = {};
    while (input_read_message(&message)) {
        runtime_message runtime_message = {};
        runtime_message.category = runtime_message_category::input;
        runtime_message.target = runtime_message_target::compositor_input;
        runtime_message.target_id = runtime_message_target_id::compositor_primary;
        runtime_message.input_event.message = message;
        runtime_message.input_event.source = runtime_input_source::keyboard;
        runtime_message_router_enqueue(runtime_message);
    }
    runtime_message runtime_event = {};
    while (timer_ticks() >= g_runtime_router_dequeue_pause_until_tick &&
           runtime_message_router_dequeue(&runtime_event)) {
        dispatch_runtime_message(runtime_event);
    }
    enqueue_submitted_command_messages();
    while (timer_ticks() >= g_runtime_router_dequeue_pause_until_tick &&
           runtime_message_router_dequeue(&runtime_event)) {
        dispatch_runtime_message(runtime_event);
    }
}


} // namespace

void kernel_service_runtime_commands(void) {
    kernel_service_runtime_commands_impl();
}
