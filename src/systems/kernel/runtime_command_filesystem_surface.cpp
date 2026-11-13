#include "fortify/systems/kernel/runtime_command_filesystem_surface.h"

#include "fortify/systems/kernel/kernel_string.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

#ifndef FORTIFY_QEMU_SHARE_TAG
#define FORTIFY_QEMU_SHARE_TAG "fortifyshare"
#endif

namespace {
constexpr uint64_t kFsVirtio9pReqStatePending = 1;
constexpr uint64_t kFsVirtio9pReqStateRunning = 2;
constexpr uint64_t kFsVirtio9pReqStateDoneOk = 3;
constexpr uint64_t kFsVirtio9pReqStateDoneErr = 4;

bool g_fs_9p_smoke_active = false;
uint64_t g_fs_9p_smoke_request_id = 0;
uint64_t g_fs_9p_smoke_timeout_ticks = 0;
uint64_t g_fs_9p_smoke_start_tick = 0;

const char *fs_9p_request_state_text(uint64_t state) {
    if (state == kFsVirtio9pReqStatePending) return "pending";
    if (state == kFsVirtio9pReqStateRunning) return "running";
    if (state == kFsVirtio9pReqStateDoneOk) return "done-ok";
    if (state == kFsVirtio9pReqStateDoneErr) return "done-err";
    return "unknown";
}

void log_fs_9p_smoke_result(bool pass,
                            uint64_t request_id,
                            uint64_t state,
                            uint64_t bytes,
                            uint64_t checksum,
                            uint64_t elapsed_ticks,
                            uint64_t timeout_ticks) {
    char request_hex[19];
    char bytes_hex[19];
    char checksum_hex[19];
    char elapsed_hex[19];
    char timeout_hex[19];
    kernel_format_hex64(request_hex, request_id);
    kernel_format_hex64(bytes_hex, bytes);
    kernel_format_hex64(checksum_hex, checksum);
    kernel_format_hex64(elapsed_hex, elapsed_ticks);
    kernel_format_hex64(timeout_hex, timeout_ticks);
    const char *state_text = fs_9p_request_state_text(state);

    serial_write(pass ? "[INFO] filesystem: serial cmd fs-9p-smoke-wait pass id="
                      : "[WARN] filesystem: serial cmd fs-9p-smoke-wait fail id=");
    serial_write(request_hex);
    serial_write(" state=");
    serial_write(state_text);
    serial_write(" bytes=");
    serial_write(bytes_hex);
    serial_write(" checksum=");
    serial_write(checksum_hex);
    serial_write(" elapsed=");
    serial_write(elapsed_hex);
    serial_write(" timeout=");
    serial_write(timeout_hex);
    serial_write(" tick-stall=0\n");

    console_write(pass ? "[INFO] filesystem: serial cmd fs-9p-smoke-wait pass id="
                       : "[WARN] filesystem: serial cmd fs-9p-smoke-wait fail id=");
    console_write(request_hex);
    console_write(" state=");
    console_write(state_text);
    console_write(" bytes=");
    console_write(bytes_hex);
    console_write(" checksum=");
    console_write(checksum_hex);
    console_write(" elapsed=");
    console_write(elapsed_hex);
    console_write(" timeout=");
    console_write(timeout_hex);
    console_write(" tick-stall=0\n");
}

const char *surface_type_name(surface_type type) {
    switch (type) {
        case surface_type::desktop: return "desktop";
        case surface_type::application: return "application";
        case surface_type::document: return "document";
        case surface_type::workspace: return "workspace";
        case surface_type::dashboard: return "dashboard";
        case surface_type::tool: return "tool";
    }
    return "unknown";
}

const char *surface_state_name(surface_state state) {
    switch (state) {
        case surface_state::floating: return "floating";
        case surface_state::docked: return "docked";
        case surface_state::anchored: return "anchored";
        case surface_state::fullscreen: return "fullscreen";
        case surface_state::collapsed: return "collapsed";
        case surface_state::background: return "background";
        case surface_state::modal: return "modal";
        case surface_state::panel: return "panel";
    }
    return "unknown";
}

bool parse_surface_type_token(const char *token, surface_type *out_type) {
    if (token == 0 || out_type == 0) {
        return false;
    }
    if (kernel_string_equals(token, "desktop")) {
        *out_type = surface_type::desktop;
        return true;
    }
    if (kernel_string_equals(token, "application")) {
        *out_type = surface_type::application;
        return true;
    }
    if (kernel_string_equals(token, "document")) {
        *out_type = surface_type::document;
        return true;
    }
    if (kernel_string_equals(token, "workspace")) {
        *out_type = surface_type::workspace;
        return true;
    }
    if (kernel_string_equals(token, "dashboard")) {
        *out_type = surface_type::dashboard;
        return true;
    }
    if (kernel_string_equals(token, "tool")) {
        *out_type = surface_type::tool;
        return true;
    }
    return false;
}

void log_fs_9p_probe_snapshot(void) {
    const uint64_t probes = filesystem_virtio_9p_probe_total();
    const uint64_t devices = filesystem_virtio_9p_device_total();
    char probes_hex[19];
    char devices_hex[19];
    kernel_format_hex64(probes_hex, probes);
    kernel_format_hex64(devices_hex, devices);

    serial_write("[INFO] filesystem: serial cmd fs-9p-probe probes=");
    serial_write(probes_hex);
    serial_write(" devices=");
    serial_write(devices_hex);
    serial_write("\n");
    console_write("[INFO] filesystem: serial cmd fs-9p-probe probes=");
    console_write(probes_hex);
    console_write(" devices=");
    console_write(devices_hex);
    console_write("\n");

    if (devices == 0) {
        return;
    }
    const uint64_t bus = filesystem_virtio_9p_last_bus();
    const uint64_t slot = filesystem_virtio_9p_last_slot();
    const uint64_t function = filesystem_virtio_9p_last_function();
    const uint64_t device_id = filesystem_virtio_9p_last_device_id();
    char bus_hex[19];
    char slot_hex[19];
    char function_hex[19];
    char device_hex[19];
    kernel_format_hex64(bus_hex, bus);
    kernel_format_hex64(slot_hex, slot);
    kernel_format_hex64(function_hex, function);
    kernel_format_hex64(device_hex, device_id);

    serial_write("[INFO] filesystem: serial cmd fs-9p-probe last=b");
    serial_write(bus_hex);
    serial_write(":s");
    serial_write(slot_hex);
    serial_write(":f");
    serial_write(function_hex);
    serial_write(" device=");
    serial_write(device_hex);
    serial_write(" tag=");
    serial_write(FORTIFY_QEMU_SHARE_TAG);
    serial_write("\n");

    console_write("[INFO] filesystem: serial cmd fs-9p-probe last=b");
    console_write(bus_hex);
    console_write(":s");
    console_write(slot_hex);
    console_write(":f");
    console_write(function_hex);
    console_write(" device=");
    console_write(device_hex);
    console_write(" tag=");
    console_write(FORTIFY_QEMU_SHARE_TAG);
    console_write("\n");
}
} // namespace

