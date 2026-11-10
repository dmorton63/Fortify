#include "fortify/systems/kernel/runtime_command_ap_stats.h"

#include "fortify/systems/kernel/ap_trigger_profile.h"
#include "fortify/systems/kernel/kernel_string.h"
#include "fortify/systems/kernel/runtime_message_router.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/subsystems/subsystems.h"

extern "C" volatile uint64_t g_ap_trigger_profile;

void kernel_log_ap_stats_snapshot(void) {
    constexpr uint64_t kSmpCounterCap = 64;
    constexpr uint64_t kMaxApDepthPrint = 8;
    const uint64_t online = smp_online_cpu_count();
    const uint64_t total = smp_total_cpu_count();
    const uint64_t ipi_observed = smp_ipi_kick_observed_ap_count();
    const uint64_t ipi_scheduler = scheduler_ipi_preempt_requests_total();
    const uint64_t ap_dispatch = scheduler_ap_work_dispatch_total();
    const uint64_t ap_execute = scheduler_ap_work_execute_total();
    const uint64_t drops = scheduler_ap_work_drop_total();
    const uint64_t skip_rate_limit = scheduler_ap_work_skip_rate_limit_total();
    const uint64_t skip_pending_cap = scheduler_ap_work_skip_pending_cap_total();
    const uint64_t ap_pinned_active = scheduler_ap_pinned_task_active_count();
    const uint64_t ap_pinned_execute = scheduler_ap_pinned_task_execute_total();
    const uint64_t ap_pinned_migrations = scheduler_ap_pinned_task_migration_total();
    const uint64_t ap_pinned_seen_cpu_mask = scheduler_ap_pinned_task_seen_cpu_mask();
    const uint64_t ap_to_ap_switch = scheduler_ap_to_ap_switch_total();
    const uint64_t bsp_to_ap_switch = scheduler_bsp_to_ap_switch_total();
    const uint64_t ap_worker_threads_provisioned = scheduler_ap_worker_threads_provisioned_total();
    const uint64_t ap_pinned_threads_provisioned = scheduler_ap_pinned_threads_provisioned_total();
    const uint64_t ap_threads_expected = scheduler_ap_threads_expected_total();
    const uint64_t trigger_profile = __atomic_load_n(&g_ap_trigger_profile, __ATOMIC_RELAXED);
    const uint64_t port_jobs = port_manager_ap_deferred_job_total();
    const uint64_t networking_jobs = networking_ap_deferred_job_total();
    const uint64_t networking_irq_total = networking_rx_irq_total();
    const uint64_t networking_irq_drop_total = networking_rx_irq_drop_total();
    const uint64_t networking_packets_total = networking_rx_packets_processed_total();
    const uint64_t networking_batches_total = networking_rx_batches_total();
    const uint64_t filesystem_jobs = filesystem_ap_deferred_job_total();
    const uint64_t filesystem_processed = filesystem_processed_events_total();
    const uint64_t filesystem_batches = filesystem_batches_total();
    const uint64_t filesystem_probe = filesystem_probe_total();
    const uint64_t filesystem_probe_fat16 = filesystem_probe_fat16_total();
    const uint64_t filesystem_probe_fat32 = filesystem_probe_fat32_total();
    const uint64_t filesystem_probe_ntfs = filesystem_probe_ntfs_total();
    const uint64_t filesystem_mount_attempt = filesystem_mount_attempt_total();
    const uint64_t filesystem_mount_success = filesystem_mount_success_total();
    const uint64_t filesystem_root_scan = filesystem_fat32_root_scan_total();
    const uint64_t filesystem_root_entries = filesystem_fat32_root_entry_total();
    const uint64_t filesystem_root_files = filesystem_fat32_root_file_total();
    const uint64_t filesystem_root_dirs = filesystem_fat32_root_dir_total();
    const uint64_t filesystem_open_attempt = filesystem_fat32_open_attempt_total();
    const uint64_t filesystem_open_success = filesystem_fat32_open_success_total();
    const uint64_t filesystem_read_attempt = filesystem_fat32_read_attempt_total();
    const uint64_t filesystem_read_success = filesystem_fat32_read_success_total();
    const uint64_t filesystem_read_bytes = filesystem_fat32_read_bytes_total();
    const uint64_t filesystem_sector_read = filesystem_sector_read_total();
    const uint64_t filesystem_sector_callback = filesystem_sector_read_callback_total();
    const uint64_t filesystem_sector_synthetic = filesystem_sector_read_synthetic_total();
    const uint64_t filesystem_sector_fail = filesystem_sector_read_fail_total();
    const uint64_t storage_irq_total = filesystem_storage_irq_total();
    const uint64_t storage_irq_drop_total = filesystem_storage_irq_drop_total();
    const uint64_t input_jobs = input_ap_deferred_job_total();
    const uint64_t input_irq_total = input_keyboard_irq_total();
    const uint64_t input_irq_drop_total = input_keyboard_irq_drop_total();
    const uint64_t usb_jobs = usb_ap_deferred_job_total();
    const uint64_t usb_processed = usb_processed_events_total();
    const uint64_t usb_batches = usb_batches_total();
    const uint64_t usb_irq_total = usb_host_irq_total();
    const uint64_t usb_irq_drop_total = usb_host_irq_drop_total();
    const uint64_t audio_jobs = audio_ap_deferred_job_total();
    const uint64_t audio_irq_total_count = audio_irq_total();
    const uint64_t audio_irq_drop_total_count = audio_irq_drop_total();
    const uint64_t webcam_jobs = webcam_ap_deferred_job_total();
    const uint64_t webcam_frame_total_count = webcam_frame_total();
    const uint64_t webcam_frame_drop_total_count = webcam_frame_drop_total();
    const uint64_t webaccess_submit_total = webaccess_head_request_submit_total();
    const uint64_t webaccess_done_total = webaccess_head_request_done_total();
    const uint64_t webaccess_done_ok_total = webaccess_head_request_done_ok_total();
    const uint64_t webaccess_done_err_total = webaccess_head_request_done_err_total();
    const uint64_t runtime_message_q_depth = runtime_message_router_queue_depth();
    const uint64_t runtime_message_q_high_water = runtime_message_router_queue_high_water();
    const uint64_t runtime_message_q_drop = runtime_message_router_drop_total();
    const uint64_t runtime_message_q_enq = runtime_message_router_enqueued_total();
    const uint64_t runtime_message_q_deq = runtime_message_router_dequeued_total();
    const uint64_t runtime_message_input_q_depth =
        runtime_message_router_queue_depth_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_message_input_q_high_water =
        runtime_message_router_queue_high_water_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_message_input_q_drop =
        runtime_message_router_drop_total_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_message_input_q_enq =
        runtime_message_router_enqueued_total_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_message_input_q_deq =
        runtime_message_router_dequeued_total_for_target(runtime_message_target::compositor_input);
    const uint64_t runtime_message_command_q_depth =
        runtime_message_router_queue_depth_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_message_command_q_high_water =
        runtime_message_router_queue_high_water_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_message_command_q_drop =
        runtime_message_router_drop_total_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_message_command_q_enq =
        runtime_message_router_enqueued_total_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_message_command_q_deq =
        runtime_message_router_dequeued_total_for_target(runtime_message_target::runtime_command);
    const uint64_t runtime_message_dispatch_input = runtime_message_router_dispatch_input_total();
    const uint64_t runtime_message_dispatch_command = runtime_message_router_dispatch_command_total();
    const uint64_t runtime_message_route_drop = runtime_message_router_route_drop_total();
    const uint64_t runtime_message_route_drop_invalid_source =
        runtime_message_router_route_drop_invalid_source_total();
    const uint64_t runtime_message_route_drop_keyboard_suppressed =
        runtime_message_router_route_drop_keyboard_suppressed_total();
    const uint64_t runtime_message_route_drop_unroutable =
        runtime_message_router_route_drop_unroutable_total();

    uint64_t queue_depth = 0;
    const uint64_t cpu_limit = (total < kSmpCounterCap) ? total : kSmpCounterCap;
    for (uint64_t i = 1; i < cpu_limit; i++) {
        queue_depth += scheduler_ap_work_queue_depth_for_cpu(i);
    }

    char online_hex[19];
    char total_hex[19];
    char ipi_observed_hex[19];
    char ipi_scheduler_hex[19];
    char dispatch_hex[19];
    char execute_hex[19];
    char queue_hex[19];
    char drops_hex[19];
    char skip_rl_hex[19];
    char skip_pc_hex[19];
    char pinned_active_hex[19];
    char pinned_execute_hex[19];
    char pinned_migrations_hex[19];
    char pinned_seen_cpu_mask_hex[19];
    char ap_to_ap_switch_hex[19];
    char bsp_to_ap_switch_hex[19];
    char ap_worker_threads_provisioned_hex[19];
    char ap_pinned_threads_provisioned_hex[19];
    char ap_threads_expected_hex[19];
    char profile_hex[19];
    char ports_hex[19];
    char net_hex[19];
    char networking_irq_hex[19];
    char networking_drop_hex[19];
    char networking_packets_hex[19];
    char networking_batches_hex[19];
    char fs_hex[19];
    char fs_processed_hex[19];
    char fs_batches_hex[19];
    char fs_probe_hex[19];
    char fs_probe_fat16_hex[19];
    char fs_probe_fat32_hex[19];
    char fs_probe_ntfs_hex[19];
    char fs_mount_attempt_hex[19];
    char fs_mount_success_hex[19];
    char fs_root_scan_hex[19];
    char fs_root_entries_hex[19];
    char fs_root_files_hex[19];
    char fs_root_dirs_hex[19];
    char fs_open_attempt_hex[19];
    char fs_open_success_hex[19];
    char fs_read_attempt_hex[19];
    char fs_read_success_hex[19];
    char fs_read_bytes_hex[19];
    char fs_sector_read_hex[19];
    char fs_sector_callback_hex[19];
    char fs_sector_synthetic_hex[19];
    char fs_sector_fail_hex[19];
    char storage_irq_hex[19];
    char storage_drop_hex[19];
    char input_hex[19];
    char input_irq_hex[19];
    char input_drop_hex[19];
    char usb_hex[19];
    char usb_processed_hex[19];
    char usb_batches_hex[19];
    char usb_irq_hex[19];
    char usb_drop_hex[19];
    char audio_hex[19];
    char audio_irq_hex[19];
    char audio_drop_hex[19];
    char webcam_hex[19];
    char webcam_frame_hex[19];
    char webcam_drop_hex[19];
    char webaccess_submit_hex[19];
    char webaccess_done_hex[19];
    char webaccess_done_ok_hex[19];
    char webaccess_done_err_hex[19];
    char runtime_message_q_depth_hex[19];
    char runtime_message_q_high_water_hex[19];
    char runtime_message_q_drop_hex[19];
    char runtime_message_q_enq_hex[19];
    char runtime_message_q_deq_hex[19];
    char runtime_message_input_q_depth_hex[19];
    char runtime_message_input_q_high_water_hex[19];
    char runtime_message_input_q_drop_hex[19];
    char runtime_message_input_q_enq_hex[19];
    char runtime_message_input_q_deq_hex[19];
    char runtime_message_command_q_depth_hex[19];
    char runtime_message_command_q_high_water_hex[19];
    char runtime_message_command_q_drop_hex[19];
    char runtime_message_command_q_enq_hex[19];
    char runtime_message_command_q_deq_hex[19];
    char runtime_message_dispatch_input_hex[19];
    char runtime_message_dispatch_command_hex[19];
    char runtime_message_route_drop_hex[19];
    char runtime_message_route_drop_invalid_source_hex[19];
    char runtime_message_route_drop_keyboard_suppressed_hex[19];
    char runtime_message_route_drop_unroutable_hex[19];
    kernel_format_hex64(online_hex, online);
    kernel_format_hex64(total_hex, total);
    kernel_format_hex64(ipi_observed_hex, ipi_observed);
    kernel_format_hex64(ipi_scheduler_hex, ipi_scheduler);
    kernel_format_hex64(dispatch_hex, ap_dispatch);
    kernel_format_hex64(execute_hex, ap_execute);
    kernel_format_hex64(queue_hex, queue_depth);
    kernel_format_hex64(drops_hex, drops);
    kernel_format_hex64(skip_rl_hex, skip_rate_limit);
    kernel_format_hex64(skip_pc_hex, skip_pending_cap);
    kernel_format_hex64(pinned_active_hex, ap_pinned_active);
    kernel_format_hex64(pinned_execute_hex, ap_pinned_execute);
    kernel_format_hex64(pinned_migrations_hex, ap_pinned_migrations);
    kernel_format_hex64(pinned_seen_cpu_mask_hex, ap_pinned_seen_cpu_mask);
    kernel_format_hex64(ap_to_ap_switch_hex, ap_to_ap_switch);
    kernel_format_hex64(bsp_to_ap_switch_hex, bsp_to_ap_switch);
    kernel_format_hex64(ap_worker_threads_provisioned_hex, ap_worker_threads_provisioned);
    kernel_format_hex64(ap_pinned_threads_provisioned_hex, ap_pinned_threads_provisioned);
    kernel_format_hex64(ap_threads_expected_hex, ap_threads_expected);
    kernel_format_hex64(profile_hex, trigger_profile);
    kernel_format_hex64(ports_hex, port_jobs);
    kernel_format_hex64(net_hex, networking_jobs);
    kernel_format_hex64(networking_irq_hex, networking_irq_total);
    kernel_format_hex64(networking_drop_hex, networking_irq_drop_total);
    kernel_format_hex64(networking_packets_hex, networking_packets_total);
    kernel_format_hex64(networking_batches_hex, networking_batches_total);
    kernel_format_hex64(fs_hex, filesystem_jobs);
    kernel_format_hex64(fs_processed_hex, filesystem_processed);
    kernel_format_hex64(fs_batches_hex, filesystem_batches);
    kernel_format_hex64(fs_probe_hex, filesystem_probe);
    kernel_format_hex64(fs_probe_fat16_hex, filesystem_probe_fat16);
    kernel_format_hex64(fs_probe_fat32_hex, filesystem_probe_fat32);
    kernel_format_hex64(fs_probe_ntfs_hex, filesystem_probe_ntfs);
    kernel_format_hex64(fs_mount_attempt_hex, filesystem_mount_attempt);
    kernel_format_hex64(fs_mount_success_hex, filesystem_mount_success);
    kernel_format_hex64(fs_root_scan_hex, filesystem_root_scan);
    kernel_format_hex64(fs_root_entries_hex, filesystem_root_entries);
    kernel_format_hex64(fs_root_files_hex, filesystem_root_files);
    kernel_format_hex64(fs_root_dirs_hex, filesystem_root_dirs);
    kernel_format_hex64(fs_open_attempt_hex, filesystem_open_attempt);
    kernel_format_hex64(fs_open_success_hex, filesystem_open_success);
    kernel_format_hex64(fs_read_attempt_hex, filesystem_read_attempt);
    kernel_format_hex64(fs_read_success_hex, filesystem_read_success);
    kernel_format_hex64(fs_read_bytes_hex, filesystem_read_bytes);
    kernel_format_hex64(fs_sector_read_hex, filesystem_sector_read);
    kernel_format_hex64(fs_sector_callback_hex, filesystem_sector_callback);
    kernel_format_hex64(fs_sector_synthetic_hex, filesystem_sector_synthetic);
    kernel_format_hex64(fs_sector_fail_hex, filesystem_sector_fail);
    kernel_format_hex64(storage_irq_hex, storage_irq_total);
    kernel_format_hex64(storage_drop_hex, storage_irq_drop_total);
    kernel_format_hex64(input_hex, input_jobs);
    kernel_format_hex64(input_irq_hex, input_irq_total);
    kernel_format_hex64(input_drop_hex, input_irq_drop_total);
    kernel_format_hex64(usb_hex, usb_jobs);
    kernel_format_hex64(usb_processed_hex, usb_processed);
    kernel_format_hex64(usb_batches_hex, usb_batches);
    kernel_format_hex64(usb_irq_hex, usb_irq_total);
    kernel_format_hex64(usb_drop_hex, usb_irq_drop_total);
    kernel_format_hex64(audio_hex, audio_jobs);
    kernel_format_hex64(audio_irq_hex, audio_irq_total_count);
    kernel_format_hex64(audio_drop_hex, audio_irq_drop_total_count);
    kernel_format_hex64(webcam_hex, webcam_jobs);
    kernel_format_hex64(webcam_frame_hex, webcam_frame_total_count);
    kernel_format_hex64(webcam_drop_hex, webcam_frame_drop_total_count);
    kernel_format_hex64(webaccess_submit_hex, webaccess_submit_total);
    kernel_format_hex64(webaccess_done_hex, webaccess_done_total);
    kernel_format_hex64(webaccess_done_ok_hex, webaccess_done_ok_total);
    kernel_format_hex64(webaccess_done_err_hex, webaccess_done_err_total);
    kernel_format_hex64(runtime_message_q_depth_hex, runtime_message_q_depth);
    kernel_format_hex64(runtime_message_q_high_water_hex, runtime_message_q_high_water);
    kernel_format_hex64(runtime_message_q_drop_hex, runtime_message_q_drop);
    kernel_format_hex64(runtime_message_q_enq_hex, runtime_message_q_enq);
    kernel_format_hex64(runtime_message_q_deq_hex, runtime_message_q_deq);
    kernel_format_hex64(runtime_message_input_q_depth_hex, runtime_message_input_q_depth);
    kernel_format_hex64(runtime_message_input_q_high_water_hex, runtime_message_input_q_high_water);
    kernel_format_hex64(runtime_message_input_q_drop_hex, runtime_message_input_q_drop);
    kernel_format_hex64(runtime_message_input_q_enq_hex, runtime_message_input_q_enq);
    kernel_format_hex64(runtime_message_input_q_deq_hex, runtime_message_input_q_deq);
    kernel_format_hex64(runtime_message_command_q_depth_hex, runtime_message_command_q_depth);
    kernel_format_hex64(runtime_message_command_q_high_water_hex, runtime_message_command_q_high_water);
    kernel_format_hex64(runtime_message_command_q_drop_hex, runtime_message_command_q_drop);
    kernel_format_hex64(runtime_message_command_q_enq_hex, runtime_message_command_q_enq);
    kernel_format_hex64(runtime_message_command_q_deq_hex, runtime_message_command_q_deq);
    kernel_format_hex64(runtime_message_dispatch_input_hex, runtime_message_dispatch_input);
    kernel_format_hex64(runtime_message_dispatch_command_hex, runtime_message_dispatch_command);
    kernel_format_hex64(runtime_message_route_drop_hex, runtime_message_route_drop);
    kernel_format_hex64(runtime_message_route_drop_invalid_source_hex,
                        runtime_message_route_drop_invalid_source);
    kernel_format_hex64(runtime_message_route_drop_keyboard_suppressed_hex,
                        runtime_message_route_drop_keyboard_suppressed);
    kernel_format_hex64(runtime_message_route_drop_unroutable_hex,
                        runtime_message_route_drop_unroutable);
    const char *profile_name = ap_trigger_profile_name(trigger_profile);

    serial_write("[INFO] smp: serial cmd ap-stats online=");
    serial_write(online_hex);
    serial_write("/");
    serial_write(total_hex);
    serial_write(" ipi(obs/req)=");
    serial_write(ipi_observed_hex);
    serial_write("/");
    serial_write(ipi_scheduler_hex);
    serial_write(" work(d/e)=");
    serial_write(dispatch_hex);
    serial_write("/");
    serial_write(execute_hex);
    serial_write(" q=");
    serial_write(queue_hex);
    serial_write(" drop=");
    serial_write(drops_hex);
    serial_write(" skip(rl/pc)=");
    serial_write(skip_rl_hex);
    serial_write("/");
    serial_write(skip_pc_hex);
    serial_write(" ap-task(act/exec)=");
    serial_write(pinned_active_hex);
    serial_write("/");
    serial_write(pinned_execute_hex);
    serial_write(" mig/mask=");
    serial_write(pinned_migrations_hex);
    serial_write("/");
    serial_write(pinned_seen_cpu_mask_hex);
    serial_write(" ctx(ap2ap/bsp2ap)=");
    serial_write(ap_to_ap_switch_hex);
    serial_write("/");
    serial_write(bsp_to_ap_switch_hex);
    serial_write(" ap-thread(worker/pinned/expected)=");
    serial_write(ap_worker_threads_provisioned_hex);
    serial_write("/");
    serial_write(ap_pinned_threads_provisioned_hex);
    serial_write("/");
    serial_write(ap_threads_expected_hex);
    serial_write(" profile=");
    serial_write(profile_hex);
    serial_write("/");
    serial_write(profile_name);
    serial_write(" subsys(p/n/f/i/u/a/w)=");
    serial_write(ports_hex);
    serial_write("/");
    serial_write(net_hex);
    serial_write("/");
    serial_write(fs_hex);
    serial_write("/");
    serial_write(input_hex);
    serial_write("/");
    serial_write(usb_hex);
    serial_write("/");
    serial_write(audio_hex);
    serial_write("/");
    serial_write(webcam_hex);
    serial_write(" net(irq/drop)=");
    serial_write(networking_irq_hex);
    serial_write("/");
    serial_write(networking_drop_hex);
    serial_write(" net(proc/batch)=");
    serial_write(networking_packets_hex);
    serial_write("/");
    serial_write(networking_batches_hex);
    serial_write(" storage(irq/drop)=");
    serial_write(storage_irq_hex);
    serial_write("/");
    serial_write(storage_drop_hex);
    serial_write(" storage(proc/batch)=");
    serial_write(fs_processed_hex);
    serial_write("/");
    serial_write(fs_batches_hex);
    serial_write(" fs(probe/mnt/succ f16/f32/ntfs)=");
    serial_write(fs_probe_hex);
    serial_write("/");
    serial_write(fs_mount_attempt_hex);
    serial_write("/");
    serial_write(fs_mount_success_hex);
    serial_write(" ");
    serial_write(fs_probe_fat16_hex);
    serial_write("/");
    serial_write(fs_probe_fat32_hex);
    serial_write("/");
    serial_write(fs_probe_ntfs_hex);
    serial_write(" fs(root scan/entries files/dirs)=");
    serial_write(fs_root_scan_hex);
    serial_write("/");
    serial_write(fs_root_entries_hex);
    serial_write(" ");
    serial_write(fs_root_files_hex);
    serial_write("/");
    serial_write(fs_root_dirs_hex);
    serial_write(" fs(open/read bytes)=");
    serial_write(fs_open_attempt_hex);
    serial_write("/");
    serial_write(fs_open_success_hex);
    serial_write(" ");
    serial_write(fs_read_attempt_hex);
    serial_write("/");
    serial_write(fs_read_success_hex);
    serial_write(" ");
    serial_write(fs_read_bytes_hex);
    serial_write(" fs(sector total cb/syn/fail)=");
    serial_write(fs_sector_read_hex);
    serial_write(" ");
    serial_write(fs_sector_callback_hex);
    serial_write("/");
    serial_write(fs_sector_synthetic_hex);
    serial_write("/");
    serial_write(fs_sector_fail_hex);
    serial_write(" input(irq/drop)=");
    serial_write(input_irq_hex);
    serial_write("/");
    serial_write(input_drop_hex);
    serial_write(" usb(irq/drop)=");
    serial_write(usb_irq_hex);
    serial_write("/");
    serial_write(usb_drop_hex);
    serial_write(" usb(proc/batch)=");
    serial_write(usb_processed_hex);
    serial_write("/");
    serial_write(usb_batches_hex);
    serial_write(" audio(irq/drop)=");
    serial_write(audio_irq_hex);
    serial_write("/");
    serial_write(audio_drop_hex);
    serial_write(" webcam(frame/drop)=");
    serial_write(webcam_frame_hex);
    serial_write("/");
    serial_write(webcam_drop_hex);
    serial_write(" web(sub/done ok/err)=");
    serial_write(webaccess_submit_hex);
    serial_write("/");
    serial_write(webaccess_done_hex);
    serial_write(" ");
    serial_write(webaccess_done_ok_hex);
    serial_write("/");
    serial_write(webaccess_done_err_hex);
    serial_write(" rmsg-q(depth/hwm/drop)=");
    serial_write(runtime_message_q_depth_hex);
    serial_write("/");
    serial_write(runtime_message_q_high_water_hex);
    serial_write("/");
    serial_write(runtime_message_q_drop_hex);
    serial_write(" rmsg-flow(enq/deq in/cmd/drop)=");
    serial_write(runtime_message_q_enq_hex);
    serial_write("/");
    serial_write(runtime_message_q_deq_hex);
    serial_write(" ");
    serial_write(runtime_message_dispatch_input_hex);
    serial_write("/");
    serial_write(runtime_message_dispatch_command_hex);
    serial_write("/");
    serial_write(runtime_message_route_drop_hex);
    serial_write(" rmsg-drop-reason(src/kbd/unroute)=");
    serial_write(runtime_message_route_drop_invalid_source_hex);
    serial_write("/");
    serial_write(runtime_message_route_drop_keyboard_suppressed_hex);
    serial_write("/");
    serial_write(runtime_message_route_drop_unroutable_hex);
    serial_write(" rmsg-target-q(input d/h/d=");
    serial_write(runtime_message_input_q_depth_hex);
    serial_write("/");
    serial_write(runtime_message_input_q_high_water_hex);
    serial_write("/");
    serial_write(runtime_message_input_q_drop_hex);
    serial_write(" cmd d/h/d=");
    serial_write(runtime_message_command_q_depth_hex);
    serial_write("/");
    serial_write(runtime_message_command_q_high_water_hex);
    serial_write("/");
    serial_write(runtime_message_command_q_drop_hex);
    serial_write(")");
    serial_write(" rmsg-target-flow(input e/d=");
    serial_write(runtime_message_input_q_enq_hex);
    serial_write("/");
    serial_write(runtime_message_input_q_deq_hex);
    serial_write(" cmd e/d=");
    serial_write(runtime_message_command_q_enq_hex);
    serial_write("/");
    serial_write(runtime_message_command_q_deq_hex);
    serial_write(")");
    serial_write(" input-q[");
    bool wrote_cpu_depth = false;
    uint64_t printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, input_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            serial_write("/");
        }
        serial_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        serial_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        serial_write("/...");
    }
    serial_write("] net-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, networking_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            serial_write("/");
        }
        serial_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        serial_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        serial_write("/...");
    }
    serial_write("] fs-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, filesystem_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            serial_write("/");
        }
        serial_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        serial_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        serial_write("/...");
    }
    serial_write("] usb-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, usb_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            serial_write("/");
        }
        serial_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        serial_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        serial_write("/...");
    }
    serial_write("] audio-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, audio_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            serial_write("/");
        }
        serial_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        serial_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        serial_write("/...");
    }
    serial_write("] webcam-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, webcam_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            serial_write("/");
        }
        serial_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        serial_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        serial_write("/...");
    }
    serial_write("] ap-task-exec[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char exec_hex[19];
        kernel_format_hex64(exec_hex, scheduler_ap_pinned_task_execute_for_cpu(cpu));
        if (wrote_cpu_depth) {
            serial_write("/");
        }
        serial_write(exec_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        serial_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        serial_write("/...");
    }
    serial_write("]");
    serial_write("\n");

    console_write("[INFO] smp: serial cmd ap-stats online=");
    console_write(online_hex);
    console_write("/");
    console_write(total_hex);
    console_write(" ipi(obs/req)=");
    console_write(ipi_observed_hex);
    console_write("/");
    console_write(ipi_scheduler_hex);
    console_write(" work(d/e)=");
    console_write(dispatch_hex);
    console_write("/");
    console_write(execute_hex);
    console_write(" q=");
    console_write(queue_hex);
    console_write(" drop=");
    console_write(drops_hex);
    console_write(" skip(rl/pc)=");
    console_write(skip_rl_hex);
    console_write("/");
    console_write(skip_pc_hex);
    console_write(" ap-task(act/exec)=");
    console_write(pinned_active_hex);
    console_write("/");
    console_write(pinned_execute_hex);
    console_write(" mig/mask=");
    console_write(pinned_migrations_hex);
    console_write("/");
    console_write(pinned_seen_cpu_mask_hex);
    console_write(" ctx(ap2ap/bsp2ap)=");
    console_write(ap_to_ap_switch_hex);
    console_write("/");
    console_write(bsp_to_ap_switch_hex);
    console_write(" ap-thread(worker/pinned/expected)=");
    console_write(ap_worker_threads_provisioned_hex);
    console_write("/");
    console_write(ap_pinned_threads_provisioned_hex);
    console_write("/");
    console_write(ap_threads_expected_hex);
    console_write(" profile=");
    console_write(profile_hex);
    console_write("/");
    console_write(profile_name);
    console_write(" subsys(p/n/f/i/u/a/w)=");
    console_write(ports_hex);
    console_write("/");
    console_write(net_hex);
    console_write("/");
    console_write(fs_hex);
    console_write("/");
    console_write(input_hex);
    console_write("/");
    console_write(usb_hex);
    console_write("/");
    console_write(audio_hex);
    console_write("/");
    console_write(webcam_hex);
    console_write(" net(irq/drop)=");
    console_write(networking_irq_hex);
    console_write("/");
    console_write(networking_drop_hex);
    console_write(" net(proc/batch)=");
    console_write(networking_packets_hex);
    console_write("/");
    console_write(networking_batches_hex);
    console_write(" storage(irq/drop)=");
    console_write(storage_irq_hex);
    console_write("/");
    console_write(storage_drop_hex);
    console_write(" storage(proc/batch)=");
    console_write(fs_processed_hex);
    console_write("/");
    console_write(fs_batches_hex);
    console_write(" fs(probe/mnt/succ f16/f32/ntfs)=");
    console_write(fs_probe_hex);
    console_write("/");
    console_write(fs_mount_attempt_hex);
    console_write("/");
    console_write(fs_mount_success_hex);
    console_write(" ");
    console_write(fs_probe_fat16_hex);
    console_write("/");
    console_write(fs_probe_fat32_hex);
    console_write("/");
    console_write(fs_probe_ntfs_hex);
    console_write(" fs(root scan/entries files/dirs)=");
    console_write(fs_root_scan_hex);
    console_write("/");
    console_write(fs_root_entries_hex);
    console_write(" ");
    console_write(fs_root_files_hex);
    console_write("/");
    console_write(fs_root_dirs_hex);
    console_write(" fs(open/read bytes)=");
    console_write(fs_open_attempt_hex);
    console_write("/");
    console_write(fs_open_success_hex);
    console_write(" ");
    console_write(fs_read_attempt_hex);
    console_write("/");
    console_write(fs_read_success_hex);
    console_write(" ");
    console_write(fs_read_bytes_hex);
    console_write(" fs(sector total cb/syn/fail)=");
    console_write(fs_sector_read_hex);
    console_write(" ");
    console_write(fs_sector_callback_hex);
    console_write("/");
    console_write(fs_sector_synthetic_hex);
    console_write("/");
    console_write(fs_sector_fail_hex);
    console_write(" input(irq/drop)=");
    console_write(input_irq_hex);
    console_write("/");
    console_write(input_drop_hex);
    console_write(" usb(irq/drop)=");
    console_write(usb_irq_hex);
    console_write("/");
    console_write(usb_drop_hex);
    console_write(" usb(proc/batch)=");
    console_write(usb_processed_hex);
    console_write("/");
    console_write(usb_batches_hex);
    console_write(" audio(irq/drop)=");
    console_write(audio_irq_hex);
    console_write("/");
    console_write(audio_drop_hex);
    console_write(" webcam(frame/drop)=");
    console_write(webcam_frame_hex);
    console_write("/");
    console_write(webcam_drop_hex);
    console_write(" web(sub/done ok/err)=");
    console_write(webaccess_submit_hex);
    console_write("/");
    console_write(webaccess_done_hex);
    console_write(" ");
    console_write(webaccess_done_ok_hex);
    console_write("/");
    console_write(webaccess_done_err_hex);
    console_write(" rmsg-q(depth/hwm/drop)=");
    console_write(runtime_message_q_depth_hex);
    console_write("/");
    console_write(runtime_message_q_high_water_hex);
    console_write("/");
    console_write(runtime_message_q_drop_hex);
    console_write(" rmsg-flow(enq/deq in/cmd/drop)=");
    console_write(runtime_message_q_enq_hex);
    console_write("/");
    console_write(runtime_message_q_deq_hex);
    console_write(" ");
    console_write(runtime_message_dispatch_input_hex);
    console_write("/");
    console_write(runtime_message_dispatch_command_hex);
    console_write("/");
    console_write(runtime_message_route_drop_hex);
    console_write(" rmsg-drop-reason(src/kbd/unroute)=");
    console_write(runtime_message_route_drop_invalid_source_hex);
    console_write("/");
    console_write(runtime_message_route_drop_keyboard_suppressed_hex);
    console_write("/");
    console_write(runtime_message_route_drop_unroutable_hex);
    console_write(" rmsg-target-q(input d/h/d=");
    console_write(runtime_message_input_q_depth_hex);
    console_write("/");
    console_write(runtime_message_input_q_high_water_hex);
    console_write("/");
    console_write(runtime_message_input_q_drop_hex);
    console_write(" cmd d/h/d=");
    console_write(runtime_message_command_q_depth_hex);
    console_write("/");
    console_write(runtime_message_command_q_high_water_hex);
    console_write("/");
    console_write(runtime_message_command_q_drop_hex);
    console_write(")");
    console_write(" rmsg-target-flow(input e/d=");
    console_write(runtime_message_input_q_enq_hex);
    console_write("/");
    console_write(runtime_message_input_q_deq_hex);
    console_write(" cmd e/d=");
    console_write(runtime_message_command_q_enq_hex);
    console_write("/");
    console_write(runtime_message_command_q_deq_hex);
    console_write(")");
    console_write(" input-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, input_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            console_write("/");
        }
        console_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        console_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        console_write("/...");
    }
    console_write("] net-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, networking_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            console_write("/");
        }
        console_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        console_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        console_write("/...");
    }
    console_write("] fs-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, filesystem_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            console_write("/");
        }
        console_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        console_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        console_write("/...");
    }
    console_write("] usb-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, usb_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            console_write("/");
        }
        console_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        console_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        console_write("/...");
    }
    console_write("] audio-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, audio_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            console_write("/");
        }
        console_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        console_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        console_write("/...");
    }
    console_write("] webcam-q[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char depth_hex[19];
        kernel_format_hex64(depth_hex, webcam_queue_depth_for_cpu(cpu));
        if (wrote_cpu_depth) {
            console_write("/");
        }
        console_write(depth_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        console_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        console_write("/...");
    }
    console_write("] ap-task-exec[");
    wrote_cpu_depth = false;
    printed_ap = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        if (printed_ap >= kMaxApDepthPrint) {
            break;
        }
        char exec_hex[19];
        kernel_format_hex64(exec_hex, scheduler_ap_pinned_task_execute_for_cpu(cpu));
        if (wrote_cpu_depth) {
            console_write("/");
        }
        console_write(exec_hex);
        wrote_cpu_depth = true;
        printed_ap++;
    }
    if (!wrote_cpu_depth) {
        console_write("none");
    }
    if (cpu_limit > (1 + printed_ap)) {
        console_write("/...");
    }
    console_write("]");
    console_write("\n");

}
