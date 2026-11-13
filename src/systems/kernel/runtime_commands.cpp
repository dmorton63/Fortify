#include "fortify/systems/kernel/runtime_commands.h"

#include "fortify/systems/kernel/ap_trigger_profile.h"
#include "fortify/systems/kernel/kernel_idle_observability.h"
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
static uint64_t g_runtime_backlog_test_pause_ticks = 300;
static uint64_t g_runtime_smoke_extended_backlog_count = 16;

static void log_ap_trigger_command_help(void) {
    log_info("smp: serial cmd: ap-trigger [0|1|2], ap-trigger, ap-stats, ap-task-start, ap-task-stop, ap-task-migrate [0|1], ap-migration-smoke [timeout-ticks], smp-log [default|quiet], smp-log-health [ticks], video-demo [0|1], video-style [0|1|2], video-speed <1..32>, video-font [0|1|2], video-text-scale <1..4>, input-kbd [0|1], net-rx-inject <count>, net-rx-stats, net-rx-smoke [count], net-rx-smoke-wait [count] [timeout-ticks], web-head <target> [max-bytes], web-head-status <request-id>, web-head-stats, web-head-smoke-wait <target> [max-bytes] [timeout-ticks], usb-inject <count>, usb-stats, usb-smoke [count], usb-smoke-wait [count] [timeout-ticks], storage-inject <count>, storage-stats, storage-smoke [count], storage-smoke-wait [count] [timeout-ticks], audio-inject <count>, audio-stats, audio-smoke [count], audio-smoke-wait [count] [timeout-ticks], webcam-inject <count>, webcam-stats, webcam-smoke [count], webcam-smoke-wait [count] [timeout-ticks], inject-all <count>, fs-root-ls [source], fs-root-head <name.EXT> [source], fs-9p-probe, fs-9p-mode [safe|experimental], fs-9p-head <path> [max-bytes], fs-9p-head-status <request-id>, fs-9p-stats, fs-9p-reset-stats, fs-9p-smoke-wait <path> [max-bytes] [timeout-ticks], surface-create <type> <title>, surface-ls, rmsg-profile [balanced|stress|quiet], rmsg-stats, rmsg-verify, rmsg-drop-test <src|kbd|unroute>, rmsg-selftest, rmsg-reset-stats, rmsg-smoke, rmsg-smoke-extended, rmsg-backlog-test <count> [input|cmd|both], help");
}

static void log_ap_stats_snapshot(void) {
    kernel_log_ap_stats_snapshot();
}