bool kernel_handle_filesystem_surface_runtime_command(const char *command) {
    if (command == 0) {
        return false;
    }
    if (kernel_string_equals(command, "fs-root-ls") || kernel_string_starts_with(command, "fs-root-ls ")) {
        uint8_t source = 1;
        if (kernel_string_starts_with(command, "fs-root-ls ")) {
            if (!kernel_parse_u8_decimal(command + 11, &source)) {
                serial_write("[WARN] filesystem: serial cmd fs-root-ls expects source 0..255\n");
                console_write("[WARN] filesystem: serial cmd fs-root-ls expects source 0..255\n");
                return true;
            }
        }
        filesystem_root_entry entries[16];
        uint64_t count = 0;
        if (!filesystem_fat32_list_root(source, entries, 16, &count)) {
            serial_write("[WARN] filesystem: serial cmd fs-root-ls failed (FAT32 root unavailable)\n");
            console_write("[WARN] filesystem: serial cmd fs-root-ls failed (FAT32 root unavailable)\n");
            return true;
        }

        char source_hex[19];
        char count_hex[19];
        kernel_format_hex64(source_hex, source);
        kernel_format_hex64(count_hex, count);
        serial_write("[INFO] filesystem: serial cmd fs-root-ls source=");
        serial_write(source_hex);
        serial_write(" entries=");
        serial_write(count_hex);
        serial_write("\n");
        console_write("[INFO] filesystem: serial cmd fs-root-ls source=");
        console_write(source_hex);
        console_write(" entries=");
        console_write(count_hex);
        console_write("\n");

        for (uint64_t i = 0; i < count; i++) {
            char index_hex[19];
            char size_hex[19];
            kernel_format_hex64(index_hex, i);
            kernel_format_hex64(size_hex, entries[i].size_bytes);
            serial_write("[INFO] filesystem: root[");
            serial_write(index_hex);
            serial_write("] ");
            serial_write(entries[i].name_8dot3);
            serial_write(entries[i].is_directory ? " type=dir size=" : " type=file size=");
            serial_write(size_hex);
            serial_write("\n");
            console_write("[INFO] filesystem: root[");
            console_write(index_hex);
            console_write("] ");
            console_write(entries[i].name_8dot3);
            console_write(entries[i].is_directory ? " type=dir size=" : " type=file size=");
            console_write(size_hex);
            console_write("\n");
        }
        return true;
    }
    if (kernel_string_starts_with(command, "fs-root-head ")) {
        char name_8dot3[16];
        char source_token[8];
        uint64_t cursor = 13;
        uint8_t source = 1;
        if (!kernel_parse_token(command, cursor, name_8dot3, sizeof(name_8dot3), &cursor)) {
            serial_write("[WARN] filesystem: serial cmd fs-root-head expects <name.EXT> [source]\n");
            console_write("[WARN] filesystem: serial cmd fs-root-head expects <name.EXT> [source]\n");
            return true;
        }
        if (kernel_parse_token(command, cursor, source_token, sizeof(source_token), &cursor)) {
            if (!kernel_parse_u8_decimal(source_token, &source)) {
                serial_write("[WARN] filesystem: serial cmd fs-root-head source expects 0..255\n");
                console_write("[WARN] filesystem: serial cmd fs-root-head source expects 0..255\n");
                return true;
            }
            char extra_token[4];
            if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                serial_write("[WARN] filesystem: serial cmd fs-root-head accepts only <name.EXT> [source]\n");
                console_write("[WARN] filesystem: serial cmd fs-root-head accepts only <name.EXT> [source]\n");
                return true;
            }
        }

        uint64_t read_bytes = 0;
        uint64_t checksum = 0;
        if (!filesystem_fat32_read_root_file_head(source, name_8dot3, 256, &read_bytes, &checksum)) {
            serial_write("[WARN] filesystem: serial cmd fs-root-head failed (file/root unavailable)\n");
            console_write("[WARN] filesystem: serial cmd fs-root-head failed (file/root unavailable)\n");
            return true;
        }

        char source_hex[19];
        char bytes_hex[19];
        char checksum_hex[19];
        kernel_format_hex64(source_hex, source);
        kernel_format_hex64(bytes_hex, read_bytes);
        kernel_format_hex64(checksum_hex, checksum);
        serial_write("[INFO] filesystem: serial cmd fs-root-head file=");
        serial_write(name_8dot3);
        serial_write(" source=");
        serial_write(source_hex);
        serial_write(" bytes=");
        serial_write(bytes_hex);
        serial_write(" checksum=");
        serial_write(checksum_hex);
        serial_write("\n");
        console_write("[INFO] filesystem: serial cmd fs-root-head file=");
        console_write(name_8dot3);
        console_write(" source=");
        console_write(source_hex);
        console_write(" bytes=");
        console_write(bytes_hex);
        console_write(" checksum=");
        console_write(checksum_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "fs-9p-probe")) {
        filesystem_probe_virtio_9p();
        log_fs_9p_probe_snapshot();
        return true;
    }
    if (kernel_string_equals(command, "fs-9p-mode")) {
        const bool experimental = filesystem_virtio_9p_share_experimental_mode_enabled();
        serial_write("[INFO] filesystem: serial cmd fs-9p-mode ");
        serial_write(experimental ? "experimental\n" : "safe\n");
        console_write("[INFO] filesystem: serial cmd fs-9p-mode ");
        console_write(experimental ? "experimental\n" : "safe\n");
        return true;
    }
    if (kernel_string_starts_with(command, "fs-9p-mode ")) {
        char mode_token[16];
        uint64_t cursor = 11;
        if (!kernel_parse_token(command, cursor, mode_token, sizeof(mode_token), &cursor)) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-mode expects [safe|experimental]\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-mode expects [safe|experimental]\n");
            return true;
        }
        char extra_token[4];
        if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-mode accepts only [safe|experimental]\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-mode accepts only [safe|experimental]\n");
            return true;
        }
        bool enabled = false;
        if (kernel_string_equals(mode_token, "safe")) {
            enabled = false;
        } else if (kernel_string_equals(mode_token, "experimental")) {
            enabled = true;
        } else {
            serial_write("[WARN] filesystem: serial cmd fs-9p-mode expects [safe|experimental]\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-mode expects [safe|experimental]\n");
            return true;
        }
        const bool applied = filesystem_set_virtio_9p_share_experimental_mode(enabled);
        if (!applied && enabled) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-mode experimental blocked in share builds (lock-guard active)\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-mode experimental blocked in share builds (lock-guard active)\n");
            return true;
        }
        serial_write("[INFO] filesystem: serial cmd fs-9p-mode set=");
        serial_write(enabled ? "experimental" : "safe");
        serial_write(enabled ? " (warning: true virtio-9p execution may lock under share mode)\n"
                             : " (share-safe gating active)\n");
        console_write("[INFO] filesystem: serial cmd fs-9p-mode set=");
        console_write(enabled ? "experimental" : "safe");
        console_write(enabled ? " (warning: true virtio-9p execution may lock under share mode)\n"
                              : " (share-safe gating active)\n");
        return true;
    }
    if (kernel_string_starts_with(command, "fs-9p-head ")) {
        char path[64];
        char max_bytes_token[16];
        uint64_t cursor = 11;
        uint64_t max_bytes = 256;
        if (!kernel_parse_token(command, cursor, path, sizeof(path), &cursor)) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-head expects <path> [max-bytes]\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-head expects <path> [max-bytes]\n");
            return true;
        }
        if (kernel_parse_token(command, cursor, max_bytes_token, sizeof(max_bytes_token), &cursor)) {
            if (!kernel_parse_u64_decimal(max_bytes_token, &max_bytes) || max_bytes == 0 || max_bytes > 2048) {
                serial_write("[WARN] filesystem: serial cmd fs-9p-head max-bytes expects 1..2048\n");
                console_write("[WARN] filesystem: serial cmd fs-9p-head max-bytes expects 1..2048\n");
                return true;
            }
            char extra_token[4];
            if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
                serial_write("[WARN] filesystem: serial cmd fs-9p-head accepts only <path> [max-bytes]\n");
                console_write("[WARN] filesystem: serial cmd fs-9p-head accepts only <path> [max-bytes]\n");
                return true;
            }
        }

        uint64_t request_id = 0;
        if (!filesystem_virtio_9p_head_request_submit(path, max_bytes, &request_id)) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-head queue full/unavailable\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-head queue full/unavailable\n");
            return true;
        }
        char request_hex[19];
        kernel_format_hex64(request_hex, request_id);
        serial_write("[INFO] filesystem: serial cmd fs-9p-head queued id=");
        serial_write(request_hex);
        serial_write(" file=");
        serial_write(path);
        serial_write("\n");
        console_write("[INFO] filesystem: serial cmd fs-9p-head queued id=");
        console_write(request_hex);
        console_write(" file=");
        console_write(path);
        console_write("\n");
        return true;
    }
    if (kernel_string_starts_with(command, "fs-9p-head-status ")) {
        uint64_t request_id = 0;
        if (!kernel_parse_u64_decimal(command + 18, &request_id) || request_id == 0) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-head-status expects request-id\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-head-status expects request-id\n");
            return true;
        }
        uint64_t state = 0;
        uint64_t bytes = 0;
        uint64_t checksum = 0;
        if (!filesystem_virtio_9p_head_request_status(request_id, &state, &bytes, &checksum)) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-head-status request not found\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-head-status request not found\n");
            return true;
        }

        char id_hex[19];
        char bytes_hex[19];
        char checksum_hex[19];
        kernel_format_hex64(id_hex, request_id);
        kernel_format_hex64(bytes_hex, bytes);
        kernel_format_hex64(checksum_hex, checksum);
        serial_write("[INFO] filesystem: serial cmd fs-9p-head-status id=");
        serial_write(id_hex);
        serial_write(" state=");
        console_write("[INFO] filesystem: serial cmd fs-9p-head-status id=");
        console_write(id_hex);
        console_write(" state=");

        const char *state_text = fs_9p_request_state_text(state);
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
    if (kernel_string_equals(command, "fs-9p-stats")) {
        char submit_hex[19];
        char done_hex[19];
        char done_ok_hex[19];
        char done_err_hex[19];
        char pending_hex[19];
        char running_hex[19];
        char claimed_hex[19];
        char bridge_ok_hex[19];
        char bridge_err_hex[19];
        char worker_ok_hex[19];
        char worker_err_hex[19];
        char enqueue_hex[19];
        char enqueue_fail_hex[19];
        char mode_block_hex[19];
        char submit_invalid_hex[19];
        char submit_timeout_hex[19];
        char submit_short_hex[19];
        kernel_format_hex64(submit_hex, filesystem_virtio_9p_head_request_submit_total());
        kernel_format_hex64(done_hex, filesystem_virtio_9p_head_request_done_total());
        kernel_format_hex64(done_ok_hex, filesystem_virtio_9p_head_request_done_ok_total());
        kernel_format_hex64(done_err_hex, filesystem_virtio_9p_head_request_done_err_total());
        kernel_format_hex64(pending_hex, filesystem_virtio_9p_head_request_pending_total());
        kernel_format_hex64(running_hex, filesystem_virtio_9p_head_request_running_total());
        kernel_format_hex64(claimed_hex, filesystem_virtio_9p_head_request_claimed_total());
        kernel_format_hex64(bridge_ok_hex, filesystem_virtio_9p_head_request_done_bridge_ok_total());
        kernel_format_hex64(bridge_err_hex, filesystem_virtio_9p_head_request_done_bridge_err_total());
        kernel_format_hex64(worker_ok_hex, filesystem_virtio_9p_head_request_done_worker_ok_total());
        kernel_format_hex64(worker_err_hex, filesystem_virtio_9p_head_request_done_worker_err_total());
        kernel_format_hex64(enqueue_hex, filesystem_virtio_9p_head_request_enqueue_worker_total());
        kernel_format_hex64(enqueue_fail_hex, filesystem_virtio_9p_head_request_enqueue_worker_fail_total());
        kernel_format_hex64(mode_block_hex, filesystem_virtio_9p_mode_blocked_total());
        kernel_format_hex64(submit_invalid_hex, filesystem_virtio_9p_submit_invalid_total());
        kernel_format_hex64(submit_timeout_hex, filesystem_virtio_9p_submit_timeout_total());
        kernel_format_hex64(submit_short_hex, filesystem_virtio_9p_submit_short_response_total());

        serial_write("[INFO] filesystem: serial cmd fs-9p-stats submit/done(ok/err)=");
        serial_write(submit_hex);
        serial_write("/");
        serial_write(done_hex);
        serial_write("(");
        serial_write(done_ok_hex);
        serial_write("/");
        serial_write(done_err_hex);
        serial_write(") state(pending/running/claimed)=");
        serial_write(pending_hex);
        serial_write("/");
        serial_write(running_hex);
        serial_write("/");
        serial_write(claimed_hex);
        serial_write(" path(bridge ok/err worker ok/err)=");
        serial_write(bridge_ok_hex);
        serial_write("/");
        serial_write(bridge_err_hex);
        serial_write(" ");
        serial_write(worker_ok_hex);
        serial_write("/");
        serial_write(worker_err_hex);
        serial_write(" enqueue(req/fail)=");
        serial_write(enqueue_hex);
        serial_write("/");
        serial_write(enqueue_fail_hex);
        serial_write(" mode-block=");
        serial_write(mode_block_hex);
        serial_write(" submit(invalid/timeout/short)=");
        serial_write(submit_invalid_hex);
        serial_write("/");
        serial_write(submit_timeout_hex);
        serial_write("/");
        serial_write(submit_short_hex);
        serial_write("\n");

        console_write("[INFO] filesystem: serial cmd fs-9p-stats submit/done(ok/err)=");
        console_write(submit_hex);
        console_write("/");
        console_write(done_hex);
        console_write("(");
        console_write(done_ok_hex);
        console_write("/");
        console_write(done_err_hex);
        console_write(") state(pending/running/claimed)=");
        console_write(pending_hex);
        console_write("/");
        console_write(running_hex);
        console_write("/");
        console_write(claimed_hex);
        console_write(" path(bridge ok/err worker ok/err)=");
        console_write(bridge_ok_hex);
        console_write("/");
        console_write(bridge_err_hex);
        console_write(" ");
        console_write(worker_ok_hex);
        console_write("/");
        console_write(worker_err_hex);
        console_write(" enqueue(req/fail)=");
        console_write(enqueue_hex);
        console_write("/");
        console_write(enqueue_fail_hex);
        console_write(" mode-block=");
        console_write(mode_block_hex);
        console_write(" submit(invalid/timeout/short)=");
        console_write(submit_invalid_hex);
        console_write("/");
        console_write(submit_timeout_hex);
        console_write("/");
        console_write(submit_short_hex);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "fs-9p-reset-stats")) {
        uint64_t active_requests = 0;
        if (filesystem_virtio_9p_head_request_reset_stats(&active_requests)) {
            serial_write("[INFO] filesystem: serial cmd fs-9p-reset-stats reset=ok\n");
            console_write("[INFO] filesystem: serial cmd fs-9p-reset-stats reset=ok\n");
        } else {
            char active_hex[19];
            kernel_format_hex64(active_hex, active_requests);
            serial_write("[WARN] filesystem: serial cmd fs-9p-reset-stats reset=blocked active=");
            serial_write(active_hex);
            serial_write("\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-reset-stats reset=blocked active=");
            console_write(active_hex);
            console_write("\n");
        }
        return true;
    }
    if (kernel_string_starts_with(command, "fs-9p-smoke-wait ")) {
        char path[64];
        char max_bytes_token[16];
        char timeout_token[16];
        uint64_t cursor = 17;
        uint64_t max_bytes = 256;
        uint64_t timeout_ticks = 600;
        if (!kernel_parse_token(command, cursor, path, sizeof(path), &cursor)) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait expects <path> [max-bytes] [timeout-ticks]\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait expects <path> [max-bytes] [timeout-ticks]\n");
            return true;
        }
        if (kernel_parse_token(command, cursor, max_bytes_token, sizeof(max_bytes_token), &cursor)) {
            if (!kernel_parse_u64_decimal(max_bytes_token, &max_bytes) || max_bytes == 0 || max_bytes > 2048) {
                serial_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait max-bytes expects 1..2048\n");
                console_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait max-bytes expects 1..2048\n");
                return true;
            }
            if (kernel_parse_token(command, cursor, timeout_token, sizeof(timeout_token), &cursor)) {
                if (!kernel_parse_u64_decimal(timeout_token, &timeout_ticks) ||
                    timeout_ticks == 0 || timeout_ticks > 10000) {
                    serial_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait timeout expects 1..10000 ticks\n");
                    console_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait timeout expects 1..10000 ticks\n");
                    return true;
                }
            }
        }
        char extra_token[4];
        if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait accepts only <path> [max-bytes] [timeout-ticks]\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait accepts only <path> [max-bytes] [timeout-ticks]\n");
            return true;
        }

        if (filesystem_virtio_9p_device_total() == 0) {
            (void)filesystem_probe_virtio_9p();
        }
        if (filesystem_virtio_9p_device_total() == 0) {
            char probes_hex[19];
            char devices_hex[19];
            kernel_format_hex64(probes_hex, filesystem_virtio_9p_probe_total());
            kernel_format_hex64(devices_hex, filesystem_virtio_9p_device_total());
            serial_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait fail no-device probes/devices=");
            serial_write(probes_hex);
            serial_write("/");
            serial_write(devices_hex);
            serial_write("\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait fail no-device probes/devices=");
            console_write(probes_hex);
            console_write("/");
            console_write(devices_hex);
            console_write("\n");
            return true;
        }
        if (g_fs_9p_smoke_active) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait busy (already running)\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait busy (already running)\n");
            return true;
        }

        uint64_t request_id = 0;
        if (!filesystem_virtio_9p_head_request_submit(path, max_bytes, &request_id)) {
            serial_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait fail submit\n");
            console_write("[WARN] filesystem: serial cmd fs-9p-smoke-wait fail submit\n");
            return true;
        }

        g_fs_9p_smoke_active = true;
        g_fs_9p_smoke_request_id = request_id;
        g_fs_9p_smoke_timeout_ticks = timeout_ticks;
        g_fs_9p_smoke_start_tick = timer_ticks();

        char request_hex[19];
        char timeout_hex[19];
        kernel_format_hex64(request_hex, request_id);
        kernel_format_hex64(timeout_hex, timeout_ticks);
        serial_write("[INFO] filesystem: serial cmd fs-9p-smoke-wait armed id=");
        serial_write(request_hex);
        serial_write(" timeout=");
        serial_write(timeout_hex);
        serial_write(" (result logs asynchronously)\n");
        console_write("[INFO] filesystem: serial cmd fs-9p-smoke-wait armed id=");
        console_write(request_hex);
        console_write(" timeout=");
        console_write(timeout_hex);
        console_write(" (result logs asynchronously)\n");
        return true;
    }
    if (kernel_string_starts_with(command, "surface-create ")) {
        char type_token[24];
        char title_token[32];
        uint64_t cursor = 15;
        surface_type type = surface_type::application;
        if (!kernel_parse_token(command, cursor, type_token, sizeof(type_token), &cursor) ||
            !parse_surface_type_token(type_token, &type)) {
            serial_write("[WARN] compositor: serial cmd surface-create expects <desktop|application|document|workspace|dashboard|tool> <title>\n");
            console_write("[WARN] compositor: serial cmd surface-create expects <desktop|application|document|workspace|dashboard|tool> <title>\n");
            return true;
        }
        if (!kernel_parse_token(command, cursor, title_token, sizeof(title_token), &cursor)) {
            serial_write("[WARN] compositor: serial cmd surface-create expects title token (use '_' for spaces)\n");
            console_write("[WARN] compositor: serial cmd surface-create expects title token (use '_' for spaces)\n");
            return true;
        }
        char extra_token[4];
        if (kernel_parse_token(command, cursor, extra_token, sizeof(extra_token), 0)) {
            serial_write("[WARN] compositor: serial cmd surface-create accepts only <type> <title>\n");
            console_write("[WARN] compositor: serial cmd surface-create accepts only <type> <title>\n");
            return true;
        }
        uint64_t surface_id = 0;
        if (!compositor_surface_create(type, title_token, 0, &surface_id)) {
            serial_write("[WARN] compositor: serial cmd surface-create failed (compositor/full/invalid)\n");
            console_write("[WARN] compositor: serial cmd surface-create failed (compositor/full/invalid)\n");
            return true;
        }
        char id_hex[19];
        kernel_format_hex64(id_hex, surface_id);
        serial_write("[INFO] compositor: serial cmd surface-create id=");
        serial_write(id_hex);
        serial_write(" type=");
        serial_write(surface_type_name(type));
        serial_write(" title=");
        serial_write(title_token);
        serial_write("\n");
        console_write("[INFO] compositor: serial cmd surface-create id=");
        console_write(id_hex);
        console_write(" type=");
        console_write(surface_type_name(type));
        console_write(" title=");
        console_write(title_token);
        console_write("\n");
        return true;
    }
    if (kernel_string_equals(command, "surface-ls")) {
        const uint64_t count = compositor_surface_count();
        char count_hex[19];
        kernel_format_hex64(count_hex, count);
        serial_write("[INFO] compositor: serial cmd surface-ls count=");
        serial_write(count_hex);
        serial_write("\n");
        console_write("[INFO] compositor: serial cmd surface-ls count=");
        console_write(count_hex);
        console_write("\n");

        for (uint64_t i = 0; i < count; i++) {
            surface_info info = {};
            if (!compositor_surface_get(i, &info)) {
                continue;
            }
            char id_hex[19];
            char layer_hex[19];
            char caps_hex[19];
            char x_hex[19];
            char y_hex[19];
            char w_hex[19];
            char h_hex[19];
            kernel_format_hex64(id_hex, info.id);
            kernel_format_hex64(layer_hex, info.layer);
            kernel_format_hex64(caps_hex, info.capabilities);
            kernel_format_hex64(x_hex, static_cast<uint64_t>(static_cast<int64_t>(info.x)));
            kernel_format_hex64(y_hex, static_cast<uint64_t>(static_cast<int64_t>(info.y)));
            kernel_format_hex64(w_hex, info.width);
            kernel_format_hex64(h_hex, info.height);
            serial_write("[INFO] compositor: surface id=");
            serial_write(id_hex);
            serial_write(" type=");
            serial_write(surface_type_name(info.type));
            serial_write(" state=");
            serial_write(surface_state_name(info.state));
            serial_write(" layer=");
            serial_write(layer_hex);
            serial_write(" pos=");
            serial_write(x_hex);
            serial_write(",");
            serial_write(y_hex);
            serial_write(" size_w=");
            serial_write(w_hex);
            serial_write(" size_h=");
            serial_write(h_hex);
            serial_write(" caps=");
            serial_write(caps_hex);
            serial_write(" title=");
            serial_write(info.title);
            serial_write("\n");

            console_write("[INFO] compositor: surface id=");
            console_write(id_hex);
            console_write(" type=");
            console_write(surface_type_name(info.type));
            console_write(" state=");
            console_write(surface_state_name(info.state));
            console_write(" layer=");
            console_write(layer_hex);
            console_write(" pos=");
            console_write(x_hex);
            console_write(",");
            console_write(y_hex);
            console_write(" size_w=");
            console_write(w_hex);
            console_write(" size_h=");
            console_write(h_hex);
            console_write(" caps=");
            console_write(caps_hex);
            console_write(" title=");
            console_write(info.title);
            console_write("\n");
        }
        return true;
    }
    return false;
}

