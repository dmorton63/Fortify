#ifndef FORTIFY_KERNEL_IDLE_OBSERVABILITY_H
#define FORTIFY_KERNEL_IDLE_OBSERVABILITY_H

#include <stdint.h>

void kernel_idle_poll_smp_observability(uint64_t ticks);
void kernel_set_runtime_message_backlog_guardrail(uint64_t warn_depth, uint64_t transition_ticks);
uint64_t kernel_runtime_message_backlog_warn_depth(void);
uint64_t kernel_runtime_message_backlog_transition_ticks(void);

#endif