static void log_runtime_message_router_stats_snapshot(void) {
    char queue_depth_hex[19];
    char queue_high_water_hex[19];
    char queue_drop_hex[19];
    char enqueued_total_hex[19];
    char dequeued_total_hex[19];
    char dispatch_input_hex[19];
    char dispatch_command_hex[19];
    char route_drop_hex[19];
    char drop_src_hex[19];
    char drop_kbd_hex[19];
    char drop_unroute_hex[19];
    char input_depth_hex[19];
    char input_high_water_hex[19];
    char input_drop_hex[19];
    char input_enqueued_hex[19];
    char input_dequeued_hex[19];
    char command_depth_hex[19];
    char command_high_water_hex[19];
    char command_drop_hex[19];
    char command_enqueued_hex[19];
    char command_dequeued_hex[19];
    kernel_format_hex64(queue_depth_hex, runtime_message_router_queue_depth());
    kernel_format_hex64(queue_high_water_hex, runtime_message_router_queue_high_water());
    kernel_format_hex64(queue_drop_hex, runtime_message_router_drop_total());
    kernel_format_hex64(enqueued_total_hex, runtime_message_router_enqueued_total());
    kernel_format_hex64(dequeued_total_hex, runtime_message_router_dequeued_total());
    kernel_format_hex64(dispatch_input_hex, runtime_message_router_dispatch_input_total());
    kernel_format_hex64(dispatch_command_hex, runtime_message_router_dispatch_command_total());
    kernel_format_hex64(route_drop_hex, runtime_message_router_route_drop_total());
    kernel_format_hex64(drop_src_hex, runtime_message_router_route_drop_invalid_source_total());
    kernel_format_hex64(drop_kbd_hex, runtime_message_router_route_drop_keyboard_suppressed_total());
    kernel_format_hex64(drop_unroute_hex, runtime_message_router_route_drop_unroutable_total());
    kernel_format_hex64(input_depth_hex,
                        runtime_message_router_queue_depth_for_target(runtime_message_target::compositor_input));
    kernel_format_hex64(input_high_water_hex,
                        runtime_message_router_queue_high_water_for_target(runtime_message_target::compositor_input));
    kernel_format_hex64(input_drop_hex,
                        runtime_message_router_drop_total_for_target(runtime_message_target::compositor_input));
    kernel_format_hex64(input_enqueued_hex,
                        runtime_message_router_enqueued_total_for_target(runtime_message_target::compositor_input));
    kernel_format_hex64(input_dequeued_hex,
                        runtime_message_router_dequeued_total_for_target(runtime_message_target::compositor_input));
    kernel_format_hex64(command_depth_hex,
                        runtime_message_router_queue_depth_for_target(runtime_message_target::runtime_command));
    kernel_format_hex64(command_high_water_hex,
                        runtime_message_router_queue_high_water_for_target(runtime_message_target::runtime_command));
    kernel_format_hex64(command_drop_hex,
                        runtime_message_router_drop_total_for_target(runtime_message_target::runtime_command));
    kernel_format_hex64(command_enqueued_hex,
                        runtime_message_router_enqueued_total_for_target(runtime_message_target::runtime_command));
    kernel_format_hex64(command_dequeued_hex,
                        runtime_message_router_dequeued_total_for_target(runtime_message_target::runtime_command));

    serial_write("[INFO] smp: serial cmd rmsg-stats q(depth/hwm/drop)=");
    serial_write(queue_depth_hex);
    serial_write("/");
    serial_write(queue_high_water_hex);
    serial_write("/");
    serial_write(queue_drop_hex);
    serial_write(" flow(enq/deq in/cmd/drop)=");
    serial_write(enqueued_total_hex);
    serial_write("/");
    serial_write(dequeued_total_hex);
    serial_write(" ");
    serial_write(dispatch_input_hex);
    serial_write("/");
    serial_write(dispatch_command_hex);
    serial_write("/");
    serial_write(route_drop_hex);
    serial_write(" drop-reason(src/kbd/unroute)=");
    serial_write(drop_src_hex);
    serial_write("/");
    serial_write(drop_kbd_hex);
    serial_write("/");
    serial_write(drop_unroute_hex);
    serial_write(" target(input d/h/d e/d cmd d/h/d e/d)=");
    serial_write(input_depth_hex);
    serial_write("/");
    serial_write(input_high_water_hex);
    serial_write("/");
    serial_write(input_drop_hex);
    serial_write(" ");
    serial_write(input_enqueued_hex);
    serial_write("/");
    serial_write(input_dequeued_hex);
    serial_write(" ");
    serial_write(command_depth_hex);
    serial_write("/");
    serial_write(command_high_water_hex);
    serial_write("/");
    serial_write(command_drop_hex);
    serial_write(" ");
    serial_write(command_enqueued_hex);
    serial_write("/");
    serial_write(command_dequeued_hex);
    serial_write("\n");

    console_write("[INFO] smp: serial cmd rmsg-stats q(depth/hwm/drop)=");
    console_write(queue_depth_hex);
    console_write("/");
    console_write(queue_high_water_hex);
    console_write("/");
    console_write(queue_drop_hex);
    console_write(" flow(enq/deq in/cmd/drop)=");
    console_write(enqueued_total_hex);
    console_write("/");
    console_write(dequeued_total_hex);
    console_write(" ");
    console_write(dispatch_input_hex);
    console_write("/");
    console_write(dispatch_command_hex);
    console_write("/");
    console_write(route_drop_hex);
    console_write(" drop-reason(src/kbd/unroute)=");
    console_write(drop_src_hex);
    console_write("/");
    console_write(drop_kbd_hex);
    console_write("/");
    console_write(drop_unroute_hex);
    console_write(" target(input d/h/d e/d cmd d/h/d e/d)=");
    console_write(input_depth_hex);
    console_write("/");
    console_write(input_high_water_hex);
    console_write("/");
    console_write(input_drop_hex);
    console_write(" ");
    console_write(input_enqueued_hex);
    console_write("/");
    console_write(input_dequeued_hex);
    console_write(" ");
    console_write(command_depth_hex);
    console_write("/");
    console_write(command_high_water_hex);
    console_write("/");
    console_write(command_drop_hex);
    console_write(" ");
    console_write(command_enqueued_hex);
    console_write("/");
    console_write(command_dequeued_hex);
    console_write("\n");
}

