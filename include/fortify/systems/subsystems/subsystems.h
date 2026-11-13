#ifndef FORTIFY_SUBSYSTEMS_H
#define FORTIFY_SUBSYSTEMS_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*filesystem_sector_reader_t)(uint8_t source, uint32_t lba, uint8_t *out_sector);

struct filesystem_root_entry {
    char name_8dot3[13];
    bool is_directory;
    uint32_t size_bytes;
};

enum class surface_type : uint8_t {
    desktop = 0,
    application = 1,
    document = 2,
    workspace = 3,
    dashboard = 4,
    tool = 5,
};

enum class surface_state : uint8_t {
    floating = 0,
    docked = 1,
    anchored = 2,
    fullscreen = 3,
    collapsed = 4,
    background = 5,
    modal = 6,
    panel = 7,
};

enum class surface_event_type : uint8_t {
    key_char = 0,
    key_backspace = 1,
    key_submit = 2,
};

enum class input_message_type : uint8_t {
    key_char = 0,
    key_backspace = 1,
    key_submit = 2,
};

enum class input_message_source : uint8_t {
    keyboard_irq = 0,
};

struct input_message {
    input_message_type type;
    char value;
    uint8_t raw_scancode;
    uint8_t route_cpu;
    uint8_t modifiers;
    input_message_source source;
    uint64_t tick;
    uint64_t sequence;
};

struct surface_info {
    uint64_t id;
    surface_type type;
    surface_state state;
    uint8_t layer;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint64_t capabilities;
    char title[32];
};

bool memory_init(void);
bool interrupts_init(void);
void interrupts_load_idt_for_current_cpu(void);
uint64_t timer_ticks(void);
bool scheduler_init(void);
void scheduler_start(void);
bool scheduler_service_deferred_switch(void);
bool smp_init(void);
uint64_t smp_online_cpu_count(void);
uint64_t smp_total_cpu_count(void);
uint64_t smp_ap_interruptible_ready_count(void);
bool smp_kick_all_aps(void);
uint64_t smp_ipi_kick_count(void);
uint64_t smp_ipi_kick_last_mode(void);
uint8_t smp_ipi_kick_vector(void);
void smp_note_ipi_kick_interrupt(void);
void smp_acknowledge_ipi_interrupt(void);
void smp_note_ap_nonkick_interrupt(uint8_t vector);
uint64_t smp_ipi_kick_observed_count(void);
uint64_t smp_ipi_kick_observed_ap_count(void);
uint64_t smp_ipi_kick_diag_mode(void);
uint64_t smp_ipi_kick_diag_path(void);
uint64_t smp_ipi_kick_diag_target(void);
uint64_t smp_ipi_kick_diag_icr_low(void);
uint64_t smp_ipi_kick_diag_icr_high(void);
uint64_t smp_ipi_kick_diag_esr_before(void);
uint64_t smp_ipi_kick_diag_esr_after(void);
uint64_t smp_ipi_kick_diag_result(void);
uint64_t smp_ap_nonkick_interrupt_count(void);
uint64_t smp_ap_wake_count_total(void);
uint64_t smp_ap_wake_count_for_cpu(uint64_t cpu_index);
bool usb_init(void);
void usb_on_host_irq(void);
uint64_t usb_inject_events(uint64_t count);
bool usb_poll_ap_processing_trigger(uint64_t cpu_index, uint64_t tick);
bool usb_set_ap_processing_trigger_config(uint64_t poll_stride_ticks, uint64_t queue_trigger);
void usb_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence);
uint64_t usb_host_irq_total(void);
uint64_t usb_host_irq_drop_total(void);
uint64_t usb_ap_deferred_job_total(void);
uint64_t usb_ap_deferred_job_for_cpu(uint64_t cpu_index);
uint64_t usb_processed_events_total(void);
uint64_t usb_processed_irq_events_total(void);
uint64_t usb_processed_injected_events_total(void);
uint64_t usb_batches_total(void);
uint64_t usb_queue_depth_for_cpu(uint64_t cpu_index);
uint64_t usb_queue_high_water_for_cpu(uint64_t cpu_index);
uint64_t usb_queue_high_water_total(void);
bool input_init(void);
void input_on_keyboard_irq(void);
bool input_read_char(char *out_char);
bool input_read_message(input_message *out_message);
bool input_poll_ap_processing_trigger(uint64_t cpu_index, uint64_t tick);
bool input_set_ap_processing_trigger_config(uint64_t poll_stride_ticks, uint64_t queue_trigger);
void input_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence);
uint64_t input_keyboard_irq_total(void);
uint64_t input_keyboard_irq_drop_total(void);
uint64_t input_ap_deferred_job_total(void);
uint64_t input_queue_depth_for_cpu(uint64_t cpu_index);
bool filesystem_init(void);
void filesystem_on_storage_irq(void);
uint64_t filesystem_inject_storage_events(uint64_t count);
bool filesystem_request_ap_maintenance(uint64_t cpu_index);
bool filesystem_poll_ap_maintenance_trigger(uint64_t cpu_index, uint64_t tick);
bool filesystem_set_ap_maintenance_trigger_config(uint64_t poll_stride_ticks, uint64_t dirty_pages_trigger);
void filesystem_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence);
uint64_t filesystem_ap_deferred_job_total(void);
uint64_t filesystem_ap_deferred_job_for_cpu(uint64_t cpu_index);
uint64_t filesystem_storage_irq_total(void);
uint64_t filesystem_storage_irq_drop_total(void);
uint64_t filesystem_queue_depth_for_cpu(uint64_t cpu_index);
uint64_t filesystem_queue_high_water_for_cpu(uint64_t cpu_index);
uint64_t filesystem_queue_high_water_total(void);
uint64_t filesystem_processed_events_total(void);
uint64_t filesystem_processed_irq_events_total(void);
uint64_t filesystem_processed_injected_events_total(void);
uint64_t filesystem_batches_total(void);
uint64_t filesystem_probe_total(void);
uint64_t filesystem_probe_fat16_total(void);
uint64_t filesystem_probe_fat32_total(void);
uint64_t filesystem_probe_ntfs_total(void);
uint64_t filesystem_mount_attempt_total(void);
uint64_t filesystem_mount_success_total(void);
uint64_t filesystem_fat32_root_scan_total(void);
uint64_t filesystem_fat32_root_entry_total(void);
uint64_t filesystem_fat32_root_file_total(void);
uint64_t filesystem_fat32_root_dir_total(void);
uint64_t filesystem_fat32_open_attempt_total(void);
uint64_t filesystem_fat32_open_success_total(void);
uint64_t filesystem_fat32_read_attempt_total(void);
uint64_t filesystem_fat32_read_success_total(void);
uint64_t filesystem_fat32_read_bytes_total(void);
bool filesystem_fat32_list_root(uint8_t source,
                                filesystem_root_entry *out_entries,
                                uint64_t max_entries,
                                uint64_t *out_count);
