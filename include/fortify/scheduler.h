#ifndef FORTIFY_SCHEDULER_H
#define FORTIFY_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "fortify/interrupts.h"

bool scheduler_init(void);
void scheduler_start(void);
interrupt_frame *scheduler_on_timer_tick(interrupt_frame *frame);
bool scheduler_service_deferred_switch(void);
bool scheduler_yield(void);

uint64_t scheduler_tick_count(void);
uint64_t scheduler_preempt_count(void);
uint64_t scheduler_current_thread_index(void);
const char *scheduler_current_thread_name(void);
uint64_t scheduler_stack_owner_index_for_address(uint64_t address);

#endif