static bool run_runtime_message_router_selftest(uint64_t *out_delta_src,
                                                uint64_t *out_delta_kbd,
                                                uint64_t *out_delta_unroute,
                                                uint64_t *out_delta_total) {
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
    if (out_delta_src != 0) {
        *out_delta_src = delta_src;
    }
    if (out_delta_kbd != 0) {
        *out_delta_kbd = delta_kbd;
    }
    if (out_delta_unroute != 0) {
        *out_delta_unroute = delta_unroute;
    }
    if (out_delta_total != 0) {
        *out_delta_total = delta_total;
    }
    return probe_src && probe_kbd && probe_unroute && delta_src == 1 && delta_kbd == 1 &&
           delta_unroute == 1 && delta_total == 3;
}

enum runtime_message_router_verify_failure : uint64_t {
    kVerifyFailureQueueDepth = 1ULL << 0,
    kVerifyFailureDropTotal = 1ULL << 1,
    kVerifyFailureEnqueuedTotal = 1ULL << 2,
    kVerifyFailureDequeuedTotal = 1ULL << 3,
    kVerifyFailureRouteDropReasonTotal = 1ULL << 4,
    kVerifyFailureDispatchParity = 1ULL << 5,
};

static bool verify_runtime_message_router_consistency(uint64_t *out_failure_mask) {
    const uint64_t input_depth =
        runtime_message_router_queue_depth_for_target(runtime_message_target::compositor_input);
    const uint64_t command_depth =
        runtime_message_router_queue_depth_for_target(runtime_message_target::runtime_command);
    const uint64_t input_drop =
        runtime_message_router_drop_total_for_target(runtime_message_target::compositor_input);
    const uint64_t command_drop =
        runtime_message_router_drop_total_for_target(runtime_message_target::runtime_command);
    const uint64_t input_enqueued =
        runtime_message_router_enqueued_total_for_target(runtime_message_target::compositor_input);
    const uint64_t command_enqueued =
        runtime_message_router_enqueued_total_for_target(runtime_message_target::runtime_command);
    const uint64_t input_dequeued =
        runtime_message_router_dequeued_total_for_target(runtime_message_target::compositor_input);
    const uint64_t command_dequeued =
        runtime_message_router_dequeued_total_for_target(runtime_message_target::runtime_command);
    const uint64_t dispatch_input_total = runtime_message_router_dispatch_input_total();
    const uint64_t dispatch_command_total = runtime_message_router_dispatch_command_total();
    const uint64_t route_drop_total = runtime_message_router_route_drop_total();
    const uint64_t route_drop_reason_total =
        runtime_message_router_route_drop_invalid_source_total() +
        runtime_message_router_route_drop_keyboard_suppressed_total() +
        runtime_message_router_route_drop_unroutable_total();

    uint64_t failure_mask = 0;
    if (runtime_message_router_queue_depth() != (input_depth + command_depth)) {
        failure_mask |= kVerifyFailureQueueDepth;
    }
    if (runtime_message_router_drop_total() != (input_drop + command_drop)) {
        failure_mask |= kVerifyFailureDropTotal;
    }
    if (runtime_message_router_enqueued_total() != (input_enqueued + command_enqueued)) {
        failure_mask |= kVerifyFailureEnqueuedTotal;
    }
    if (runtime_message_router_dequeued_total() != (input_dequeued + command_dequeued)) {
        failure_mask |= kVerifyFailureDequeuedTotal;
    }
    if (route_drop_total != route_drop_reason_total) {
        failure_mask |= kVerifyFailureRouteDropReasonTotal;
    }
    const uint64_t routed_total = dispatch_input_total + dispatch_command_total + route_drop_total;
    const uint64_t dequeued_total = runtime_message_router_dequeued_total();
    if (routed_total < dequeued_total ||
        (routed_total - dequeued_total) > route_drop_total) {
        failure_mask |= kVerifyFailureDispatchParity;
    }
    if (out_failure_mask != 0) {
        *out_failure_mask = failure_mask;
    }
    return failure_mask == 0;
}

static void set_runtime_message_router_profile(uint64_t warn_depth,
                                               uint64_t transition_ticks,
                                               uint64_t backlog_pause_ticks,
                                               uint64_t smoke_backlog_count) {
    kernel_set_runtime_message_backlog_guardrail(warn_depth, transition_ticks);
    g_runtime_backlog_test_pause_ticks = backlog_pause_ticks;
    g_runtime_smoke_extended_backlog_count = smoke_backlog_count;
}