bool filesystem_fat32_read_root_file_head(uint8_t source,
                                          const char *name_8dot3,
                                          uint64_t max_bytes,
                                          uint64_t *out_bytes,
                                          uint64_t *out_checksum);
void filesystem_set_sector_reader(filesystem_sector_reader_t reader);
bool filesystem_has_sector_reader(void);
uint64_t filesystem_sector_read_total(void);
uint64_t filesystem_sector_read_callback_total(void);
uint64_t filesystem_sector_read_synthetic_total(void);
uint64_t filesystem_sector_read_fail_total(void);
bool filesystem_probe_virtio_9p(void);
uint64_t filesystem_virtio_9p_probe_total(void);
uint64_t filesystem_virtio_9p_device_total(void);
uint64_t filesystem_virtio_9p_last_bus(void);
uint64_t filesystem_virtio_9p_last_slot(void);
uint64_t filesystem_virtio_9p_last_function(void);
uint64_t filesystem_virtio_9p_last_device_id(void);
bool filesystem_virtio_9p_head_request_submit(const char *path,
                                              uint64_t max_bytes,
                                              uint64_t *out_request_id);
bool filesystem_virtio_9p_head_request_status(uint64_t request_id,
                                              uint64_t *out_state,
                                              uint64_t *out_bytes,
                                              uint64_t *out_checksum);
uint64_t filesystem_virtio_9p_head_request_submit_total(void);
uint64_t filesystem_virtio_9p_head_request_done_total(void);
uint64_t filesystem_virtio_9p_head_request_done_ok_total(void);
uint64_t filesystem_virtio_9p_head_request_done_err_total(void);
uint64_t filesystem_virtio_9p_head_request_pending_total(void);
uint64_t filesystem_virtio_9p_head_request_running_total(void);
uint64_t filesystem_virtio_9p_head_request_claimed_total(void);
uint64_t filesystem_virtio_9p_head_request_done_bridge_ok_total(void);
uint64_t filesystem_virtio_9p_head_request_done_bridge_err_total(void);
uint64_t filesystem_virtio_9p_head_request_done_worker_ok_total(void);
uint64_t filesystem_virtio_9p_head_request_done_worker_err_total(void);
uint64_t filesystem_virtio_9p_head_request_enqueue_worker_total(void);
uint64_t filesystem_virtio_9p_head_request_enqueue_worker_fail_total(void);
uint64_t filesystem_virtio_9p_mode_blocked_total(void);
bool filesystem_virtio_9p_head_request_reset_stats(uint64_t *active_requests);
bool filesystem_set_virtio_9p_share_experimental_mode(bool enabled);
bool filesystem_virtio_9p_share_experimental_mode_enabled(void);
bool port_manager_init(void);
bool port_manager_request_ap_maintenance(uint64_t cpu_index);
bool port_manager_poll_ap_maintenance_trigger(uint64_t cpu_index, uint64_t tick);
bool port_manager_set_ap_maintenance_trigger_config(uint64_t poll_stride_ticks, uint64_t deferred_jobs_trigger);
void port_manager_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence);
uint64_t port_manager_ap_deferred_job_total(void);
uint64_t port_manager_ap_deferred_job_for_cpu(uint64_t cpu_index);
bool video_init(void);
bool renderer_init(void);
bool compositor_init(void);
void video_render_tick(uint64_t tick);
bool video_set_demo_enabled(bool enabled);
bool video_demo_enabled(void);
bool video_set_render_style(uint64_t style);
uint64_t video_render_style(void);
bool video_set_spin_step(uint64_t step);
uint64_t video_spin_step(void);
bool video_set_font_style(uint64_t style);
uint64_t video_font_style(void);
bool video_set_text_scale(uint64_t scale);
uint64_t video_text_scale(void);
bool video_toggle_cube_easter_mode(bool *out_enabled);
bool video_cube_easter_mode(void);
bool video_trigger_cube_easter_pulse(uint64_t duration_ticks);
bool video_set_input_surface_draft(const char *draft_text);
bool video_set_command_center_last_command(const char *command_text);
bool video_set_command_center_status(const char *status_text);
bool compositor_route_event(surface_event_type type, char value);
bool compositor_take_submitted_command(char *out_command, uint64_t out_size);
bool compositor_surface_create(surface_type type,
                               const char *title,
                               uint64_t capabilities,
                               uint64_t *out_surface_id);
