#ifndef FORTIFY_SUBSYSTEMS_H
#define FORTIFY_SUBSYSTEMS_H

#include <stdbool.h>
#include <stdint.h>

bool memory_init(void);
bool interrupts_init(void);
uint64_t timer_ticks(void);
bool scheduler_init(void);
void scheduler_start(void);
bool scheduler_service_deferred_switch(void);
bool smp_init(void);
bool usb_init(void);
bool input_init(void);
bool filesystem_init(void);
bool port_manager_init(void);
bool video_init(void);
bool renderer_init(void);
bool compositor_init(void);
bool networking_init(void);
bool webaccess_init(void);
bool webcam_init(void);
bool audio_init(void);
bool tpm_init(void);
bool secureboot_init(void);
bool quantum_sim_init(void);
void quantum_sim_run_demo(void);

#endif