static bool inject_runtime_message_router_backlog(uint64_t count,
                                                  bool inject_input,
                                                  bool inject_command,
                                                  uint64_t *out_input_enqueued,
                                                  uint64_t *out_command_enqueued) {
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
            input_msg.input_event.message.value = '\0';
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
    if (out_input_enqueued != 0) {
        *out_input_enqueued = input_enqueued;
    }
    if (out_command_enqueued != 0) {
        *out_command_enqueued = command_enqueued;
    }
    return (!inject_input || input_enqueued == count) && (!inject_command || command_enqueued == count);
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
    if (kernel_string_equals(command, "rmsg-stats")) {
        log_runtime_message_router_stats_snapshot();
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
        uint64_t delta_src = 0;
        uint64_t delta_kbd = 0;
        uint64_t delta_unroute = 0;
        uint64_t delta_total = 0;
        const bool pass = run_runtime_message_router_selftest(
            &delta_src, &delta_kbd, &delta_unroute, &delta_total);
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
    if (kernel_string_equals(command, "rmsg-reset-stats")) {
        uint64_t pending_depth = 0;
        if (runtime_message_router_reset_stats(&pending_depth)) {
            serial_write("[INFO] smp: serial cmd rmsg-reset-stats reset=ok\n");
            console_write("[INFO] smp: serial cmd rmsg-reset-stats reset=ok\n");
        } else {
            char pending_depth_hex[19];
            kernel_format_hex64(pending_depth_hex, pending_depth);
            serial_write("[WARN] smp: serial cmd rmsg-reset-stats reset=blocked pending-depth=");
            serial_write(pending_depth_hex);
            serial_write("\n");
            console_write("[WARN] smp: serial cmd rmsg-reset-stats reset=blocked pending-depth=");
            console_write(pending_depth_hex);
            console_write("\n");
        }
        return;
    }
    if (kernel_string_equals(command, "rmsg-profile")) {
        char warn_depth_hex[19];
        char transition_ticks_hex[19];
        char pause_ticks_hex[19];
        char smoke_count_hex[19];
        kernel_format_hex64(warn_depth_hex, kernel_runtime_message_backlog_warn_depth());
        kernel_format_hex64(transition_ticks_hex, kernel_runtime_message_backlog_transition_ticks());
        kernel_format_hex64(pause_ticks_hex, g_runtime_backlog_test_pause_ticks);
        kernel_format_hex64(smoke_count_hex, g_runtime_smoke_extended_backlog_count);
        serial_write("[INFO] smp: serial cmd rmsg-profile current warn-depth/transition/pause/smoke=");
        serial_write(warn_depth_hex);
        serial_write("/");
        serial_write(transition_ticks_hex);
        serial_write("/");
        serial_write(pause_ticks_hex);
        serial_write("/");
        serial_write(smoke_count_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd rmsg-profile current warn-depth/transition/pause/smoke=");
        console_write(warn_depth_hex);
        console_write("/");
        console_write(transition_ticks_hex);
        console_write("/");
        console_write(pause_ticks_hex);
        console_write("/");
        console_write(smoke_count_hex);
        console_write("\n");
        return;
    }
    if (kernel_string_equals(command, "rmsg-verify")) {
        uint64_t failure_mask = 0;
        const bool pass = verify_runtime_message_router_consistency(&failure_mask);
        char failure_hex[19];
        kernel_format_hex64(failure_hex, failure_mask);
        serial_write(pass ? "[INFO] smp: serial cmd rmsg-verify pass fail-mask="
                          : "[WARN] smp: serial cmd rmsg-verify fail fail-mask=");
        serial_write(failure_hex);
        serial_write("\n");
        console_write(pass ? "[INFO] smp: serial cmd rmsg-verify pass fail-mask="
                           : "[WARN] smp: serial cmd rmsg-verify fail fail-mask=");
        console_write(failure_hex);
        console_write("\n");
        return;
    }
    if (kernel_string_starts_with(command, "rmsg-profile ")) {
        const char *profile = command + 13;
        if (kernel_string_equals(profile, "balanced")) {
            set_runtime_message_router_profile(16, 25, 300, 16);
        } else if (kernel_string_equals(profile, "stress")) {
            set_runtime_message_router_profile(8, 10, 450, 64);
        } else if (kernel_string_equals(profile, "quiet")) {
            set_runtime_message_router_profile(32, 50, 200, 16);
        } else {
            serial_write("[WARN] smp: serial cmd rmsg-profile expects balanced|stress|quiet\n");
            console_write("[WARN] smp: serial cmd rmsg-profile expects balanced|stress|quiet\n");
            return;
        }
        char warn_depth_hex[19];
        char transition_ticks_hex[19];
        char pause_ticks_hex[19];
        char smoke_count_hex[19];
        kernel_format_hex64(warn_depth_hex, kernel_runtime_message_backlog_warn_depth());
        kernel_format_hex64(transition_ticks_hex, kernel_runtime_message_backlog_transition_ticks());
        kernel_format_hex64(pause_ticks_hex, g_runtime_backlog_test_pause_ticks);
        kernel_format_hex64(smoke_count_hex, g_runtime_smoke_extended_backlog_count);
        serial_write("[INFO] smp: serial cmd rmsg-profile set warn-depth/transition/pause/smoke=");
        serial_write(warn_depth_hex);
        serial_write("/");
        serial_write(transition_ticks_hex);
        serial_write("/");
        serial_write(pause_ticks_hex);
        serial_write("/");
        serial_write(smoke_count_hex);
        serial_write("\n");
        console_write("[INFO] smp: serial cmd rmsg-profile set warn-depth/transition/pause/smoke=");
        console_write(warn_depth_hex);
        console_write("/");
        console_write(transition_ticks_hex);
        console_write("/");
        console_write(pause_ticks_hex);
        console_write("/");
        console_write(smoke_count_hex);
        console_write("\n");
        return;
    }
    if (kernel_string_equals(command, "rmsg-smoke")) {
        uint64_t pending_depth = 0;
        if (!runtime_message_router_reset_stats(&pending_depth)) {
            char pending_depth_hex[19];
            kernel_format_hex64(pending_depth_hex, pending_depth);
            serial_write("[WARN] smp: serial cmd rmsg-smoke fail reset=blocked pending-depth=");
            serial_write(pending_depth_hex);
            serial_write("\n");
            console_write("[WARN] smp: serial cmd rmsg-smoke fail reset=blocked pending-depth=");
            console_write(pending_depth_hex);
            console_write("\n");
            return;
        }

        uint64_t delta_src = 0;
        uint64_t delta_kbd = 0;
        uint64_t delta_unroute = 0;
        uint64_t delta_total = 0;
        const bool selftest_pass = run_runtime_message_router_selftest(
            &delta_src, &delta_kbd, &delta_unroute, &delta_total);
        char delta_src_hex[19];
        char delta_kbd_hex[19];
        char delta_unroute_hex[19];
        char delta_total_hex[19];
        kernel_format_hex64(delta_src_hex, delta_src);
        kernel_format_hex64(delta_kbd_hex, delta_kbd);
        kernel_format_hex64(delta_unroute_hex, delta_unroute);
        kernel_format_hex64(delta_total_hex, delta_total);
        serial_write(selftest_pass ? "[INFO] smp: serial cmd rmsg-smoke pass reset=ok selftest=pass delta(src/kbd/unroute/total)="
                                   : "[WARN] smp: serial cmd rmsg-smoke fail reset=ok selftest=fail delta(src/kbd/unroute/total)=");
        serial_write(delta_src_hex);
        serial_write("/");
        serial_write(delta_kbd_hex);
        serial_write("/");
        serial_write(delta_unroute_hex);
        serial_write("/");
        serial_write(delta_total_hex);
        serial_write("\n");
        console_write(selftest_pass ? "[INFO] smp: serial cmd rmsg-smoke pass reset=ok selftest=pass delta(src/kbd/unroute/total)="
                                    : "[WARN] smp: serial cmd rmsg-smoke fail reset=ok selftest=fail delta(src/kbd/unroute/total)=");
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
    if (kernel_string_equals(command, "rmsg-smoke-extended")) {
        uint64_t pending_depth = 0;
        if (!runtime_message_router_reset_stats(&pending_depth)) {
            char pending_depth_hex[19];
            kernel_format_hex64(pending_depth_hex, pending_depth);
            serial_write("[WARN] smp: serial cmd rmsg-smoke-extended fail reset=blocked pending-depth=");
            serial_write(pending_depth_hex);
            serial_write("\n");
            console_write("[WARN] smp: serial cmd rmsg-smoke-extended fail reset=blocked pending-depth=");
            console_write(pending_depth_hex);
            console_write("\n");
            return;
        }

        uint64_t delta_src = 0;
        uint64_t delta_kbd = 0;
        uint64_t delta_unroute = 0;
        uint64_t delta_total = 0;
        const bool selftest_pass = run_runtime_message_router_selftest(
            &delta_src, &delta_kbd, &delta_unroute, &delta_total);
        uint64_t input_enqueued = 0;
        uint64_t command_enqueued = 0;
        const bool backlog_pass = inject_runtime_message_router_backlog(
            g_runtime_smoke_extended_backlog_count,
            true,
            true,
            &input_enqueued,
            &command_enqueued);
        g_runtime_router_dequeue_pause_until_tick = timer_ticks() + g_runtime_backlog_test_pause_ticks;

        char delta_src_hex[19];
        char delta_kbd_hex[19];
        char delta_unroute_hex[19];
        char delta_total_hex[19];
        char smoke_count_hex[19];
        char input_enqueued_hex[19];
        char command_enqueued_hex[19];
        kernel_format_hex64(delta_src_hex, delta_src);
        kernel_format_hex64(delta_kbd_hex, delta_kbd);
        kernel_format_hex64(delta_unroute_hex, delta_unroute);
        kernel_format_hex64(delta_total_hex, delta_total);
        kernel_format_hex64(smoke_count_hex, g_runtime_smoke_extended_backlog_count);
        kernel_format_hex64(input_enqueued_hex, input_enqueued);
        kernel_format_hex64(command_enqueued_hex, command_enqueued);

        uint64_t verify_failure_mask = 0;
        const bool verify_pass = verify_runtime_message_router_consistency(&verify_failure_mask);
        char verify_failure_hex[19];
        kernel_format_hex64(verify_failure_hex, verify_failure_mask);
        const bool pass = selftest_pass && backlog_pass && verify_pass;
        serial_write(pass ? "[INFO] smp: serial cmd rmsg-smoke-extended pass reset=ok selftest=pass backlog(request/enq input/cmd)="
                          : "[WARN] smp: serial cmd rmsg-smoke-extended fail reset=ok selftest/backlog/verify=");
        if (!pass) {
            serial_write(selftest_pass ? "pass/" : "fail/");
            serial_write(backlog_pass ? "pass/" : "fail/");
            serial_write(verify_pass ? "pass " : "fail ");
            serial_write("backlog(request/enq input/cmd)=");
        }
        serial_write(smoke_count_hex);
        serial_write("/");
        serial_write(input_enqueued_hex);
        serial_write("/");
        serial_write(command_enqueued_hex);
        serial_write(" delta(src/kbd/unroute/total)=");
        serial_write(delta_src_hex);
        serial_write("/");
        serial_write(delta_kbd_hex);
        serial_write("/");
        serial_write(delta_unroute_hex);
        serial_write("/");
        serial_write(delta_total_hex);
        serial_write(" verify-fail-mask=");
        serial_write(verify_failure_hex);
        serial_write("\n");

        console_write(pass ? "[INFO] smp: serial cmd rmsg-smoke-extended pass reset=ok selftest=pass backlog(request/enq input/cmd)="
                           : "[WARN] smp: serial cmd rmsg-smoke-extended fail reset=ok selftest/backlog/verify=");
        if (!pass) {
            console_write(selftest_pass ? "pass/" : "fail/");
            console_write(backlog_pass ? "pass/" : "fail/");
            console_write(verify_pass ? "pass " : "fail ");
            console_write("backlog(request/enq input/cmd)=");
        }
        console_write(smoke_count_hex);
        console_write("/");
        console_write(input_enqueued_hex);
        console_write("/");
        console_write(command_enqueued_hex);
        console_write(" delta(src/kbd/unroute/total)=");
        console_write(delta_src_hex);
        console_write("/");
        console_write(delta_kbd_hex);
        console_write("/");
        console_write(delta_unroute_hex);
        console_write("/");
        console_write(delta_total_hex);
        console_write(" verify-fail-mask=");
        console_write(verify_failure_hex);
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
        inject_runtime_message_router_backlog(
            count, inject_input, inject_command, &input_enqueued, &command_enqueued);
        g_runtime_router_dequeue_pause_until_tick = timer_ticks() + g_runtime_backlog_test_pause_ticks;
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

    kernel_poll_filesystem_surface_runtime_tasks();

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
