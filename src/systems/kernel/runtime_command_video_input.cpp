#include "fortify/systems/kernel/runtime_command_video_input.h"

#include "fortify/systems/kernel/kernel_string.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

bool kernel_handle_video_input_runtime_command(const char *command, bool *runtime_keyboard_events_to_commands) {
    if (command == 0) {
        return false;
    }
    if (kernel_string_equals(command, "video-demo")) {
        const bool enabled = video_demo_enabled();
        serial_write("[INFO] video: serial cmd video-demo current=");
        serial_write(enabled ? "1\n" : "0\n");
        console_write("[INFO] video: serial cmd video-demo current=");
        console_write(enabled ? "1\n" : "0\n");
        return true;
    }
    if (kernel_string_starts_with(command, "video-demo ")) {
        const char *value_text = command + 11;
        uint64_t enabled = 0;
        if (!kernel_parse_u64_decimal(value_text, &enabled) || enabled > 1) {
            serial_write("[WARN] video: serial cmd video-demo expects 0 or 1\n");
            console_write("[WARN] video: serial cmd video-demo expects 0 or 1\n");
            return true;
        }
        video_set_demo_enabled(enabled != 0);
        serial_write("[INFO] video: serial cmd video-demo set=");
        serial_write(enabled != 0 ? "1\n" : "0\n");
        console_write("[INFO] video: serial cmd video-demo set=");
        console_write(enabled != 0 ? "1\n" : "0\n");
        return true;
    }
    if (kernel_string_equals(command, "video-style")) {
        char style_hex[19];
        const uint64_t style = video_render_style();
        kernel_format_hex64(style_hex, style);
        serial_write("[INFO] video: serial cmd video-style current=");
        serial_write(style_hex);
        serial_write(" (0=solid,1=wire,2=both)\n");
        console_write("[INFO] video: serial cmd video-style current=");
        console_write(style_hex);
        console_write(" (0=solid,1=wire,2=both)\n");
        return true;
    }
    if (kernel_string_starts_with(command, "video-style ")) {
        const char *value_text = command + 12;
        uint64_t style = 0;
        if (!kernel_parse_u64_decimal(value_text, &style) || style > 2 || !video_set_render_style(style)) {
            serial_write("[WARN] video: serial cmd video-style expects 0..2\n");
            console_write("[WARN] video: serial cmd video-style expects 0..2\n");
            return true;
        }
        char style_hex[19];
        kernel_format_hex64(style_hex, style);
        serial_write("[INFO] video: serial cmd video-style set=");
        serial_write(style_hex);
        serial_write("\n");
        console_write("[INFO] video: serial cmd video-style set=");
        console_write(style_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "video-speed")) {
        char speed_hex[19];
        const uint64_t speed = video_spin_step();
        kernel_format_hex64(speed_hex, speed);
        serial_write("[INFO] video: serial cmd video-speed current=");
        serial_write(speed_hex);
        serial_write("\n");
        console_write("[INFO] video: serial cmd video-speed current=");
        console_write(speed_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "video-speed ")) {
        const char *value_text = command + 12;
        uint64_t speed = 0;
        if (!kernel_parse_u64_decimal(value_text, &speed) || !video_set_spin_step(speed)) {
            serial_write("[WARN] video: serial cmd video-speed expects 1..32\n");
            console_write("[WARN] video: serial cmd video-speed expects 1..32\n");
            return true;
        }
        char speed_hex[19];
        kernel_format_hex64(speed_hex, speed);
        serial_write("[INFO] video: serial cmd video-speed set=");
        serial_write(speed_hex);
        serial_write("\n");
        console_write("[INFO] video: serial cmd video-speed set=");
        console_write(speed_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "video-font")) {
        char style_hex[19];
        const uint64_t style = video_font_style();
        kernel_format_hex64(style_hex, style);
        serial_write("[INFO] video: serial cmd video-font current=");
        serial_write(style_hex);
        serial_write(" (0=mono,1=bold,2=shadow)\n");
        console_write("[INFO] video: serial cmd video-font current=");
        console_write(style_hex);
        console_write(" (0=mono,1=bold,2=shadow)\n");
        return true;
    }
    if (kernel_string_equals(command, "input-kbd")) {
        serial_write("[INFO] input: serial cmd input-kbd current=");
        serial_write(*runtime_keyboard_events_to_commands ? "1\n" : "0\n");
        console_write("[INFO] input: serial cmd input-kbd current=");
        console_write(*runtime_keyboard_events_to_commands ? "1\n" : "0\n");
        return true;
    }
    if (kernel_string_starts_with(command, "input-kbd ")) {
        const char *value_text = command + 10;
        uint64_t enabled = 0;
        if (!kernel_parse_u64_decimal(value_text, &enabled) || enabled > 1) {
            serial_write("[WARN] input: serial cmd input-kbd expects 0 or 1\n");
            console_write("[WARN] input: serial cmd input-kbd expects 0 or 1\n");
            return true;
        }
        *runtime_keyboard_events_to_commands = (enabled != 0);
        serial_write("[INFO] input: serial cmd input-kbd set=");
        serial_write(*runtime_keyboard_events_to_commands ? "1\n" : "0\n");
        console_write("[INFO] input: serial cmd input-kbd set=");
        console_write(*runtime_keyboard_events_to_commands ? "1\n" : "0\n");
        return true;
    }
    if (kernel_string_starts_with(command, "video-font ")) {
        const char *value_text = command + 11;
        uint64_t style = 0;
        if (!kernel_parse_u64_decimal(value_text, &style) || style > 2 || !video_set_font_style(style)) {
            serial_write("[WARN] video: serial cmd video-font expects 0..2\n");
            console_write("[WARN] video: serial cmd video-font expects 0..2\n");
            return true;
        }
        char style_hex[19];
        kernel_format_hex64(style_hex, style);
        serial_write("[INFO] video: serial cmd video-font set=");
        serial_write(style_hex);
        serial_write("\n");
        console_write("[INFO] video: serial cmd video-font set=");
        console_write(style_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "video-text-scale")) {
        char scale_hex[19];
        const uint64_t scale = video_text_scale();
        kernel_format_hex64(scale_hex, scale);
        serial_write("[INFO] video: serial cmd video-text-scale current=");
        serial_write(scale_hex);
        serial_write("\n");
        console_write("[INFO] video: serial cmd video-text-scale current=");
        console_write(scale_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "video-text-scale ")) {
        const char *value_text = command + 17;
        uint64_t scale = 0;
        if (!kernel_parse_u64_decimal(value_text, &scale) || !video_set_text_scale(scale)) {
            serial_write("[WARN] video: serial cmd video-text-scale expects 1..4\n");
            console_write("[WARN] video: serial cmd video-text-scale expects 1..4\n");
            return true;
        }
        char scale_hex[19];
        kernel_format_hex64(scale_hex, scale);
        serial_write("[INFO] video: serial cmd video-text-scale set=");
        serial_write(scale_hex);
        serial_write("\n");
        console_write("[INFO] video: serial cmd video-text-scale set=");
        console_write(scale_hex);
        console_write("\n");
        return true;
    }
    return false;
}
