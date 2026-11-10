#include "fortify/systems/kernel/kernel_idle_loop.h"

#include "fortify/systems/kernel/kernel_idle_observability.h"
#include "fortify/systems/kernel/runtime_commands.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

namespace {
void kernel_idle_with_tick_heartbeat_impl(void) {
    for (;;) {
        scheduler_service_deferred_switch();
        __asm__ volatile ("hlt");
        kernel_service_runtime_commands();
        const uint64_t ticks = timer_ticks();
        video_render_tick(ticks);
        kernel_idle_poll_smp_observability(ticks);
    }
}

} // namespace

void kernel_idle_with_tick_heartbeat(void) {
    kernel_idle_with_tick_heartbeat_impl();
}