void kernel_poll_filesystem_surface_runtime_tasks(void) {
    if (!g_fs_9p_smoke_active) {
        return;
    }
    const uint64_t now = timer_ticks();
    const uint64_t elapsed = now - g_fs_9p_smoke_start_tick;
    const bool timed_out = elapsed > g_fs_9p_smoke_timeout_ticks;

    uint64_t state = 0;
    uint64_t bytes = 0;
    uint64_t checksum = 0;
    const bool have_status =
        filesystem_virtio_9p_head_request_status(g_fs_9p_smoke_request_id, &state, &bytes, &checksum);
    const bool done = have_status &&
                      (state == kFsVirtio9pReqStateDoneOk || state == kFsVirtio9pReqStateDoneErr);
    if (!done && !timed_out) {
        return;
    }
    if (!have_status) {
        state = kFsVirtio9pReqStateDoneErr;
    }
    const bool pass = done && state == kFsVirtio9pReqStateDoneOk;
    log_fs_9p_smoke_result(pass,
                           g_fs_9p_smoke_request_id,
                           state,
                           bytes,
                           checksum,
                           elapsed,
                           g_fs_9p_smoke_timeout_ticks);
    g_fs_9p_smoke_active = false;
    g_fs_9p_smoke_request_id = 0;
    g_fs_9p_smoke_timeout_ticks = 0;
    g_fs_9p_smoke_start_tick = 0;
}