uint64_t compositor_surface_count(void);
bool compositor_surface_get(uint64_t index, surface_info *out_info);
bool networking_init(void);
bool webaccess_init(void);
bool webaccess_head_request_submit(const char *target, uint64_t max_bytes, uint64_t *out_request_id);
bool webaccess_head_request_status(uint64_t request_id,
                                   uint64_t *out_state,
                                   uint64_t *out_bytes,
                                   uint64_t *out_checksum);
uint64_t webaccess_head_request_submit_total(void);
uint64_t webaccess_head_request_done_total(void);
uint64_t webaccess_head_request_done_ok_total(void);
uint64_t webaccess_head_request_done_err_total(void);
uint64_t webaccess_head_request_pending_total(void);
uint64_t webaccess_head_request_running_total(void);
void networking_on_rx_irq(void);
uint64_t networking_inject_rx_events(uint64_t count);
bool networking_request_ap_buffer_cleanup(uint64_t cpu_index);
bool networking_poll_ap_cleanup_trigger(uint64_t cpu_index, uint64_t tick);
bool networking_set_ap_cleanup_trigger_config(uint64_t poll_stride_ticks, uint64_t backlog_trigger);
void networking_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence);
uint64_t networking_ap_deferred_job_total(void);
uint64_t networking_ap_deferred_job_for_cpu(uint64_t cpu_index);
uint64_t networking_rx_irq_total(void);
uint64_t networking_rx_irq_drop_total(void);
uint64_t networking_queue_depth_for_cpu(uint64_t cpu_index);
uint64_t networking_queue_high_water_for_cpu(uint64_t cpu_index);
uint64_t networking_queue_high_water_total(void);
uint64_t networking_rx_packets_processed_total(void);
uint64_t networking_rx_packets_processed_irq_total(void);
uint64_t networking_rx_packets_processed_injected_total(void);
uint64_t networking_rx_batches_total(void);
uint64_t networking_rx_packets_processed_for_cpu(uint64_t cpu_index);
uint64_t networking_rx_batches_for_cpu(uint64_t cpu_index);
bool webcam_init(void);
uint64_t webcam_inject_frames(uint64_t count);
bool webcam_poll_ap_processing_trigger(uint64_t cpu_index, uint64_t tick);
bool webcam_set_ap_processing_trigger_config(uint64_t poll_stride_ticks, uint64_t queue_trigger);
void webcam_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence);
uint64_t webcam_frame_total(void);
uint64_t webcam_frame_drop_total(void);
uint64_t webcam_ap_deferred_job_total(void);
uint64_t webcam_ap_deferred_job_for_cpu(uint64_t cpu_index);
uint64_t webcam_queue_depth_for_cpu(uint64_t cpu_index);
uint64_t webcam_queue_high_water_for_cpu(uint64_t cpu_index);
uint64_t webcam_queue_high_water_total(void);
uint64_t webcam_processed_events_total(void);
uint64_t webcam_processed_irq_events_total(void);
uint64_t webcam_processed_injected_events_total(void);
uint64_t webcam_batches_total(void);
bool audio_init(void);
void audio_on_irq(void);
uint64_t audio_inject_events(uint64_t count);
bool audio_poll_ap_buffer_trigger(uint64_t cpu_index, uint64_t tick);
bool audio_set_ap_buffer_trigger_config(uint64_t poll_stride_ticks, uint64_t queue_trigger);
void audio_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence);
uint64_t audio_irq_total(void);
uint64_t audio_irq_drop_total(void);
uint64_t audio_ap_deferred_job_total(void);
uint64_t audio_queue_depth_for_cpu(uint64_t cpu_index);
uint64_t audio_queue_high_water_for_cpu(uint64_t cpu_index);
uint64_t audio_queue_high_water_total(void);
uint64_t audio_processed_events_total(void);
uint64_t audio_processed_irq_events_total(void);
uint64_t audio_processed_injected_events_total(void);
uint64_t audio_batches_total(void);
bool tpm_init(void);
bool secureboot_init(void);
bool quantum_sim_init(void);
void quantum_sim_run_demo(void);

#endif
