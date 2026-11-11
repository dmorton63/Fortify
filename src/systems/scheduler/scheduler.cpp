#include "fortify/systems/boot/boot.h"
#include "fortify/systems/console/console.h"
#include "fortify/systems/diagnostics/diag.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/serial/serial.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

#include <stdint.h>

bool scheduler_service_deferred_switch(void);

namespace {
constexpr uint64_t kMaxCpus = 64;
constexpr uint64_t kMaxThreads = 1 + ((kMaxCpus - 1) * 2);
constexpr uint64_t kDefaultTimesliceTicks = 10;
constexpr uint64_t kThreadStackSize = 8192;
constexpr bool kForensicsMode = false;
constexpr bool kEnableWorkerUnparkProbe = kForensicsMode;
constexpr bool kEnableWorkerEscapeProbe = false;             // behavior-altering probe
constexpr bool kEnableWorkerIRQIFGuard = false;              // behavior-altering probe
constexpr bool kEnableWorkerSecondTickProbe = kForensicsMode;
constexpr bool kEnableWorkerFrameChecksumProbe = kForensicsMode;
constexpr bool kEnableWorkerFrameOwnershipProbe = kForensicsMode;
constexpr bool kEnableWorkerEntryStageProbe = kForensicsMode;
constexpr bool kEnableWorkerLiveRspProbe = kForensicsMode;
constexpr bool kEnableWorkerLiveRspAsmProbe = kForensicsMode;
constexpr bool kEnableWorkerEntryMaskIrq0Probe = false;      // behavior-altering probe
constexpr bool kEnableWorkerEntryNoStiProbe = false;         // behavior-altering probe
constexpr bool kEnableWorkerEntryWindowProbe = kForensicsMode;
constexpr bool kEnableWorkerEntryEdgeProbe = kForensicsMode;
constexpr bool kEnableDeferredContextSwitch = true;
constexpr bool kEnableWorkerEventSeqProbe = kForensicsMode;
constexpr bool kEnableWorkerReentryGuardProbe = kForensicsMode;
constexpr uint64_t kWorkerUnparkProbeTick = 300;
constexpr uint64_t kWorkerThreadIndex = 1;
constexpr uint64_t kNoThreadIndex = ~0ULL;
#if defined(FORTIFY_AP_STRESS)
constexpr uint64_t kApWorkQueueCapacity = 32;
constexpr bool kEnableApWorkRateLimit = false;
#else
constexpr uint64_t kApWorkQueueCapacity = 16;
constexpr bool kEnableApWorkRateLimit = true;
#endif
constexpr uint64_t kApWorkKindSchedulerKick = 1;
constexpr uint64_t kApWorkKindMaintenanceNop = 2;
constexpr uint64_t kApWorkKindNetworkingNop = 3;
constexpr uint64_t kApWorkKindPinnedTask = 4;
constexpr uint64_t kMaxApPinnedTasks = kMaxCpus - 1;
#if defined(FORTIFY_AP_STRESS)
constexpr uint64_t kApWorkMinIntervalTicksMaintenance = 0;
constexpr uint64_t kApWorkMinIntervalTicksNetworking = 0;
constexpr uint64_t kApWorkMaxPendingPerKindPerCpu = 1;
constexpr uint64_t kApPinnedTaskMigrationIntervalTicks = 50;
#else
constexpr uint64_t kApWorkMinIntervalTicksMaintenance = 300;
constexpr uint64_t kApWorkMinIntervalTicksNetworking = 400;
constexpr uint64_t kApWorkMaxPendingPerKindPerCpu = 1;
constexpr uint64_t kApPinnedTaskMigrationIntervalTicks = 500;
#endif

struct ap_pinned_task {
    bool active;
    uint64_t cpu_index;
    volatile uint64_t run_count;
    uint64_t seen_cpu_mask;
};

struct scheduler_thread {
    const char *name;
    bool runnable;
    bool cpu_pinned;
    uint64_t pinned_cpu_index;
    bool has_resume_frame;
    uint64_t runtime_ticks;
    bool needs_bootstrap_frame;
    void (*entry_point)(void);
    interrupt_stack_image *stack_image_slot;
    interrupt_stack_image saved_stack_image;
    uint64_t stack_base_addr;
    uint64_t stack_top_addr;
    uint64_t stack_canary_lo_addr;
    uint64_t stack_canary_hi_addr;
};

struct scheduler_state {
    bool ready;
    bool running;
    scheduler_thread threads[kMaxThreads];
    uint64_t thread_count;
    uint64_t total_ticks;
    uint64_t preemptions;
    uint64_t context_switches;
    uint64_t cpu_count;
    bool logged_first_preempt;
    bool logged_first_frame_restore;
    bool logged_deferred_mode;
    bool logged_deferred_commit;
    bool logged_irq_delivery_invariants;
    bool worker_probe_attempted;
    bool worker_probe_switched;
    bool worker_probe_completed;
    bool worker_active;
    uint64_t worker_irq_trace_count;
    uint64_t worker_phase_trace_seq;
    uint64_t worker_event_seq;
    bool worker_escape_done;
    bool worker_if_guard_pending;
    bool worker_second_tick_first_captured;
    bool worker_second_tick_reported;
    uint64_t worker_second_tick_first_frame_ptr;
    uint64_t worker_second_tick_first_slot_ptr;
    interrupt_stack_image worker_second_tick_first_irq_frame;
    interrupt_stack_image worker_second_tick_first_slot_frame;
    bool worker_frame_probe_armed;
    bool worker_frame_probe_reported;
    interrupt_stack_image worker_probe_switch_frame;
};

struct scheduler_cpu_state {
    bool online;
    bool preempt_pending;
    uint64_t current_index;
    uint64_t pending_index;
    uint64_t quantum_ticks;
    interrupt_stack_image last_saved_frame;
};

struct ap_work_item {
    uint64_t kind;
    uint64_t sequence;
};

struct ap_work_dispatch_entry {
    uint64_t kind;
    void (*execute)(uint64_t cpu_index, uint64_t sequence);
    volatile uint64_t *execute_total_counter;
    volatile uint64_t *execute_by_cpu_counter;
};

scheduler_state g_scheduler = {};
scheduler_cpu_state g_cpu[kMaxCpus] = {};
alignas(16) uint8_t g_thread_stacks[kMaxThreads][kThreadStackSize] = {};
volatile uint64_t g_ipi_preempt_requests_total = 0;
volatile uint64_t g_ipi_preempt_requests_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_dispatch_total = 0;
volatile uint64_t g_ap_work_dispatch_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_execute_total = 0;
volatile uint64_t g_ap_work_execute_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_maintenance_dispatch_total = 0;
volatile uint64_t g_ap_work_maintenance_dispatch_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_maintenance_execute_total = 0;
volatile uint64_t g_ap_work_maintenance_execute_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_maintenance_last_enqueue_tick_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_networking_dispatch_total = 0;
volatile uint64_t g_ap_work_networking_dispatch_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_networking_execute_total = 0;
volatile uint64_t g_ap_work_networking_execute_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_networking_last_enqueue_tick_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_pinned_task_dispatch_total = 0;
volatile uint64_t g_ap_work_pinned_task_dispatch_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_pinned_task_execute_total = 0;
volatile uint64_t g_ap_work_pinned_task_execute_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_pinned_task_last_enqueue_tick_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_pinned_task_migration_total = 0;
volatile uint64_t g_ap_pinned_task_migration_enabled = 0;
volatile uint64_t g_ap_pinned_task_last_migration_tick = 0;
volatile uint64_t g_ap_to_ap_switch_total = 0;
volatile uint64_t g_ap_to_ap_switch_by_cpu[kMaxCpus] = {};
volatile uint64_t g_bsp_to_ap_switch_total = 0;
volatile uint64_t g_bsp_to_ap_switch_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_worker_threads_provisioned_total = 0;
volatile uint64_t g_ap_pinned_threads_provisioned_total = 0;
volatile uint64_t g_ap_threads_expected_total = 0;
volatile uint64_t g_ap_work_skip_rate_limit_total = 0;
volatile uint64_t g_ap_work_skip_rate_limit_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_skip_pending_cap_total = 0;
volatile uint64_t g_ap_work_skip_pending_cap_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_drop_total = 0;
volatile uint64_t g_ap_work_drop_by_cpu[kMaxCpus] = {};
volatile uint64_t g_ap_work_next_sequence_by_cpu[kMaxCpus] = {};
volatile uint32_t g_ap_work_head_by_cpu[kMaxCpus] = {};
volatile uint32_t g_ap_work_tail_by_cpu[kMaxCpus] = {};
ap_work_item g_ap_work_queues[kMaxCpus][kApWorkQueueCapacity] = {};
uint64_t g_ap_worker_thread_for_cpu[kMaxCpus] = {};
uint64_t g_ap_pinned_thread_for_cpu[kMaxCpus] = {};
ap_pinned_task g_ap_pinned_tasks[kMaxApPinnedTasks] = {};

constexpr uint64_t kFrameCanaryLo = 0xcafebabedeadbeefULL;
constexpr uint64_t kFrameCanaryHi = 0xfacefeed12345678ULL;
uint64_t g_kernel_ss_selector = 0;

extern "C" volatile uint64_t g_worker_frame_slot_probe_enabled;
extern "C" volatile uint64_t g_worker_frame_slot_probe_frame_addr;
extern "C" volatile uint64_t g_worker_frame_slot_probe_canary_lo_addr;
extern "C" volatile uint64_t g_worker_frame_slot_probe_canary_hi_addr;
extern "C" volatile uint64_t g_worker_frame_slot_probe_canary_lo_expect;
extern "C" volatile uint64_t g_worker_frame_slot_probe_canary_hi_expect;
extern "C" volatile uint64_t g_worker_frame_slot_probe_owner_index;
extern "C" volatile uint64_t g_worker_frame_slot_probe_resume_rsp;
extern "C" uint8_t irq_dispatch_stack;
extern "C" uint8_t irq_dispatch_stack_top;
extern "C" uint8_t irq_dispatch_stacks_end;

extern "C" volatile uint64_t g_worker_rsp_before_sti __attribute__((section(".noinit.diag"))) = 0;
extern "C" volatile uint64_t g_worker_rsp_after_sti __attribute__((section(".noinit.diag"))) = 0;
extern "C" volatile uint64_t g_worker_rsp_first_loop __attribute__((section(".noinit.diag"))) = 0;
extern "C" volatile uint64_t g_worker_rsp_probe_stack_base __attribute__((section(".noinit.diag"))) = 0;
extern "C" volatile uint64_t g_worker_rsp_probe_stack_top __attribute__((section(".noinit.diag"))) = 0;
extern "C" volatile uint64_t g_worker_rsp_probe_enabled __attribute__((section(".noinit.diag"))) = 0;

uint64_t current_cpu_index(void);
bool enqueue_ap_work_item(uint64_t cpu_idx, uint64_t kind, uint64_t sequence);
bool dequeue_ap_work_item(uint64_t cpu_idx, ap_work_item *out);

void serial_write_hex64(uint64_t value) {
    static const char kHex[] = "0123456789abcdef";
    char out[19] = "0x0000000000000000";
    for (uint64_t i = 0; i < 16; i++) {
        out[17 - i] = kHex[value & 0x0f];
        value >>= 4;
    }
    serial_write(out);
}

void write_hex64(char out[19], uint64_t value) {
    static const char kHex[] = "0123456789abcdef";
    out[0] = '0';
    out[1] = 'x';
    for (uint64_t i = 0; i < 16; i++) {
        out[17 - i] = kHex[value & 0x0f];
        value >>= 4;
    }
    out[18] = '\0';
}

void log_ap_thread_provision_summary(uint64_t workers, uint64_t pinned, uint64_t expected) {
    char workers_hex[19];
    char pinned_hex[19];
    char expected_hex[19];
    write_hex64(workers_hex, workers);
    write_hex64(pinned_hex, pinned);
    write_hex64(expected_hex, expected);

    serial_write("[INFO] scheduler: AP thread provisioning workers=");
    serial_write(workers_hex);
    serial_write("/");
    serial_write(expected_hex);
    serial_write(" pinned=");
    serial_write(pinned_hex);
    serial_write("/");
    serial_write(expected_hex);
    serial_write("\n");

    console_write("[INFO] scheduler: AP thread provisioning workers=");
    console_write(workers_hex);
    console_write("/");
    console_write(expected_hex);
    console_write(" pinned=");
    console_write(pinned_hex);
    console_write("/");
    console_write(expected_hex);
    console_write("\n");
}

void execute_ap_work_scheduler_kick(uint64_t cpu_index, uint64_t sequence) {
    (void)cpu_index;
    (void)sequence;
}

void execute_ap_work_maintenance(uint64_t cpu_index, uint64_t sequence) {
    usb_process_ap_deferred_job(cpu_index, sequence);
    audio_process_ap_deferred_job(cpu_index, sequence);
    webcam_process_ap_deferred_job(cpu_index, sequence);
    port_manager_process_ap_deferred_job(cpu_index, sequence);
    filesystem_process_ap_deferred_job(cpu_index, sequence);
    input_process_ap_deferred_job(cpu_index, sequence);
}

void execute_ap_work_networking(uint64_t cpu_index, uint64_t sequence) {
    networking_process_ap_deferred_job(cpu_index, sequence);
}

void execute_ap_work_pinned_task(uint64_t cpu_index, uint64_t sequence) {
    const uint64_t task_index = sequence;
    if (task_index >= kMaxApPinnedTasks) {
        return;
    }
    if (!g_ap_pinned_tasks[task_index].active) {
        return;
    }
    if (g_ap_pinned_tasks[task_index].cpu_index != cpu_index) {
        return;
    }
    __atomic_fetch_add(&g_ap_pinned_tasks[task_index].run_count, 1, __ATOMIC_RELAXED);
}

const ap_work_dispatch_entry *find_ap_work_dispatch_entry(uint64_t kind) {
    static const ap_work_dispatch_entry kEntries[] = {
        {kApWorkKindSchedulerKick, execute_ap_work_scheduler_kick, nullptr, nullptr},
        {kApWorkKindMaintenanceNop, execute_ap_work_maintenance,
         &g_ap_work_maintenance_execute_total, &g_ap_work_maintenance_execute_by_cpu[0]},
        {kApWorkKindNetworkingNop, execute_ap_work_networking,
         &g_ap_work_networking_execute_total, &g_ap_work_networking_execute_by_cpu[0]},
        {kApWorkKindPinnedTask, execute_ap_work_pinned_task,
         &g_ap_work_pinned_task_execute_total, &g_ap_work_pinned_task_execute_by_cpu[0]},
    };
    for (uint64_t i = 0; i < (sizeof(kEntries) / sizeof(kEntries[0])); i++) {
        if (kEntries[i].kind == kind) {
            return &kEntries[i];
        }
    }
    return nullptr;
}

uint64_t ap_work_pending_for_cpu(const volatile uint64_t *dispatch_by_cpu,
                                 const volatile uint64_t *execute_by_cpu,
                                 uint64_t cpu_idx) {
    if (cpu_idx >= kMaxCpus) {
        return 0;
    }
    const uint64_t dispatched = __atomic_load_n(&dispatch_by_cpu[cpu_idx], __ATOMIC_RELAXED);
    const uint64_t executed = __atomic_load_n(&execute_by_cpu[cpu_idx], __ATOMIC_RELAXED);
    return dispatched >= executed ? (dispatched - executed) : 0;
}

bool enqueue_periodic_ap_work_kind(uint64_t cpu_idx,
                                   uint64_t kind,
                                   volatile uint64_t *dispatch_total_counter,
                                   volatile uint64_t *dispatch_by_cpu_counter,
                                   volatile uint64_t *execute_by_cpu_counter,
                                   volatile uint64_t *last_enqueue_tick_by_cpu,
                                   uint64_t min_interval_ticks,
                                   uint64_t now_ticks) {
    if (cpu_idx >= kMaxCpus) {
        return false;
    }

    if (kEnableApWorkRateLimit) {
        const uint64_t last_tick = __atomic_load_n(&last_enqueue_tick_by_cpu[cpu_idx], __ATOMIC_RELAXED);
        if (now_ticks < last_tick || (now_ticks - last_tick) < min_interval_ticks) {
            __atomic_fetch_add(&g_ap_work_skip_rate_limit_total, 1, __ATOMIC_RELAXED);
            __atomic_fetch_add(&g_ap_work_skip_rate_limit_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
            return false;
        }
    }

    const uint64_t pending = ap_work_pending_for_cpu(dispatch_by_cpu_counter, execute_by_cpu_counter, cpu_idx);
    if (pending >= kApWorkMaxPendingPerKindPerCpu) {
        __atomic_fetch_add(&g_ap_work_skip_pending_cap_total, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_ap_work_skip_pending_cap_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
        return false;
    }

    const uint64_t sequence = __atomic_add_fetch(&g_ap_work_next_sequence_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
    if (!enqueue_ap_work_item(cpu_idx, kind, sequence)) {
        return false;
    }

    __atomic_store_n(&last_enqueue_tick_by_cpu[cpu_idx], now_ticks, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_ap_work_dispatch_total, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_ap_work_dispatch_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(dispatch_total_counter, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&dispatch_by_cpu_counter[cpu_idx], 1, __ATOMIC_RELAXED);
    return true;
}

bool enqueue_ap_work_item(uint64_t cpu_idx, uint64_t kind, uint64_t sequence) {
    if (cpu_idx >= kMaxCpus) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_ap_work_head_by_cpu[cpu_idx], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_ap_work_tail_by_cpu[cpu_idx], __ATOMIC_ACQUIRE);
    const uint32_t next_head = (head + 1) % kApWorkQueueCapacity;
    if (next_head == tail) {
        __atomic_fetch_add(&g_ap_work_drop_total, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_ap_work_drop_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
        return false;
    }
    g_ap_work_queues[cpu_idx][head].kind = kind;
    g_ap_work_queues[cpu_idx][head].sequence = sequence;
    __atomic_store_n(&g_ap_work_head_by_cpu[cpu_idx], next_head, __ATOMIC_RELEASE);
    return true;
}

bool dequeue_ap_work_item(uint64_t cpu_idx, ap_work_item *out) {
    if (cpu_idx >= kMaxCpus || out == nullptr) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_ap_work_head_by_cpu[cpu_idx], __ATOMIC_ACQUIRE);
    const uint32_t tail = __atomic_load_n(&g_ap_work_tail_by_cpu[cpu_idx], __ATOMIC_RELAXED);
    if (tail == head) {
        return false;
    }
    *out = g_ap_work_queues[cpu_idx][tail];
    const uint32_t next_tail = (tail + 1) % kApWorkQueueCapacity;
    __atomic_store_n(&g_ap_work_tail_by_cpu[cpu_idx], next_tail, __ATOMIC_RELEASE);
    return true;
}

bool ap_work_queue_has_items(uint64_t cpu_idx) {
    if (cpu_idx >= kMaxCpus) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_ap_work_head_by_cpu[cpu_idx], __ATOMIC_ACQUIRE);
    const uint32_t tail = __atomic_load_n(&g_ap_work_tail_by_cpu[cpu_idx], __ATOMIC_RELAXED);
    return head != tail;
}

[[noreturn]] void scheduler_ap_pinned_task_entry(void) {
    for (;;) {
        scheduler_service_deferred_switch();
        const uint64_t cpu_idx = current_cpu_index();
        if (cpu_idx > 0 && cpu_idx < kMaxCpus) {
            bool ran_task = false;
            for (uint64_t task_index = 0; task_index < kMaxApPinnedTasks; task_index++) {
                if (!g_ap_pinned_tasks[task_index].active ||
                    g_ap_pinned_tasks[task_index].cpu_index != cpu_idx) {
                    continue;
                }
                g_ap_pinned_tasks[task_index].seen_cpu_mask |= (1ULL << cpu_idx);
                __atomic_fetch_add(&g_ap_pinned_tasks[task_index].run_count, 1, __ATOMIC_RELAXED);
                __atomic_fetch_add(&g_ap_work_pinned_task_execute_total, 1, __ATOMIC_RELAXED);
                __atomic_fetch_add(&g_ap_work_pinned_task_execute_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
                ran_task = true;
            }
            if (!ran_task) {
                __asm__ volatile ("pause" : : : "memory");
            }
        }
        __asm__ volatile ("sti; hlt; cli" : : : "memory");
    }
}

[[noreturn]] void scheduler_ap_worker_entry(void) {
    for (;;) {
        scheduler_service_deferred_switch();
        const uint64_t cpu_idx = current_cpu_index();
        if (cpu_idx < kMaxCpus) {
            ap_work_item item = {};
            if (dequeue_ap_work_item(cpu_idx, &item)) {
                const ap_work_dispatch_entry *entry = find_ap_work_dispatch_entry(item.kind);
                if (entry == nullptr || entry->execute == nullptr) {
                    continue;
                }
                entry->execute(cpu_idx, item.sequence);
                __atomic_fetch_add(&g_ap_work_execute_total, 1, __ATOMIC_RELAXED);
                __atomic_fetch_add(&g_ap_work_execute_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
                if (entry->execute_total_counter != nullptr) {
                    __atomic_fetch_add(entry->execute_total_counter, 1, __ATOMIC_RELAXED);
                }
                if (entry->execute_by_cpu_counter != nullptr && cpu_idx < kMaxCpus) {
                    __atomic_fetch_add(&entry->execute_by_cpu_counter[cpu_idx], 1, __ATOMIC_RELAXED);
                }
                continue;
            }
        }
        __asm__ volatile ("sti; hlt; cli" : : : "memory");
    }
}

uint64_t read_rsp(void) {
    uint64_t rsp = 0;
    __asm__ volatile ("movq %%rsp, %0" : "=r"(rsp));
    return rsp;
}

uint64_t frame_fingerprint(const interrupt_frame *frame) {
    if (frame == nullptr) {
        return 0;
    }
    const uint64_t *words = reinterpret_cast<const uint64_t *>(frame);
    uint64_t acc = 0x9e3779b97f4a7c15ULL;
    for (uint64_t i = 0; i < 20; i++) {
        acc ^= words[i] + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2);
    }
    return acc;
}

uint64_t frame_word_diff_count(const interrupt_frame *a, const interrupt_frame *b) {
    if (a == nullptr || b == nullptr) {
        return 0;
    }
    const uint64_t *lhs = reinterpret_cast<const uint64_t *>(a);
    const uint64_t *rhs = reinterpret_cast<const uint64_t *>(b);
    uint64_t diff = 0;
    for (uint64_t i = 0; i < 20; i++) {
        if (lhs[i] != rhs[i]) {
            diff++;
        }
    }
    return diff;
}

uint64_t frame_word_diff_count_range(const interrupt_frame *a, const interrupt_frame *b,
                                     uint64_t start_word, uint64_t word_count) {
    if (a == nullptr || b == nullptr || start_word >= 20) {
        return 0;
    }
    const uint64_t max_count = (start_word + word_count > 20) ? (20 - start_word) : word_count;
    const uint64_t *lhs = reinterpret_cast<const uint64_t *>(a);
    const uint64_t *rhs = reinterpret_cast<const uint64_t *>(b);
    uint64_t diff = 0;
    for (uint64_t i = 0; i < max_count; i++) {
        if (lhs[start_word + i] != rhs[start_word + i]) {
            diff++;
        }
    }
    return diff;
}

uint64_t frame_fingerprint_range(const interrupt_frame *frame, uint64_t start_word, uint64_t word_count) {
    if (frame == nullptr || start_word >= 20) {
        return 0;
    }
    const uint64_t max_count = (start_word + word_count > 20) ? (20 - start_word) : word_count;
    const uint64_t *words = reinterpret_cast<const uint64_t *>(frame);
    uint64_t acc = 0x9e3779b97f4a7c15ULL;
    for (uint64_t i = 0; i < max_count; i++) {
        acc ^= words[start_word + i] + 0x9e3779b97f4a7c15ULL + (acc << 6) + (acc >> 2);
    }
    return acc;
}

bool should_trace_worker_phase(uint64_t seq) {
    return seq < 32 || (seq % 64) == 0;
}

bool should_defer_context_switch(void) {
    return kEnableDeferredContextSwitch;
}

bool thread_slot_canary_ok(const scheduler_thread &thread);

bool is_canonical_address(uint64_t addr) {
    const uint64_t sign = (addr >> 47) & 1ULL;
    const uint64_t upper = addr >> 48;
    return sign == 0 ? upper == 0 : upper == 0xFFFF;
}

uint16_t read_ss(void) {
    uint16_t ss = 0;
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));
    return ss;
}

extern "C" __attribute__((naked, noreturn)) void scheduler_iret_to_frame(interrupt_frame *frame) {
    __asm__ volatile (
        "movq %rdi, %rsp\n"
        "popq %r15\n"
        "popq %r14\n"
        "popq %r13\n"
        "popq %r12\n"
        "popq %r11\n"
        "popq %r10\n"
        "popq %r9\n"
        "popq %r8\n"
        "popq %rdi\n"
        "popq %rsi\n"
        "popq %rbp\n"
        "popq %rdx\n"
        "popq %rcx\n"
        "popq %rbx\n"
        "popq %rax\n"
        "addq $16, %rsp\n"
        "iretq\n"
    );
}

const char *stack_owner_label(uint64_t idx) {
    if (idx == 0) {
        return "main";
    }
    if (idx == 1) {
        return "worker";
    }
    if (idx == ~0ULL) {
        return "unknown";
    }
    return "other";
}

bool is_in_irq_dispatch_stack(uint64_t address) {
    const uint64_t lo = reinterpret_cast<uint64_t>(&irq_dispatch_stack);
    const uint64_t hi = reinterpret_cast<uint64_t>(&irq_dispatch_stacks_end);
    return address >= lo && address < hi;
}

const char *stack_class_label(uint64_t address) {
    if (is_in_irq_dispatch_stack(address)) {
        return "irq-dispatch";
    }
    const uint64_t idx = scheduler_stack_owner_index_for_address(address);
    if (idx == 0) {
        return "main";
    }
    if (idx == 1) {
        return "worker";
    }
    if (idx == ~0ULL) {
        return "non-thread";
    }
    return "other-thread";
}

extern "C" void scheduler_worker_live_rsp_probe(uint64_t live_rsp, uint64_t phase_tag) {
    if (!kEnableWorkerLiveRspProbe || kWorkerThreadIndex >= g_scheduler.thread_count) {
        return;
    }

    const scheduler_thread &worker = g_scheduler.threads[kWorkerThreadIndex];
    const bool in_range = live_rsp >= worker.stack_base_addr && live_rsp < worker.stack_top_addr;
    const bool canary_ok = thread_slot_canary_ok(worker);

    serial_write("[TRACE] worker-live-rsp phase=");
    serial_write_hex64(phase_tag);
    serial_write(" rsp=");
    serial_write_hex64(live_rsp);
    serial_write(" class=");
    serial_write(stack_class_label(live_rsp));
    serial_write(" in_range=");
    serial_write(in_range ? "1" : "0");
    serial_write(" align=");
    serial_write_hex64(live_rsp & 0x0f);
    serial_write(" canonical=");
    serial_write(is_canonical_address(live_rsp) ? "1" : "0");
    serial_write(" base=");
    serial_write_hex64(worker.stack_base_addr);
    serial_write(" top=");
    serial_write_hex64(worker.stack_top_addr);
    serial_write(" canary_ok=");
    serial_write(canary_ok ? "1" : "0");
    serial_write("\n");
}

void trace_worker_phase(const char *phase, uint64_t cpu_idx, const interrupt_frame *frame,
                        uint64_t current_idx, uint64_t pending_idx) {
    const uint64_t seq = g_scheduler.worker_phase_trace_seq++;
    if (!should_trace_worker_phase(seq)) {
        return;
    }

    serial_write("[TRACE] worker-phase ");
    serial_write(phase);
    serial_write(" seq=");
    serial_write_hex64(seq);
    serial_write(" cpu=");
    serial_write_hex64(cpu_idx);
    serial_write(" cur=");
    serial_write_hex64(current_idx);
    serial_write(" pend=");
    serial_write_hex64(pending_idx);
    if (frame != nullptr) {
        serial_write(" frame=");
        serial_write_hex64(reinterpret_cast<uint64_t>(frame));
        serial_write(" rip=");
        serial_write_hex64(frame->rip);
    }
    serial_write("\n");
}

[[noreturn]] void panic_worker_reentry(const char *reason, uint64_t cpu_idx,
                                       uint64_t current_idx, uint64_t pending_idx) {
    serial_write("[FATAL] worker-reentry-guard: ");
    serial_write(reason);
    serial_write(" cpu=");
    serial_write_hex64(cpu_idx);
    serial_write(" cur=");
    serial_write_hex64(current_idx);
    serial_write(" pend=");
    serial_write_hex64(pending_idx);
    serial_write("\n");
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void trace_worker_event(const char *event, uint64_t cpu_idx, uint64_t current_idx, uint64_t pending_idx) {
    if (!kEnableWorkerEventSeqProbe) {
        return;
    }
    const uint64_t seq = g_scheduler.worker_event_seq++;
    serial_write("[TRACE] worker-event #");
    serial_write_hex64(seq);
    serial_write(" ");
    serial_write(event);
    serial_write(" cpu=");
    serial_write_hex64(cpu_idx);
    serial_write(" cur=");
    serial_write_hex64(current_idx);
    serial_write(" pend=");
    serial_write_hex64(pending_idx);
    serial_write("\n");
}

extern "C" __attribute__((naked, noreturn)) void scheduler_worker_entry(void) {
    __asm__ volatile (
        "cld\n"
        "xorq %rbp, %rbp\n"
        "movw $0x3f8, %dx\n"
        "movb $0x57, %al\n"
        "outb %al, %dx\n"
        "sti\n"
        "1:\n"
        "movb $0x77, %al\n"
        "outb %al, %dx\n"
        "hlt\n"
        "jmp 1b\n"
    );
}

extern "C" __attribute__((naked, noreturn)) void scheduler_worker_entry_stage_probe(void) {
    __asm__ volatile (
        "cld\n"
        "xorq %rbp, %rbp\n"
        "movw $0x3f8, %dx\n"
        "movb $0x57, %al\n"
        "outb %al, %dx\n"
        "movb $0x41, %al\n"
        "outb %al, %dx\n"
        "movq g_worker_rsp_probe_enabled(%rip), %rcx\n"
        "testq %rcx, %rcx\n"
        "je 20f\n"
        "movq %rsp, g_worker_rsp_before_sti(%rip)\n"
        "movq g_worker_rsp_probe_stack_base(%rip), %r8\n"
        "movq g_worker_rsp_probe_stack_top(%rip), %r9\n"
        "movq %rsp, %rax\n"
        "cmpq %r8, %rax\n"
        "jb 10f\n"
        "cmpq %r9, %rax\n"
        "jae 10f\n"
        "movb $0x72, %al\n"   // 'r'
        "jmp 11f\n"
        "10:\n"
        "movb $0x52, %al\n"   // 'R'
        "11:\n"
        "movw $0x3f8, %dx\n"
        "outb %al, %dx\n"
        "movq %rsp, %rax\n"
        "testq $0x0f, %rax\n"
        "jne 12f\n"
        "movb $0x61, %al\n"   // 'a'
        "jmp 13f\n"
        "12:\n"
        "movb $0x41, %al\n"   // 'A'
        "13:\n"
        "movw $0x3f8, %dx\n"
        "outb %al, %dx\n"
        "movb $0x70, %al\n"   // 'p'
        "outb %al, %dx\n"
        "movq %rsp, %r10\n"
        "movq $16, %r11\n"
        "14:\n"
        "movq %r10, %rax\n"
        "shrq $60, %rax\n"
        "andq $0x0f, %rax\n"
        "cmpq $9, %rax\n"
        "jbe 15f\n"
        "addb $0x57, %al\n"
        "jmp 16f\n"
        "15:\n"
        "addb $0x30, %al\n"
        "16:\n"
        "movw $0x3f8, %dx\n"
        "outb %al, %dx\n"
        "shlq $4, %r10\n"
        "decq %r11\n"
        "jnz 14b\n"
        "20:\n"
        "sti\n"
        "movb $0x42, %al\n"
        "outb %al, %dx\n"
        "movq g_worker_rsp_probe_enabled(%rip), %rcx\n"
        "testq %rcx, %rcx\n"
        "je 30f\n"
        "movq %rsp, g_worker_rsp_after_sti(%rip)\n"
        "movq g_worker_rsp_probe_stack_base(%rip), %r8\n"
        "movq g_worker_rsp_probe_stack_top(%rip), %r9\n"
        "movq %rsp, %rax\n"
        "cmpq %r8, %rax\n"
        "jb 21f\n"
        "cmpq %r9, %rax\n"
        "jae 21f\n"
        "movb $0x73, %al\n"   // 's'
        "jmp 22f\n"
        "21:\n"
        "movb $0x53, %al\n"   // 'S'
        "22:\n"
        "movw $0x3f8, %dx\n"
        "outb %al, %dx\n"
        "movq %rsp, %rax\n"
        "testq $0x0f, %rax\n"
        "jne 23f\n"
        "movb $0x74, %al\n"   // 't'
        "jmp 24f\n"
        "23:\n"
        "movb $0x54, %al\n"   // 'T'
        "24:\n"
        "movw $0x3f8, %dx\n"
        "outb %al, %dx\n"
        "movb $0x71, %al\n"   // 'q'
        "outb %al, %dx\n"
        "movq %rsp, %r10\n"
        "movq $16, %r11\n"
        "25:\n"
        "movq %r10, %rax\n"
        "shrq $60, %rax\n"
        "andq $0x0f, %rax\n"
        "cmpq $9, %rax\n"
        "jbe 26f\n"
        "addb $0x57, %al\n"
        "jmp 27f\n"
        "26:\n"
        "addb $0x30, %al\n"
        "27:\n"
        "movw $0x3f8, %dx\n"
        "outb %al, %dx\n"
        "shlq $4, %r10\n"
        "decq %r11\n"
        "jnz 25b\n"
        "30:\n"
        "1:\n"
        "movq g_worker_rsp_probe_enabled(%rip), %rcx\n"
        "testq %rcx, %rcx\n"
        "je 31f\n"
        "cmpq $0, g_worker_rsp_first_loop(%rip)\n"
        "jne 31f\n"
        "movq %rsp, g_worker_rsp_first_loop(%rip)\n"
        "31:\n"
        "movb $0x77, %al\n"
        "outb %al, %dx\n"
        "hlt\n"
        "jmp 1b\n"
    );
}

extern "C" __attribute__((naked, noreturn)) void scheduler_worker_entry_mask_irq0_probe(void) {
    __asm__ volatile (
        "cld\n"
        "xorq %rbp, %rbp\n"
        "movw $0x3f8, %dx\n"
        "movb $0x57, %al\n"
        "outb %al, %dx\n"
        "movb $0x41, %al\n"
        "outb %al, %dx\n"
        "movw $0x21, %dx\n"
        "inb %dx, %al\n"
        "orb $0x01, %al\n"
        "outb %al, %dx\n"
        "movw $0x3f8, %dx\n"
        "movb $0x4d, %al\n"
        "outb %al, %dx\n"
        "sti\n"
        "movb $0x42, %al\n"
        "outb %al, %dx\n"
        "1:\n"
        "movb $0x77, %al\n"
        "outb %al, %dx\n"
        "pause\n"
        "jmp 1b\n"
    );
}

extern "C" __attribute__((naked, noreturn)) void scheduler_worker_entry_no_sti_probe(void) {
    __asm__ volatile (
        "cld\n"
        "xorq %rbp, %rbp\n"
        "movw $0x3f8, %dx\n"
        "movb $0x57, %al\n"
        "outb %al, %dx\n"
        "movb $0x4e, %al\n"
        "outb %al, %dx\n"
        "1:\n"
        "movb $0x6e, %al\n"
        "outb %al, %dx\n"
        "hlt\n"
        "jmp 1b\n"
    );
}

extern "C" __attribute__((naked, noreturn)) void scheduler_worker_entry_window_probe(void) {
    __asm__ volatile (
        "cld\n"
        "xorq %rbp, %rbp\n"
        "movw $0x3f8, %dx\n"
        "movb $0x57, %al\n"
        "outb %al, %dx\n"
        "movb $0x41, %al\n"
        "outb %al, %dx\n"
        "sti\n"
        "movb $0x42, %al\n"
        "outb %al, %dx\n"
        "movl $0x00040000, %ecx\n"
        "1:\n"
        "pause\n"
        "decl %ecx\n"
        "jnz 1b\n"
        "movb $0x43, %al\n"
        "outb %al, %dx\n"
        "2:\n"
        "movb $0x77, %al\n"
        "outb %al, %dx\n"
        "hlt\n"
        "jmp 2b\n"
    );
}

extern "C" __attribute__((naked, noreturn)) void scheduler_worker_entry_edge_probe(void) {
    __asm__ volatile (
        "cld\n"
        "xorq %rbp, %rbp\n"
        "movw $0x3f8, %dx\n"
        "movb $0x57, %al\n"
        "outb %al, %dx\n"
        "movb $0x41, %al\n"
        "outb %al, %dx\n"
        "sti\n"
        "movb $0x42, %al\n"
        "outb %al, %dx\n"
        "movl $0x00000100, %ecx\n"
        "1:\n"
        "pause\n"
        "decl %ecx\n"
        "jnz 1b\n"
        "movb $0x43, %al\n"
        "outb %al, %dx\n"
        "cli\n"
        "movb $0x44, %al\n"
        "outb %al, %dx\n"
        "2:\n"
        "pause\n"
        "jmp 2b\n"
    );
}

void (*select_worker_entry_probe_target())(void) {
    if (kEnableWorkerEntryMaskIrq0Probe) {
        return scheduler_worker_entry_mask_irq0_probe;
    }
    if (kEnableWorkerEntryNoStiProbe) {
        return scheduler_worker_entry_no_sti_probe;
    }
    if (kEnableWorkerEntryEdgeProbe) {
        return scheduler_worker_entry_edge_probe;
    }
    if (kEnableWorkerEntryWindowProbe) {
        return scheduler_worker_entry_window_probe;
    }
    if (kEnableWorkerEntryStageProbe) {
        return scheduler_worker_entry_stage_probe;
    }
    return scheduler_worker_entry;
}

bool add_thread(const char *name, bool runnable, void (*entry_point)(void)) {
    if (g_scheduler.thread_count >= kMaxThreads) {
        return false;
    }

    const uint64_t index = g_scheduler.thread_count++;
    scheduler_thread &slot = g_scheduler.threads[index];
    const uintptr_t stack_base = reinterpret_cast<uintptr_t>(&g_thread_stacks[index][0]);
    const uintptr_t stack_top = stack_base + kThreadStackSize;
    const uintptr_t frame_addr = (stack_top - sizeof(interrupt_frame) - (3 * sizeof(uint64_t))) & ~static_cast<uintptr_t>(0x0f);
    const uintptr_t canary_lo_addr = frame_addr - sizeof(uint64_t);
    const uintptr_t post_iret_rsp_addr = frame_addr + sizeof(interrupt_frame);
    const uintptr_t post_iret_ss_addr = post_iret_rsp_addr + sizeof(uint64_t);
    const uintptr_t canary_hi_addr = post_iret_ss_addr + sizeof(uint64_t);

    slot.name = name;
    slot.runnable = runnable;
    slot.cpu_pinned = false;
    slot.pinned_cpu_index = 0;
    slot.has_resume_frame = false;
    slot.runtime_ticks = 0;
    slot.needs_bootstrap_frame = entry_point != nullptr;
    slot.entry_point = entry_point;
    slot.stack_image_slot = reinterpret_cast<interrupt_stack_image *>(frame_addr);
    slot.stack_base_addr = stack_base;
    slot.stack_top_addr = stack_top;
    slot.stack_canary_lo_addr = canary_lo_addr;
    slot.stack_canary_hi_addr = canary_hi_addr;
    *reinterpret_cast<uint64_t *>(post_iret_rsp_addr) = post_iret_rsp_addr;
    *reinterpret_cast<uint64_t *>(post_iret_ss_addr) = g_kernel_ss_selector;
    *reinterpret_cast<uint64_t *>(slot.stack_canary_lo_addr) = kFrameCanaryLo;
    *reinterpret_cast<uint64_t *>(slot.stack_canary_hi_addr) = kFrameCanaryHi;
    return true;
}

void pin_thread_to_cpu(uint64_t thread_index, uint64_t cpu_index) {
    if (thread_index >= g_scheduler.thread_count || cpu_index >= kMaxCpus) {
        return;
    }
    g_scheduler.threads[thread_index].cpu_pinned = true;
    g_scheduler.threads[thread_index].pinned_cpu_index = cpu_index;
}

bool thread_runnable_on_cpu(uint64_t thread_index, uint64_t cpu_index) {
    if (thread_index >= g_scheduler.thread_count || cpu_index >= kMaxCpus) {
        return false;
    }
    const scheduler_thread &thread = g_scheduler.threads[thread_index];
    if (!thread.runnable) {
        return false;
    }
    if (thread.cpu_pinned && thread.pinned_cpu_index != cpu_index) {
        return false;
    }
    return true;
}

bool thread_is_ap_owned(uint64_t thread_index) {
    if (thread_index >= g_scheduler.thread_count) {
        return false;
    }
    const scheduler_thread &thread = g_scheduler.threads[thread_index];
    return thread.cpu_pinned && thread.pinned_cpu_index > 0;
}

uint64_t find_next_runnable_for_cpu(uint64_t start_index, uint64_t cpu_index) {
    if (g_scheduler.thread_count == 0) {
        return start_index;
    }

    for (uint64_t step = 1; step <= g_scheduler.thread_count; step++) {
        const uint64_t idx = (start_index + step) % g_scheduler.thread_count;
        if (thread_runnable_on_cpu(idx, cpu_index)) {
            return idx;
        }
    }
    return start_index;
}

uint64_t current_cpu_index(void) {
    return boot_current_cpu_index();
}

interrupt_frame *as_interrupt_frame(interrupt_stack_image *image) {
    return reinterpret_cast<interrupt_frame *>(image);
}

void save_interrupt_frame(interrupt_frame *dst, const interrupt_frame *src) {
    const uint64_t *in = reinterpret_cast<const uint64_t *>(src);
    uint64_t *out = reinterpret_cast<uint64_t *>(dst);
    for (uint64_t i = 0; i < 20; i++) {
        out[i] = in[i];
    }
}

void save_interrupt_stack_image(interrupt_stack_image *dst, const interrupt_frame *src) {
    const uint64_t *in = reinterpret_cast<const uint64_t *>(src);
    uint64_t *out = reinterpret_cast<uint64_t *>(dst);
    for (uint64_t i = 0; i < 20; i++) {
        out[i] = in[i];
    }
}

bool thread_slot_canary_ok(const scheduler_thread &thread) {
    if (thread.stack_canary_lo_addr == 0 || thread.stack_canary_hi_addr == 0) {
        return false;
    }
    const uint64_t lo = *reinterpret_cast<const uint64_t *>(thread.stack_canary_lo_addr);
    const uint64_t hi = *reinterpret_cast<const uint64_t *>(thread.stack_canary_hi_addr);
    return lo == kFrameCanaryLo && hi == kFrameCanaryHi;
}

interrupt_frame *materialize_resume_frame(scheduler_thread &thread) {
    if (thread.stack_image_slot == nullptr) {
        return nullptr;
    }

    if (kForensicsMode && !thread_slot_canary_ok(thread)) {
        serial_write("[TRACE] scheduler: frame-slot-canary mismatch before materialize name=");
        serial_write(thread.name != nullptr ? thread.name : "<?>");
        serial_write(" slot=");
        serial_write_hex64(reinterpret_cast<uint64_t>(thread.stack_image_slot));
        serial_write("\n");
    }

    uint64_t *dst = reinterpret_cast<uint64_t *>(thread.stack_image_slot);
    const uint64_t *src = reinterpret_cast<const uint64_t *>(&thread.saved_stack_image);
    for (uint64_t i = 0; i < 20; i++) {
        dst[i] = src[i];
    }
    return as_interrupt_frame(thread.stack_image_slot);
}

interrupt_frame *commit_pending_preemption(uint64_t cpu_idx, interrupt_frame *frame);

interrupt_frame *build_bootstrap_frame(uint64_t thread_index, const interrupt_frame *template_frame) {
    if (thread_index >= kMaxThreads) {
        return nullptr;
    }

    scheduler_thread &thread = g_scheduler.threads[thread_index];
    if (!thread.needs_bootstrap_frame || thread.entry_point == nullptr) {
        return nullptr;
    }

    auto *frame = as_interrupt_frame(&thread.saved_stack_image);
    save_interrupt_frame(frame, template_frame);
    frame->rip = reinterpret_cast<uint64_t>(thread.entry_point);
    frame->vector = 0;
    frame->error_code = 0;
    frame->rax = 0;
    frame->rbx = 0;
    frame->rcx = 0;
    frame->rdx = 0;
    frame->rbp = 0;
    frame->rdi = 0;
    frame->rsi = 0;
    frame->r8 = 0;
    frame->r9 = 0;
    frame->r10 = 0;
    frame->r11 = 0;
    frame->r12 = 0;
    frame->r13 = 0;
    frame->r14 = 0;
    frame->r15 = 0;
    frame->rflags &= ~static_cast<uint64_t>(0x200);

    thread.has_resume_frame = true;
    thread.needs_bootstrap_frame = false;

    if (kForensicsMode) {
        serial_write("[TRACE] scheduler: worker bootstrap frame=");
        serial_write_hex64(reinterpret_cast<uint64_t>(frame));
        serial_write(" rip=");
        serial_write_hex64(frame->rip);
        serial_write(" cs=");
        serial_write_hex64(frame->cs);
        serial_write(" rflags=");
        serial_write_hex64(frame->rflags);
        serial_write("\n");
    }

    return materialize_resume_frame(thread);
}

interrupt_frame *activate_worker_probe(uint64_t cpu_idx, interrupt_frame *frame) {
    if (g_scheduler.worker_probe_attempted || g_scheduler.worker_probe_completed) {
        return frame;
    }
    g_scheduler.worker_probe_attempted = true;

    if (kWorkerThreadIndex >= g_scheduler.thread_count) {
        log_error("scheduler: worker probe failed (thread index invalid).");
        g_scheduler.worker_probe_completed = true;
        return frame;
    }

    scheduler_thread &worker = g_scheduler.threads[kWorkerThreadIndex];
    worker.runnable = true;
    trace_worker_event("worker_unpark", cpu_idx, g_cpu[cpu_idx].current_index, g_cpu[cpu_idx].pending_index);
    log_info("scheduler: worker probe unparked worker thread.");
    if (kForensicsMode) {
        serial_write("[TRACE] scheduler: worker probe unparked.\n");
    }

    scheduler_cpu_state &cpu = g_cpu[cpu_idx];
    save_interrupt_stack_image(&cpu.last_saved_frame, frame);
    cpu.pending_index = kWorkerThreadIndex;
    cpu.preempt_pending = true;
    cpu.quantum_ticks = 0;
    trace_worker_event("deferred_commit_request", cpu_idx, cpu.current_index, cpu.pending_index);

    if (should_defer_context_switch()) {
        log_info("scheduler: worker probe armed (deferred switch mode).");
        if (kForensicsMode) {
            serial_write("[TRACE] scheduler: deferred mode keeping current frame on worker probe arm.\n");
        }
        g_scheduler.worker_probe_completed = true;
        return frame;
    }

    interrupt_frame *next_frame = commit_pending_preemption(cpu_idx, frame);
    if (g_cpu[cpu_idx].current_index == kWorkerThreadIndex) {
        g_scheduler.worker_probe_switched = true;
        log_info("scheduler: worker probe switched into worker context.");
        if (kForensicsMode) {
            serial_write("[TRACE] scheduler: switched to worker frame.\n");
        }
        if (kEnableWorkerFrameOwnershipProbe) {
            const uint64_t actual_rsp = read_rsp();
            const uint64_t rsp_owner = scheduler_stack_owner_index_for_address(actual_rsp);
            serial_write("[TRACE] scheduler: actual_rsp=");
            serial_write_hex64(actual_rsp);
            serial_write(" actual_rsp_owner=");
            serial_write(stack_owner_label(rsp_owner));
            serial_write(" actual_rsp_class=");
            serial_write(stack_class_label(actual_rsp));
            serial_write(" slot_owner=");
            serial_write(stack_owner_label(g_worker_frame_slot_probe_owner_index));
            serial_write(" slot_class=");
            serial_write(stack_class_label(g_worker_frame_slot_probe_frame_addr));
            serial_write(" slot_frame=");
            serial_write_hex64(g_worker_frame_slot_probe_frame_addr);
            serial_write("\n");
        }
        if (kEnableWorkerFrameChecksumProbe && next_frame != nullptr) {
            g_scheduler.worker_frame_probe_armed = true;
            g_scheduler.worker_frame_probe_reported = false;
            save_interrupt_stack_image(&g_scheduler.worker_probe_switch_frame, next_frame);
        }
    } else {
        log_warn("scheduler: worker probe unparked but did not switch.");
        if (kForensicsMode) {
            serial_write("[TRACE] scheduler: switch to worker frame did not occur.\n");
        }
    }

    g_scheduler.worker_probe_completed = true;
    return next_frame;
}

void request_preemption(uint64_t cpu_idx, interrupt_frame *frame) {
    scheduler_cpu_state &cpu = g_cpu[cpu_idx];
    if (cpu.current_index == kWorkerThreadIndex) {
        trace_worker_phase("request-enter", cpu_idx, frame, cpu.current_index, cpu.pending_index);
    }
    const uint64_t next = find_next_runnable_for_cpu(cpu.current_index, cpu_idx);
    cpu.quantum_ticks = 0;
    if (next == cpu.current_index) {
        cpu.preempt_pending = false;
        if (cpu.current_index == kWorkerThreadIndex) {
            trace_worker_phase("request-nochange", cpu_idx, frame, cpu.current_index, cpu.pending_index);
        }
        return;
    }

    save_interrupt_stack_image(&cpu.last_saved_frame, frame);
    cpu.pending_index = next;
    cpu.preempt_pending = true;
    trace_worker_event("deferred_commit_request", cpu_idx, cpu.current_index, cpu.pending_index);
    if (cpu.current_index == kWorkerThreadIndex || cpu.pending_index == kWorkerThreadIndex) {
        trace_worker_phase("request-ready", cpu_idx, frame, cpu.current_index, cpu.pending_index);
    }
}

interrupt_frame *commit_pending_preemption(uint64_t cpu_idx, interrupt_frame *frame) {
    scheduler_cpu_state &cpu = g_cpu[cpu_idx];
    if (!cpu.preempt_pending) {
        return frame;
    }
    if (kEnableWorkerReentryGuardProbe &&
        cpu.pending_index == kWorkerThreadIndex &&
        g_scheduler.worker_active) {
        panic_worker_reentry("pending switch targets active worker", cpu_idx,
                             cpu.current_index, cpu.pending_index);
    }
    if (cpu.current_index == kWorkerThreadIndex || cpu.pending_index == kWorkerThreadIndex) {
        trace_worker_phase("commit-enter", cpu_idx, frame, cpu.current_index, cpu.pending_index);
    }

    scheduler_thread &current = g_scheduler.threads[cpu.current_index];
    scheduler_thread &next = g_scheduler.threads[cpu.pending_index];
    if (current.stack_image_slot == nullptr || next.stack_image_slot == nullptr) {
        cpu.preempt_pending = false;
        return frame;
    }
    save_interrupt_stack_image(&current.saved_stack_image, frame);
    current.has_resume_frame = true;

    if (!next.has_resume_frame) {
        if (build_bootstrap_frame(cpu.pending_index, frame) == nullptr) {
            cpu.preempt_pending = false;
            return frame;
        }
    }

    interrupt_frame *next_frame = materialize_resume_frame(next);
    if (next_frame == nullptr) {
        cpu.preempt_pending = false;
        return frame;
    }

    if (cpu.pending_index == kWorkerThreadIndex) {
        if (kForensicsMode) {
            serial_write("[TRACE] scheduler: preparing worker iret handoff.\n");
            serial_write("[TRACE] scheduler: worker handoff frame=");
            serial_write_hex64(reinterpret_cast<uint64_t>(next_frame));
            serial_write(" saved=");
            serial_write_hex64(reinterpret_cast<uint64_t>(&next.saved_stack_image));
            serial_write(" rip=");
            serial_write_hex64(next_frame->rip);
            serial_write(" cs=");
            serial_write_hex64(next_frame->cs);
            serial_write(" rflags=");
            serial_write_hex64(next_frame->rflags);
            serial_write(" canary_lo=");
            serial_write_hex64(*reinterpret_cast<const uint64_t *>(next.stack_canary_lo_addr));
            serial_write(" canary_hi=");
            serial_write_hex64(*reinterpret_cast<const uint64_t *>(next.stack_canary_hi_addr));
            serial_write("\n");
        }

        if (kEnableWorkerFrameOwnershipProbe) {
            g_worker_frame_slot_probe_enabled = 1;
            g_worker_frame_slot_probe_frame_addr = reinterpret_cast<uint64_t>(next_frame);
            g_worker_frame_slot_probe_canary_lo_addr = next.stack_canary_lo_addr;
            g_worker_frame_slot_probe_canary_hi_addr = next.stack_canary_hi_addr;
            g_worker_frame_slot_probe_canary_lo_expect = kFrameCanaryLo;
            g_worker_frame_slot_probe_canary_hi_expect = kFrameCanaryHi;
            g_worker_frame_slot_probe_owner_index = cpu.pending_index;
            g_worker_frame_slot_probe_resume_rsp =
                reinterpret_cast<uint64_t>(next.stack_image_slot) + sizeof(interrupt_frame);
        }
    }

    if (cpu.current_index == kWorkerThreadIndex || cpu.pending_index == kWorkerThreadIndex) {
        trace_worker_phase("commit-select", cpu_idx, next_frame, cpu.current_index, cpu.pending_index);
    }
    if (cpu.pending_index == kWorkerThreadIndex) {
        diag_mark_phase(kDiagPhaseWorkerCommitSelect, reinterpret_cast<uint64_t>(next_frame));
    }

    if (kEnableWorkerIRQIFGuard && cpu.pending_index == kWorkerThreadIndex) {
        g_scheduler.worker_if_guard_pending = true;
    }

    const uint64_t previous_index = cpu.current_index;
    const uint64_t next_index = cpu.pending_index;
    const bool previous_ap_owned = thread_is_ap_owned(previous_index);
    const bool next_ap_owned = thread_is_ap_owned(next_index);
    if (cpu_idx > 0 && previous_index != next_index) {
        if (previous_ap_owned && next_ap_owned) {
            __atomic_fetch_add(&g_ap_to_ap_switch_total, 1, __ATOMIC_RELAXED);
            __atomic_fetch_add(&g_ap_to_ap_switch_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
        } else if (previous_index == 0 && next_ap_owned) {
            __atomic_fetch_add(&g_bsp_to_ap_switch_total, 1, __ATOMIC_RELAXED);
            __atomic_fetch_add(&g_bsp_to_ap_switch_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
        }
    }
    cpu.current_index = next_index;
    if (previous_index == kWorkerThreadIndex && cpu.current_index != kWorkerThreadIndex) {
        g_scheduler.worker_active = false;
        trace_worker_event("worker_exit", cpu_idx, previous_index, cpu.current_index);
    }
    if (cpu.current_index == kWorkerThreadIndex) {
        if (kEnableWorkerReentryGuardProbe && g_scheduler.worker_active) {
            panic_worker_reentry("worker entered while active", cpu_idx,
                                 previous_index, cpu.current_index);
        }
        g_scheduler.worker_active = true;
        trace_worker_event("worker_enter", cpu_idx, cpu.current_index, cpu.pending_index);
    }
    cpu.preempt_pending = false;
    g_scheduler.preemptions++;
    g_scheduler.context_switches++;

    if (!__atomic_exchange_n(&g_scheduler.logged_first_preempt, true, __ATOMIC_RELAXED)) {
        log_info("scheduler: timer preemption handoff active.");
    }
    if (!__atomic_exchange_n(&g_scheduler.logged_first_frame_restore, true, __ATOMIC_RELAXED)) {
        log_info("scheduler: interrupt-frame context restore active.");
    }
    if (cpu.current_index == kWorkerThreadIndex) {
        trace_worker_phase("commit-return-worker", cpu_idx, next_frame, cpu.current_index, cpu.pending_index);
    }
    return next_frame;
}
}

bool scheduler_init(void) {
    g_kernel_ss_selector = read_ss();
    for (uint64_t i = 0; i < kMaxThreads; i++) {
        g_scheduler.threads[i].name = 0;
        g_scheduler.threads[i].runnable = false;
        g_scheduler.threads[i].cpu_pinned = false;
        g_scheduler.threads[i].pinned_cpu_index = 0;
        g_scheduler.threads[i].has_resume_frame = false;
        g_scheduler.threads[i].runtime_ticks = 0;
        g_scheduler.threads[i].needs_bootstrap_frame = false;
        g_scheduler.threads[i].entry_point = nullptr;
        g_scheduler.threads[i].stack_image_slot = nullptr;
        g_scheduler.threads[i].stack_base_addr = 0;
        g_scheduler.threads[i].stack_top_addr = 0;
        g_scheduler.threads[i].stack_canary_lo_addr = 0;
        g_scheduler.threads[i].stack_canary_hi_addr = 0;
        uint64_t *saved = reinterpret_cast<uint64_t *>(&g_scheduler.threads[i].saved_stack_image);
        for (uint64_t w = 0; w < 20; w++) {
            saved[w] = 0;
        }
    }
    g_scheduler.ready = false;
    g_scheduler.running = false;
    g_scheduler.thread_count = 0;
    g_scheduler.total_ticks = 0;
    g_scheduler.preemptions = 0;
    g_scheduler.context_switches = 0;
    g_scheduler.logged_first_preempt = false;
    g_scheduler.logged_first_frame_restore = false;
    g_scheduler.logged_deferred_mode = false;
    g_scheduler.logged_deferred_commit = false;
    g_scheduler.logged_irq_delivery_invariants = false;
    g_scheduler.worker_probe_attempted = false;
    g_scheduler.worker_probe_switched = false;
    g_scheduler.worker_probe_completed = false;
    g_scheduler.worker_active = false;
    g_scheduler.worker_irq_trace_count = 0;
    g_scheduler.worker_phase_trace_seq = 0;
    g_scheduler.worker_event_seq = 0;
    g_scheduler.worker_escape_done = false;
    g_scheduler.worker_if_guard_pending = false;
    g_scheduler.worker_second_tick_first_captured = false;
    g_scheduler.worker_second_tick_reported = false;
    g_scheduler.worker_second_tick_first_frame_ptr = 0;
    g_scheduler.worker_second_tick_first_slot_ptr = 0;
    g_scheduler.worker_frame_probe_armed = false;
    g_scheduler.worker_frame_probe_reported = false;
    g_worker_frame_slot_probe_enabled = 0;
    g_worker_frame_slot_probe_owner_index = ~0ULL;
    g_worker_frame_slot_probe_resume_rsp = 0;
    g_worker_rsp_before_sti = 0;
    g_worker_rsp_after_sti = 0;
    g_worker_rsp_first_loop = 0;
    g_worker_rsp_probe_stack_base = 0;
    g_worker_rsp_probe_stack_top = 0;
    g_worker_rsp_probe_enabled = 0;
    for (uint64_t i = 0; i < kMaxCpus; i++) {
        g_ap_worker_thread_for_cpu[i] = kNoThreadIndex;
        g_ap_pinned_thread_for_cpu[i] = kNoThreadIndex;
    }

    if (!add_thread("kernel-main", true, nullptr)) {
        return false;
    }
    if (!add_thread("kernel-worker0", false, select_worker_entry_probe_target())) {
        return false;
    }

    g_scheduler.cpu_count = boot_cpu_count();
    if (g_scheduler.cpu_count == 0 || g_scheduler.cpu_count > kMaxCpus) {
        g_scheduler.cpu_count = 1;
    }
    __atomic_store_n(&g_ipi_preempt_requests_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_dispatch_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_execute_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_maintenance_dispatch_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_maintenance_execute_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_networking_dispatch_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_networking_execute_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_pinned_task_dispatch_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_pinned_task_execute_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_pinned_task_migration_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_pinned_task_migration_enabled, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_pinned_task_last_migration_tick, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_to_ap_switch_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_bsp_to_ap_switch_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_worker_threads_provisioned_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_pinned_threads_provisioned_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_threads_expected_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_skip_rate_limit_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_skip_pending_cap_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_work_drop_total, 0, __ATOMIC_RELAXED);

    for (uint64_t i = 0; i < kMaxCpus; i++) {
        g_cpu[i].online = false;
        g_cpu[i].preempt_pending = false;
        g_cpu[i].current_index = 0;
        g_cpu[i].pending_index = 0;
        g_cpu[i].quantum_ticks = 0;
        __atomic_store_n(&g_ipi_preempt_requests_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_dispatch_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_execute_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_maintenance_dispatch_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_maintenance_execute_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_maintenance_last_enqueue_tick_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_networking_dispatch_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_networking_execute_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_networking_last_enqueue_tick_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_pinned_task_dispatch_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_pinned_task_execute_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_pinned_task_last_enqueue_tick_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_to_ap_switch_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_bsp_to_ap_switch_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_skip_rate_limit_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_skip_pending_cap_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_drop_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_next_sequence_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_head_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_ap_work_tail_by_cpu[i], 0, __ATOMIC_RELAXED);
    }

    const uint64_t cpu_limit = g_scheduler.cpu_count > kMaxCpus ? kMaxCpus : g_scheduler.cpu_count;
    for (uint64_t i = 0; i < kMaxApPinnedTasks; i++) {
        g_ap_pinned_tasks[i].active = false;
        g_ap_pinned_tasks[i].cpu_index = 0;
        g_ap_pinned_tasks[i].run_count = 0;
        g_ap_pinned_tasks[i].seen_cpu_mask = 0;
    }
    uint64_t ap_worker_threads = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit && g_scheduler.thread_count < kMaxThreads; cpu++) {
        if (!add_thread("kernel-ap-worker", true, scheduler_ap_worker_entry)) {
            break;
        }
        g_ap_worker_thread_for_cpu[cpu] = g_scheduler.thread_count - 1;
        pin_thread_to_cpu(g_ap_worker_thread_for_cpu[cpu], cpu);
        ap_worker_threads++;
    }
    uint64_t ap_pinned_threads = 0;
    for (uint64_t cpu = 1; cpu < cpu_limit && g_scheduler.thread_count < kMaxThreads; cpu++) {
        if (!add_thread("kernel-ap-pinned-task", false, scheduler_ap_pinned_task_entry)) {
            break;
        }
        g_ap_pinned_thread_for_cpu[cpu] = g_scheduler.thread_count - 1;
        pin_thread_to_cpu(g_ap_pinned_thread_for_cpu[cpu], cpu);
        ap_pinned_threads++;
    }

    uint64_t bsp_cpu = current_cpu_index();
    if (bsp_cpu >= g_scheduler.cpu_count) {
        bsp_cpu = 0;
    }
    pin_thread_to_cpu(0, bsp_cpu);
    g_cpu[bsp_cpu].online = true;

    g_scheduler.ready = true;
    const uint64_t expected_ap_threads = cpu_limit > 0 ? (cpu_limit - 1) : 0;
    __atomic_store_n(&g_ap_worker_threads_provisioned_total, ap_worker_threads, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_pinned_threads_provisioned_total, ap_pinned_threads, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_threads_expected_total, expected_ap_threads, __ATOMIC_RELAXED);
    log_ap_thread_provision_summary(ap_worker_threads, ap_pinned_threads, expected_ap_threads);
    if (ap_worker_threads != expected_ap_threads || ap_pinned_threads != expected_ap_threads) {
        log_warn("scheduler: AP thread provisioning below CPU count; AP self-test may remain pending.");
    }
    log_info("scheduler: round-robin scaffold initialized with per-CPU handoff.");
    log_info("scheduler: AP worker thread context prepared per CPU.");
    return true;
}

void scheduler_start(void) {
    if (!g_scheduler.ready || g_scheduler.running) {
        return;
    }

    g_scheduler.running = true;
    log_info("scheduler: context switching enabled.");
#if defined(FORTIFY_AP_STRESS)
    log_warn("scheduler: AP stress mode enabled (high-frequency AP enqueue).");
#endif
    if (should_defer_context_switch()) {
        log_warn("scheduler: deferred switch mode active (timer IRQ marks pending only).");
    }
}

interrupt_frame *scheduler_on_timer_tick(interrupt_frame *frame) {
    if (!g_scheduler.ready || !g_scheduler.running || g_scheduler.thread_count == 0) {
        return frame;
    }

    const uint64_t cpu_idx = current_cpu_index();
    if (cpu_idx >= g_scheduler.cpu_count || !g_cpu[cpu_idx].online) {
        return frame;
    }

    scheduler_cpu_state &cpu = g_cpu[cpu_idx];
    g_scheduler.total_ticks++;

    if (kEnableWorkerUnparkProbe &&
        !g_scheduler.worker_probe_attempted &&
        g_scheduler.total_ticks >= kWorkerUnparkProbeTick) {
        frame = activate_worker_probe(cpu_idx, frame);
    }

    if (g_cpu[cpu_idx].current_index == kWorkerThreadIndex) {
        scheduler_thread &worker = g_scheduler.threads[kWorkerThreadIndex];
        const uint64_t n = g_scheduler.worker_irq_trace_count++;
        diag_mark_phase(kDiagPhaseWorkerIrqFrame, reinterpret_cast<uint64_t>(frame));
        if (n < 8 || (n % 32) == 0) {
            if (kForensicsMode) {
                serial_write("[TRACE] scheduler: worker IRQ frame=");
                serial_write_hex64(reinterpret_cast<uint64_t>(frame));
                serial_write(" rip=");
                serial_write_hex64(frame->rip);
                serial_write(" cs=");
                serial_write_hex64(frame->cs);
                serial_write(" rflags=");
                serial_write_hex64(frame->rflags);
                serial_write("\n");
            }
        }

        if (kEnableWorkerFrameOwnershipProbe && (n < 8 || (n % 32) == 0)) {
            serial_write("[TRACE] worker-frame-ownership: irq_frame=");
            serial_write_hex64(reinterpret_cast<uint64_t>(frame));
            serial_write(" slot_frame=");
            serial_write_hex64(reinterpret_cast<uint64_t>(worker.stack_image_slot));
            serial_write(" saved_frame=");
            serial_write_hex64(reinterpret_cast<uint64_t>(&worker.saved_stack_image));
            serial_write(" canary_ok=");
            serial_write(thread_slot_canary_ok(worker) ? "1" : "0");
            serial_write("\n");
        }

        if (kEnableWorkerSecondTickProbe) {
            auto *slot_frame = as_interrupt_frame(worker.stack_image_slot);
            if (!g_scheduler.worker_second_tick_first_captured) {
                g_scheduler.worker_second_tick_first_captured = true;
                g_scheduler.worker_second_tick_first_frame_ptr = reinterpret_cast<uint64_t>(frame);
                g_scheduler.worker_second_tick_first_slot_ptr = reinterpret_cast<uint64_t>(slot_frame);
                save_interrupt_stack_image(&g_scheduler.worker_second_tick_first_irq_frame, frame);
                if (slot_frame != nullptr) {
                    save_interrupt_stack_image(&g_scheduler.worker_second_tick_first_slot_frame, slot_frame);
                }

                serial_write("[TRACE] worker-second-tick: first-worker-irq captured frame_ptr=");
                serial_write_hex64(g_scheduler.worker_second_tick_first_frame_ptr);
                serial_write(" slot_ptr=");
                serial_write_hex64(g_scheduler.worker_second_tick_first_slot_ptr);
                serial_write(" frame_fp=");
                serial_write_hex64(frame_fingerprint(frame));
                serial_write(" slot_fp=");
                serial_write_hex64(frame_fingerprint(slot_frame));
                serial_write(" rip=");
                serial_write_hex64(frame->rip);
                serial_write(" rflags=");
                serial_write_hex64(frame->rflags);
                serial_write("\n");
            } else if (!g_scheduler.worker_second_tick_reported) {
                g_scheduler.worker_second_tick_reported = true;
                serial_write("[TRACE] worker-second-tick: second-worker-irq observed frame_ptr=");
                serial_write_hex64(reinterpret_cast<uint64_t>(frame));
                serial_write(" slot_ptr=");
                serial_write_hex64(reinterpret_cast<uint64_t>(slot_frame));
                serial_write(" frame_fp=");
                serial_write_hex64(frame_fingerprint(frame));
                serial_write(" slot_fp=");
                serial_write_hex64(frame_fingerprint(slot_frame));
                serial_write(" frame_diff_words=");
                serial_write_hex64(frame_word_diff_count(as_interrupt_frame(&g_scheduler.worker_second_tick_first_irq_frame), frame));
                serial_write(" slot_diff_words=");
                serial_write_hex64(frame_word_diff_count(as_interrupt_frame(&g_scheduler.worker_second_tick_first_slot_frame), slot_frame));
                serial_write(" rip=");
                serial_write_hex64(frame->rip);
                serial_write(" rflags=");
                serial_write_hex64(frame->rflags);
                serial_write("\n");
            }
        }

        if (kEnableWorkerFrameChecksumProbe &&
            g_scheduler.worker_frame_probe_armed &&
            !g_scheduler.worker_frame_probe_reported) {
            g_scheduler.worker_frame_probe_reported = true;
            serial_write("[TRACE] worker-frame-probe: switch_gpr_fp=");
            serial_write_hex64(frame_fingerprint_range(as_interrupt_frame(&g_scheduler.worker_probe_switch_frame), 0, 15));
            serial_write(" irq_gpr_fp=");
            serial_write_hex64(frame_fingerprint_range(frame, 0, 15));
            serial_write(" gpr_diff_words=");
            serial_write_hex64(frame_word_diff_count_range(as_interrupt_frame(&g_scheduler.worker_probe_switch_frame), frame, 0, 15));
            serial_write(" switch_iret_fp=");
            serial_write_hex64(frame_fingerprint_range(as_interrupt_frame(&g_scheduler.worker_probe_switch_frame), 17, 3));
            serial_write(" irq_iret_fp=");
            serial_write_hex64(frame_fingerprint_range(frame, 17, 3));
            serial_write(" iret_diff_words=");
            serial_write_hex64(frame_word_diff_count_range(as_interrupt_frame(&g_scheduler.worker_probe_switch_frame), frame, 17, 3));
            serial_write(" switch_vector=");
            serial_write_hex64(as_interrupt_frame(&g_scheduler.worker_probe_switch_frame)->vector);
            serial_write(" irq_vector=");
            serial_write_hex64(frame->vector);
            serial_write("\n");
        }

        if (kEnableWorkerIRQIFGuard && g_scheduler.worker_if_guard_pending) {
            g_scheduler.worker_if_guard_pending = false;
            frame->rflags &= ~static_cast<uint64_t>(0x200);
            serial_write("[TRACE] worker-if-guard: cleared IF on worker IRQ return frame.\n");
        }

        if (kEnableWorkerEscapeProbe && !g_scheduler.worker_escape_done) {
            g_scheduler.worker_escape_done = true;
            serial_write("[TRACE] worker-escape: forcing return to kernel-main frame.\n");

            scheduler_thread &worker = g_scheduler.threads[kWorkerThreadIndex];
            scheduler_thread &main_thread = g_scheduler.threads[0];
            if (worker.stack_image_slot != nullptr && main_thread.stack_image_slot != nullptr) {
                save_interrupt_stack_image(&worker.saved_stack_image, frame);
                worker.has_resume_frame = true;

                g_cpu[cpu_idx].current_index = 0;
                g_cpu[cpu_idx].pending_index = 0;
                g_cpu[cpu_idx].preempt_pending = false;
                g_cpu[cpu_idx].quantum_ticks = 0;
                return materialize_resume_frame(main_thread);
            }
            serial_write("[TRACE] worker-escape: frame slots unavailable.\n");
        }
    }

    cpu.quantum_ticks++;
    g_scheduler.threads[cpu.current_index].runtime_ticks++;
    if (cpu.quantum_ticks >= kDefaultTimesliceTicks) {
        if (cpu.current_index == kWorkerThreadIndex) {
            trace_worker_phase("tick-before-request", cpu_idx, frame, cpu.current_index, cpu.pending_index);
        }
        request_preemption(cpu_idx, frame);
        if (should_defer_context_switch()) {
            if (kForensicsMode && !g_scheduler.logged_deferred_mode) {
                g_scheduler.logged_deferred_mode = true;
                serial_write("[TRACE] scheduler: deferred mode left preemption pending (no IRQ-frame handoff).\n");
            }
            return frame;
        }
        if (cpu.current_index == kWorkerThreadIndex || cpu.pending_index == kWorkerThreadIndex) {
            trace_worker_phase("tick-before-commit", cpu_idx, frame, cpu.current_index, cpu.pending_index);
        }
        interrupt_frame *next_frame = commit_pending_preemption(cpu_idx, frame);
        if (g_cpu[cpu_idx].current_index == kWorkerThreadIndex) {
            trace_worker_phase("tick-after-commit", cpu_idx, next_frame,
                               g_cpu[cpu_idx].current_index, g_cpu[cpu_idx].pending_index);
        }
        return next_frame;
    }
    return frame;
}

bool scheduler_enqueue_ap_maintenance_job(uint64_t cpu_index) {
    if (!g_scheduler.ready || !g_scheduler.running ||
        cpu_index >= g_scheduler.cpu_count || !g_cpu[cpu_index].online) {
        return false;
    }
    return enqueue_periodic_ap_work_kind(
        cpu_index,
        kApWorkKindMaintenanceNop,
        &g_ap_work_maintenance_dispatch_total,
        &g_ap_work_maintenance_dispatch_by_cpu[0],
        &g_ap_work_maintenance_execute_by_cpu[0],
        &g_ap_work_maintenance_last_enqueue_tick_by_cpu[0],
        kApWorkMinIntervalTicksMaintenance,
        g_scheduler.total_ticks);
}

bool scheduler_enqueue_ap_networking_job(uint64_t cpu_index) {
    if (!g_scheduler.ready || !g_scheduler.running ||
        cpu_index >= g_scheduler.cpu_count || !g_cpu[cpu_index].online) {
        return false;
    }
    return enqueue_periodic_ap_work_kind(
        cpu_index,
        kApWorkKindNetworkingNop,
        &g_ap_work_networking_dispatch_total,
        &g_ap_work_networking_dispatch_by_cpu[0],
        &g_ap_work_networking_execute_by_cpu[0],
        &g_ap_work_networking_last_enqueue_tick_by_cpu[0],
        kApWorkMinIntervalTicksNetworking,
        g_scheduler.total_ticks);
}

bool scheduler_start_ap_pinned_demo_tasks(void) {
    if (!g_scheduler.ready) {
        return false;
    }

    bool created_any = false;
    const uint64_t cpu_limit = g_scheduler.cpu_count > kMaxCpus ? kMaxCpus : g_scheduler.cpu_count;
    for (uint64_t cpu = 1; cpu < cpu_limit; cpu++) {
        const uint64_t task_index = cpu - 1;
        if (task_index >= kMaxApPinnedTasks) {
            break;
        }
        const uint64_t thread_index = g_ap_pinned_thread_for_cpu[cpu];
        if (thread_index == kNoThreadIndex || thread_index >= g_scheduler.thread_count) {
            continue;
        }
        g_ap_pinned_tasks[task_index].active = true;
        g_ap_pinned_tasks[task_index].cpu_index = cpu;
        g_ap_pinned_tasks[task_index].run_count = 0;
        g_ap_pinned_tasks[task_index].seen_cpu_mask = 0;
        g_scheduler.threads[thread_index].runnable = true;
        created_any = true;
    }
    if (created_any) {
        __atomic_store_n(&g_ap_pinned_task_last_migration_tick, g_scheduler.total_ticks, __ATOMIC_RELAXED);
    }
    return created_any;
}

void scheduler_stop_ap_pinned_demo_tasks(void) {
    for (uint64_t i = 0; i < kMaxApPinnedTasks; i++) {
        g_ap_pinned_tasks[i].active = false;
    }
    for (uint64_t cpu = 1; cpu < kMaxCpus; cpu++) {
        const uint64_t thread_index = g_ap_pinned_thread_for_cpu[cpu];
        if (thread_index != kNoThreadIndex && thread_index < g_scheduler.thread_count) {
            g_scheduler.threads[thread_index].runnable = false;
        }
    }
}

bool scheduler_force_ap_pinned_task_migration_step(void) {
    if (!g_scheduler.ready || !g_scheduler.running || g_scheduler.cpu_count <= 2) {
        return false;
    }

    const uint64_t ap_cpu_count = g_scheduler.cpu_count - 1;
    bool moved_any = false;
    for (uint64_t task_index = 0; task_index < kMaxApPinnedTasks; task_index++) {
        if (!g_ap_pinned_tasks[task_index].active || ap_cpu_count == 0) {
            continue;
        }
        const uint64_t current_cpu = g_ap_pinned_tasks[task_index].cpu_index;
        if (current_cpu == 0 || current_cpu >= g_scheduler.cpu_count) {
            continue;
        }
        uint64_t next_cpu = (current_cpu - 1 + 1) % ap_cpu_count;
        next_cpu += 1;
        if (next_cpu == current_cpu) {
            continue;
        }
        g_ap_pinned_tasks[task_index].cpu_index = next_cpu;
        __atomic_fetch_add(&g_ap_pinned_task_migration_total, 1, __ATOMIC_RELAXED);
        moved_any = true;
    }
    if (moved_any) {
        __atomic_store_n(&g_ap_pinned_task_last_migration_tick, g_scheduler.total_ticks, __ATOMIC_RELAXED);
    }
    return moved_any;
}

bool scheduler_dispatch_ap_pinned_tasks(void) {
    if (!g_scheduler.ready || !g_scheduler.running) {
        return false;
    }

    const bool migration_enabled =
        __atomic_load_n(&g_ap_pinned_task_migration_enabled, __ATOMIC_RELAXED) != 0;
    if (migration_enabled && g_scheduler.cpu_count > 2) {
        const uint64_t now_ticks = g_scheduler.total_ticks;
        const uint64_t last_migration_tick =
            __atomic_load_n(&g_ap_pinned_task_last_migration_tick, __ATOMIC_RELAXED);
        if (now_ticks >= last_migration_tick &&
            (now_ticks - last_migration_tick) >= kApPinnedTaskMigrationIntervalTicks) {
            (void)scheduler_force_ap_pinned_task_migration_step();
        }
    }

    bool active_any = false;
    for (uint64_t task_index = 0; task_index < kMaxApPinnedTasks; task_index++) {
        const ap_pinned_task &task = g_ap_pinned_tasks[task_index];
        if (!task.active || task.cpu_index >= g_scheduler.cpu_count) {
            continue;
        }
        if (!g_cpu[task.cpu_index].online) {
            continue;
        }
        active_any = true;
    }
    return active_any;
}

uint64_t scheduler_ap_pinned_task_execute_total(void) {
    return __atomic_load_n(&g_ap_work_pinned_task_execute_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_pinned_task_execute_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_pinned_task_execute_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_pinned_task_active_count(void) {
    uint64_t count = 0;
    for (uint64_t i = 0; i < kMaxApPinnedTasks; i++) {
        if (g_ap_pinned_tasks[i].active) {
            count++;
        }
    }
    return count;
}

void scheduler_set_ap_pinned_task_migration_enabled(bool enabled) {
    __atomic_store_n(&g_ap_pinned_task_migration_enabled, enabled ? 1ULL : 0ULL, __ATOMIC_RELAXED);
}

bool scheduler_ap_pinned_task_migration_enabled(void) {
    return __atomic_load_n(&g_ap_pinned_task_migration_enabled, __ATOMIC_RELAXED) != 0;
}

uint64_t scheduler_ap_pinned_task_migration_total(void) {
    return __atomic_load_n(&g_ap_pinned_task_migration_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_pinned_task_seen_cpu_mask(void) {
    uint64_t mask = 0;
    for (uint64_t task_index = 0; task_index < kMaxApPinnedTasks; task_index++) {
        if (!g_ap_pinned_tasks[task_index].active) {
            continue;
        }
        mask |= g_ap_pinned_tasks[task_index].seen_cpu_mask;
    }
    return mask;
}

uint64_t scheduler_ap_pinned_task_assigned_cpu_mask(void) {
    uint64_t mask = 0;
    for (uint64_t task_index = 0; task_index < kMaxApPinnedTasks; task_index++) {
        if (!g_ap_pinned_tasks[task_index].active) {
            continue;
        }
        const uint64_t cpu_index = g_ap_pinned_tasks[task_index].cpu_index;
        if (cpu_index >= g_scheduler.cpu_count || cpu_index >= 64) {
            continue;
        }
        mask |= (1ULL << cpu_index);
    }
    return mask;
}

uint64_t scheduler_ap_to_ap_switch_total(void) {
    return __atomic_load_n(&g_ap_to_ap_switch_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_to_ap_switch_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_to_ap_switch_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_bsp_to_ap_switch_total(void) {
    return __atomic_load_n(&g_bsp_to_ap_switch_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_bsp_to_ap_switch_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_bsp_to_ap_switch_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_worker_threads_provisioned_total(void) {
    return __atomic_load_n(&g_ap_worker_threads_provisioned_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_pinned_threads_provisioned_total(void) {
    return __atomic_load_n(&g_ap_pinned_threads_provisioned_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_threads_expected_total(void) {
    return __atomic_load_n(&g_ap_threads_expected_total, __ATOMIC_RELAXED);
}

bool cpu_has_active_pinned_demo_task(uint64_t cpu_idx) {
    if (cpu_idx == 0 || cpu_idx >= g_scheduler.cpu_count || cpu_idx >= kMaxCpus) {
        return false;
    }
    const uint64_t task_index = cpu_idx - 1;
    if (task_index >= kMaxApPinnedTasks) {
        return false;
    }
    const ap_pinned_task &task = g_ap_pinned_tasks[task_index];
    if (!task.active || task.cpu_index != cpu_idx) {
        return false;
    }
    const uint64_t thread_index = g_ap_pinned_thread_for_cpu[cpu_idx];
    return thread_index != kNoThreadIndex &&
           thread_index < g_scheduler.thread_count &&
           thread_runnable_on_cpu(thread_index, cpu_idx);
}

interrupt_frame *scheduler_on_ipi_kick(interrupt_frame *frame) {
    if (!g_scheduler.ready || !g_scheduler.running || g_scheduler.thread_count == 0) {
        return frame;
    }

    const uint64_t cpu_idx = current_cpu_index();
    if (cpu_idx >= g_scheduler.cpu_count || !g_cpu[cpu_idx].online) {
        return frame;
    }

    __atomic_fetch_add(&g_ipi_preempt_requests_total, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_ipi_preempt_requests_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
    const uint64_t sequence = __atomic_add_fetch(&g_ap_work_next_sequence_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
    const bool scheduler_enqueued = enqueue_ap_work_item(cpu_idx, kApWorkKindSchedulerKick, sequence);
    if (scheduler_enqueued) {
        __atomic_fetch_add(&g_ap_work_dispatch_total, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&g_ap_work_dispatch_by_cpu[cpu_idx], 1, __ATOMIC_RELAXED);
    }
    const uint64_t now_ticks = g_scheduler.total_ticks;
    const bool maintenance_enqueued = enqueue_periodic_ap_work_kind(
        cpu_idx,
        kApWorkKindMaintenanceNop,
        &g_ap_work_maintenance_dispatch_total,
        &g_ap_work_maintenance_dispatch_by_cpu[0],
        &g_ap_work_maintenance_execute_by_cpu[0],
        &g_ap_work_maintenance_last_enqueue_tick_by_cpu[0],
        kApWorkMinIntervalTicksMaintenance,
        now_ticks);
    const bool networking_enqueued = enqueue_periodic_ap_work_kind(
        cpu_idx,
        kApWorkKindNetworkingNop,
        &g_ap_work_networking_dispatch_total,
        &g_ap_work_networking_dispatch_by_cpu[0],
        &g_ap_work_networking_execute_by_cpu[0],
        &g_ap_work_networking_last_enqueue_tick_by_cpu[0],
        kApWorkMinIntervalTicksNetworking,
        now_ticks);
    const bool any_enqueued = scheduler_enqueued || maintenance_enqueued || networking_enqueued;
    const bool pinned_task_active = cpu_has_active_pinned_demo_task(cpu_idx);

    scheduler_cpu_state &cpu = g_cpu[cpu_idx];
    if (cpu.preempt_pending) {
        return frame;
    }
    const bool queue_has_items = ap_work_queue_has_items(cpu_idx);
    const uint64_t ap_worker_thread =
        (cpu_idx < kMaxCpus) ? g_ap_worker_thread_for_cpu[cpu_idx] : kNoThreadIndex;
    if ((any_enqueued || queue_has_items) &&
        ap_worker_thread != kNoThreadIndex &&
        cpu.current_index != ap_worker_thread) {
        save_interrupt_stack_image(&cpu.last_saved_frame, frame);
        cpu.pending_index = ap_worker_thread;
        cpu.preempt_pending = true;
        cpu.quantum_ticks = 0;
    } else if (pinned_task_active) {
        cpu.quantum_ticks++;
        g_scheduler.threads[cpu.current_index].runtime_ticks++;
        if (cpu.quantum_ticks >= kDefaultTimesliceTicks) {
            request_preemption(cpu_idx, frame);
        }
    } else if (!queue_has_items && !any_enqueued) {
        return frame;
    } else {
        request_preemption(cpu_idx, frame);
    }
    if (!cpu.preempt_pending) {
        return frame;
    }

    return commit_pending_preemption(cpu_idx, frame);
}

bool scheduler_service_deferred_switch(void) {
    if (!g_scheduler.ready || !g_scheduler.running || !should_defer_context_switch()) {
        return false;
    }

    const uint64_t cpu_idx = current_cpu_index();
    if (cpu_idx >= g_scheduler.cpu_count || !g_cpu[cpu_idx].online) {
        return false;
    }

    __asm__ volatile ("cli" : : : "memory");

    scheduler_cpu_state &cpu = g_cpu[cpu_idx];
    if (!cpu.preempt_pending || cpu.pending_index == cpu.current_index) {
        __asm__ volatile ("sti" : : : "memory");
        return false;
    }

    interrupt_frame *seed = as_interrupt_frame(&cpu.last_saved_frame);
    if (seed->rip == 0 || seed->cs == 0) {
        __asm__ volatile ("sti" : : : "memory");
        return false;
    }

    trace_worker_event("deferred_commit_execute", cpu_idx, cpu.current_index, cpu.pending_index);
    if (!g_scheduler.logged_irq_delivery_invariants) {
        g_scheduler.logged_irq_delivery_invariants = true;
        interrupts_log_delivery_invariants("pre-deferred-commit");
    }
    interrupt_frame *next_frame = commit_pending_preemption(cpu_idx, seed);
    if (next_frame == nullptr || next_frame == seed) {
        __asm__ volatile ("sti" : : : "memory");
        return false;
    }

    if (cpu.current_index == kWorkerThreadIndex) {
        const scheduler_thread &worker = g_scheduler.threads[kWorkerThreadIndex];
        g_worker_rsp_before_sti = 0;
        g_worker_rsp_after_sti = 0;
        g_worker_rsp_first_loop = 0;
        g_worker_rsp_probe_stack_base = worker.stack_base_addr;
        g_worker_rsp_probe_stack_top = worker.stack_top_addr;
        g_worker_rsp_probe_enabled = kEnableWorkerLiveRspAsmProbe ? 1 : 0;
        const uint64_t resume_rsp = reinterpret_cast<uint64_t>(worker.stack_image_slot) + sizeof(interrupt_frame);
        if (kForensicsMode) {
            serial_write("[TRACE] worker-stack-check base=");
            serial_write_hex64(worker.stack_base_addr);
            serial_write(" top=");
            serial_write_hex64(worker.stack_top_addr);
            serial_write(" slot=");
            serial_write_hex64(reinterpret_cast<uint64_t>(worker.stack_image_slot));
            serial_write(" resume_rsp=");
            serial_write_hex64(resume_rsp);
            serial_write(" in_range=");
            serial_write((resume_rsp >= worker.stack_base_addr && resume_rsp <= worker.stack_top_addr) ? "1" : "0");
            serial_write(" remaining=");
            if (resume_rsp >= worker.stack_base_addr) {
                serial_write_hex64(resume_rsp - worker.stack_base_addr);
            } else {
                serial_write_hex64(0);
            }
            serial_write(" align=");
            serial_write_hex64(resume_rsp & 0x0f);
            serial_write(" canonical=");
            serial_write(is_canonical_address(resume_rsp) ? "1" : "0");
            serial_write(" canary_lo=");
            serial_write_hex64(*reinterpret_cast<const uint64_t *>(worker.stack_canary_lo_addr));
            serial_write(" canary_hi=");
            serial_write_hex64(*reinterpret_cast<const uint64_t *>(worker.stack_canary_hi_addr));
            serial_write("\n");
        }
    }

    if (kForensicsMode) {
        serial_write("[TRACE] scheduler: deferred-commit frame=");
        serial_write_hex64(reinterpret_cast<uint64_t>(next_frame));
        serial_write(" rip=");
        serial_write_hex64(next_frame->rip);
        serial_write(" cs=");
        serial_write_hex64(next_frame->cs);
        serial_write(" rflags=");
        serial_write_hex64(next_frame->rflags);
        serial_write("\n");
    }

    if (kForensicsMode && !g_scheduler.logged_deferred_commit) {
        g_scheduler.logged_deferred_commit = true;
        serial_write("[TRACE] scheduler: deferred mode committing pending switch from thread context.\n");
    }

    scheduler_iret_to_frame(next_frame);
}

bool scheduler_yield(void) {
    if (!g_scheduler.ready) {
        return false;
    }
    const uint64_t cpu_idx = current_cpu_index();
    if (cpu_idx >= g_scheduler.cpu_count || !g_cpu[cpu_idx].online) {
        return false;
    }
    scheduler_cpu_state &cpu = g_cpu[cpu_idx];
    const uint64_t next = find_next_runnable_for_cpu(cpu.current_index, cpu_idx);
    if (next == cpu.current_index) {
        return true;
    }
    cpu.current_index = next;
    cpu.quantum_ticks = 0;
    g_scheduler.preemptions++;
    return true;
}

uint64_t scheduler_tick_count(void) {
    return g_scheduler.total_ticks;
}

uint64_t scheduler_preempt_count(void) {
    return g_scheduler.preemptions;
}

uint64_t scheduler_ipi_preempt_requests_total(void) {
    return __atomic_load_n(&g_ipi_preempt_requests_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ipi_preempt_requests_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ipi_preempt_requests_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_dispatch_total(void) {
    return __atomic_load_n(&g_ap_work_dispatch_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_dispatch_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_dispatch_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_execute_total(void) {
    return __atomic_load_n(&g_ap_work_execute_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_execute_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_execute_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_maintenance_dispatch_total(void) {
    return __atomic_load_n(&g_ap_work_maintenance_dispatch_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_maintenance_dispatch_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_maintenance_dispatch_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_maintenance_execute_total(void) {
    return __atomic_load_n(&g_ap_work_maintenance_execute_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_maintenance_execute_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_maintenance_execute_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_networking_dispatch_total(void) {
    return __atomic_load_n(&g_ap_work_networking_dispatch_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_networking_dispatch_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_networking_dispatch_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_networking_execute_total(void) {
    return __atomic_load_n(&g_ap_work_networking_execute_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_networking_execute_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_networking_execute_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_drop_total(void) {
    return __atomic_load_n(&g_ap_work_drop_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_skip_rate_limit_total(void) {
    return __atomic_load_n(&g_ap_work_skip_rate_limit_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_skip_rate_limit_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_skip_rate_limit_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_skip_pending_cap_total(void) {
    return __atomic_load_n(&g_ap_work_skip_pending_cap_total, __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_skip_pending_cap_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_skip_pending_cap_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_drop_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return __atomic_load_n(&g_ap_work_drop_by_cpu[cpu_index], __ATOMIC_RELAXED);
}

uint64_t scheduler_ap_work_queue_depth_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    const uint32_t head = __atomic_load_n(&g_ap_work_head_by_cpu[cpu_index], __ATOMIC_ACQUIRE);
    const uint32_t tail = __atomic_load_n(&g_ap_work_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (head >= tail) {
        return head - tail;
    }
    return kApWorkQueueCapacity - (tail - head);
}

uint64_t scheduler_current_thread_index(void) {
    if (!g_scheduler.ready || g_scheduler.thread_count == 0) {
        return 0;
    }
    const uint64_t cpu_idx = current_cpu_index();
    if (cpu_idx >= g_scheduler.cpu_count || !g_cpu[cpu_idx].online) {
        return 0;
    }
    return g_cpu[cpu_idx].current_index;
}

const char *scheduler_current_thread_name(void) {
    if (!g_scheduler.ready || g_scheduler.thread_count == 0) {
        return "none";
    }
    const uint64_t cpu_idx = current_cpu_index();
    if (cpu_idx >= g_scheduler.cpu_count || !g_cpu[cpu_idx].online) {
        return "none";
    }
    return g_scheduler.threads[g_cpu[cpu_idx].current_index].name;
}

uint64_t scheduler_stack_owner_index_for_address(uint64_t address) {
    for (uint64_t i = 0; i < g_scheduler.thread_count; i++) {
        const scheduler_thread &thread = g_scheduler.threads[i];
        if (address >= thread.stack_base_addr && address < thread.stack_top_addr) {
            return i;
        }
    }
    return ~0ULL;
}

void scheduler_mark_current_cpu_online(void) {
    if (!g_scheduler.ready) {
        return;
    }
    const uint64_t cpu_idx = current_cpu_index();
    if (cpu_idx >= g_scheduler.cpu_count || cpu_idx >= kMaxCpus) {
        return;
    }

    scheduler_cpu_state &cpu = g_cpu[cpu_idx];
    if (!cpu.online) {
        cpu.online = true;
        cpu.preempt_pending = false;
        cpu.current_index = 0;
        cpu.pending_index = 0;
        cpu.quantum_ticks = 0;
    }
}

uint64_t scheduler_online_cpu_count(void) {
    if (!g_scheduler.ready) {
        return 0;
    }

    uint64_t online = 0;
    const uint64_t limit = g_scheduler.cpu_count > kMaxCpus ? kMaxCpus : g_scheduler.cpu_count;
    for (uint64_t i = 0; i < limit; i++) {
        if (g_cpu[i].online) {
            online++;
        }
    }
    return online;
}
