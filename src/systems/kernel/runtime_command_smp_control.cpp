#include "fortify/systems/kernel/runtime_command_smp_control.h"

#include "fortify/systems/kernel/kernel_string.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

extern "C" volatile uint64_t g_ap_trigger_profile;

bool kernel_handle_smp_control_runtime_command(const char *command) {
    if (command == 0) {
        return false;
    }
    if (kernel_string_equals(command, "ap-task-start")) {
        const bool started = scheduler_start_ap_pinned_demo_tasks();
        serial_write(started
                         ? "[INFO] smp: serial cmd ap-task-start enabled AP-pinned runnable tasks\n"
                         : "[WARN] smp: serial cmd ap-task-start found no AP targets\n");
        console_write(started
                          ? "[INFO] smp: serial cmd ap-task-start enabled AP-pinned runnable tasks\n"
                          : "[WARN] smp: serial cmd ap-task-start found no AP targets\n");
        return true;
    }
    if (kernel_string_equals(command, "ap-task-stop")) {
        scheduler_stop_ap_pinned_demo_tasks();
        serial_write("[INFO] smp: serial cmd ap-task-stop disabled AP-pinned runnable tasks\n");
        console_write("[INFO] smp: serial cmd ap-task-stop disabled AP-pinned runnable tasks\n");
        return true;
    }
    if (kernel_string_equals(command, "ap-task-migrate")) {
        const bool enabled = scheduler_ap_pinned_task_migration_enabled();
        serial_write("[INFO] smp: serial cmd ap-task-migrate current=");
        serial_write(enabled ? "1\n" : "0\n");
        console_write("[INFO] smp: serial cmd ap-task-migrate current=");
        console_write(enabled ? "1\n" : "0\n");
        return true;
    }
    if (kernel_string_starts_with(command, "ap-task-migrate ")) {
        const char *value_text = command + 16;
        uint64_t enabled = 0;
        if (!kernel_parse_u64_decimal(value_text, &enabled) || enabled > 1) {
            serial_write("[WARN] smp: serial cmd ap-task-migrate expects 0 or 1\n");
            console_write("[WARN] smp: serial cmd ap-task-migrate expects 0 or 1\n");
            return true;
        }
        scheduler_set_ap_pinned_task_migration_enabled(enabled != 0);
        serial_write("[INFO] smp: serial cmd ap-task-migrate set=");
        serial_write(enabled != 0 ? "1\n" : "0\n");
        console_write("[INFO] smp: serial cmd ap-task-migrate set=");
        console_write(enabled != 0 ? "1\n" : "0\n");
        return true;
    }
    if (kernel_string_starts_with(command, "net-rx-inject ")) {
        const char *count_text = command + 14;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd net-rx-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd net-rx-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = networking_inject_rx_events(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd net-rx-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd net-rx-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "web-head ")) {
        char target[64];
        char max_bytes_token[16];
        uint64_t cursor = 9;
        uint64_t max_bytes = 256;
        if (!kernel_parse_token(command, cursor, target, sizeof(target), &cursor)) {
            serial_write("[WARN] webaccess: serial cmd web-head expects <target> [max-bytes]\n");
            console_write("[WARN] webaccess: serial cmd web-head expects <target> [max-bytes]\n");
            return true;
        }
        if (kernel_parse_token(command, cursor, max_bytes_token, sizeof(max_bytes_token), &cursor)) {
            if (!kernel_parse_u64_decimal(max_bytes_token, &max_bytes) || max_bytes == 0 || max_bytes > 4096) {
                serial_write("[WARN] webaccess: serial cmd web-head max-bytes expects 1..4096\n");
                console_write("[WARN] webaccess: serial cmd web-head max-bytes expects 1..4096\n");
                return true;
            }
            char extra_token[4];
            if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                serial_write("[WARN] webaccess: serial cmd web-head accepts only <target> [max-bytes]\n");
                console_write("[WARN] webaccess: serial cmd web-head accepts only <target> [max-bytes]\n");
                return true;
            }
        }

        uint64_t request_id = 0;
        if (!webaccess_head_request_submit(target, max_bytes, &request_id)) {
            serial_write("[WARN] webaccess: serial cmd web-head queue full/unavailable\n");
            console_write("[WARN] webaccess: serial cmd web-head queue full/unavailable\n");
            return true;
        }
        char request_hex[19];
        kernel_format_hex64(request_hex, request_id);
        serial_write("[INFO] webaccess: serial cmd web-head queued id=");
        serial_write(request_hex);
        serial_write(" target=");
        serial_write(target);
        serial_write("\n");
        console_write("[INFO] webaccess: serial cmd web-head queued id=");
        console_write(request_hex);
        console_write(" target=");
        console_write(target);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "web-head-status ")) {
        constexpr uint64_t kWebHeadRequestStatePending = 1;
        constexpr uint64_t kWebHeadRequestStateRunning = 2;
        constexpr uint64_t kWebHeadRequestStateDoneOk = 3;
        constexpr uint64_t kWebHeadRequestStateDoneErr = 4;
        uint64_t request_id = 0;
        if (!kernel_parse_u64_decimal(command + 16, &request_id) || request_id == 0) {
            serial_write("[WARN] webaccess: serial cmd web-head-status expects request-id\n");
            console_write("[WARN] webaccess: serial cmd web-head-status expects request-id\n");
            return true;
        }
        uint64_t state = 0;
        uint64_t bytes = 0;
        uint64_t checksum = 0;
        if (!webaccess_head_request_status(request_id, &state, &bytes, &checksum)) {
            serial_write("[WARN] webaccess: serial cmd web-head-status request not found\n");
            console_write("[WARN] webaccess: serial cmd web-head-status request not found\n");
            return true;
        }
        char id_hex[19];
        char bytes_hex[19];
        char checksum_hex[19];
        kernel_format_hex64(id_hex, request_id);
        kernel_format_hex64(bytes_hex, bytes);
        kernel_format_hex64(checksum_hex, checksum);
        serial_write("[INFO] webaccess: serial cmd web-head-status id=");
        serial_write(id_hex);
        serial_write(" state=");
        console_write("[INFO] webaccess: serial cmd web-head-status id=");
        console_write(id_hex);
        console_write(" state=");

        const char *state_text = "unknown";
        if (state == kWebHeadRequestStatePending) state_text = "pending";
        else if (state == kWebHeadRequestStateRunning) state_text = "running";
        else if (state == kWebHeadRequestStateDoneOk) state_text = "done-ok";
        else if (state == kWebHeadRequestStateDoneErr) state_text = "done-err";
        serial_write(state_text);
        serial_write(" bytes=");
        serial_write(bytes_hex);
        serial_write(" checksum=");
        serial_write(checksum_hex);
        serial_write("\n");

        console_write(state_text);
        console_write(" bytes=");
        console_write(bytes_hex);
        console_write(" checksum=");
        console_write(checksum_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "usb-inject ")) {
        const char *count_text = command + 11;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd usb-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd usb-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = usb_inject_events(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd usb-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd usb-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "storage-inject ")) {
        const char *count_text = command + 15;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd storage-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd storage-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = filesystem_inject_storage_events(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd storage-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd storage-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "audio-inject ")) {
        const char *count_text = command + 13;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd audio-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd audio-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = audio_inject_events(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd audio-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd audio-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "webcam-inject ")) {
        const char *count_text = command + 14;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd webcam-inject expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd webcam-inject expects count 1..4096\n");
            return true;
        }
        const uint64_t injected = webcam_inject_frames(count);
        char count_hex[19];
        char injected_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(injected_hex, injected);
        serial_write("[INFO] smp: serial cmd webcam-inject requested=");
        serial_write(count_hex);
        serial_write(" injected=");
        serial_write(injected_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd webcam-inject requested=");
        console_write(count_hex);
        console_write(" injected=");
        console_write(injected_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "inject-all ")) {
        const char *count_text = command + 11;
        uint64_t count = 0;
        if (!kernel_parse_u64_decimal(count_text, &count) || count == 0 || count > 4096) {
            serial_write("[WARN] smp: serial cmd inject-all expects count 1..4096\n");
            console_write("[WARN] smp: serial cmd inject-all expects count 1..4096\n");
            return true;
        }
        const uint64_t net_injected = networking_inject_rx_events(count);
        const uint64_t usb_injected = usb_inject_events(count);
        const uint64_t storage_injected = filesystem_inject_storage_events(count);
        const uint64_t audio_injected = audio_inject_events(count);
        const uint64_t webcam_injected = webcam_inject_frames(count);
        char count_hex[19];
        char net_hex[19];
        char usb_hex[19];
        char storage_hex[19];
        char audio_hex[19];
        char webcam_hex[19];
        kernel_format_hex64(count_hex, count);
        kernel_format_hex64(net_hex, net_injected);
        kernel_format_hex64(usb_hex, usb_injected);
        kernel_format_hex64(storage_hex, storage_injected);
        kernel_format_hex64(audio_hex, audio_injected);
        kernel_format_hex64(webcam_hex, webcam_injected);
        serial_write("[INFO] smp: serial cmd inject-all requested=");
        serial_write(count_hex);
        serial_write(" net/usb/storage/audio/webcam=");
        serial_write(net_hex);
        serial_write("/");
        serial_write(usb_hex);
        serial_write("/");
        serial_write(storage_hex);
        serial_write("/");
        serial_write(audio_hex);
        serial_write("/");
        serial_write(webcam_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd inject-all requested=");
        console_write(count_hex);
        console_write(" net/usb/storage/audio/webcam=");
        console_write(net_hex);
        console_write("/");
        console_write(usb_hex);
        console_write("/");
        console_write(storage_hex);
        console_write("/");
        console_write(audio_hex);
        console_write("/");
        console_write(webcam_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "ap-trigger 0") ||
        kernel_string_equals(command, "ap-trigger 1") ||
        kernel_string_equals(command, "ap-trigger 2")) {
        const uint64_t profile = static_cast<uint64_t>(command[11] - '0');
        __atomic_store_n(&g_ap_trigger_profile, profile, __ATOMIC_RELAXED);
        char profile_hex[19];
        kernel_format_hex64(profile_hex, profile);
        serial_write("[INFO] smp: serial cmd ap-trigger set request=");
        serial_write(profile_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd ap-trigger set request=");
        console_write(profile_hex);
        console_write("\n");
        return true;
    }

    return false;
}
