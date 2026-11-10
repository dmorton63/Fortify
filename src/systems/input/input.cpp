#include "fortify/systems/logging/log.h"
#include "fortify/systems/scheduler/scheduler.h"
#include "fortify/systems/subsystems/subsystems.h"

namespace {
constexpr uint64_t kMaxCpus = 64;
constexpr uint32_t kInputQueueCapacity = 32;
constexpr uint32_t kInputMessageQueueCapacity = 128;
constexpr uint64_t kInputApDrainBatch = 4;
constexpr uint16_t kPs2DataPort = 0x60;
constexpr uint16_t kPs2StatusPort = 0x64;
volatile uint64_t g_poll_stride_ticks = 1;
volatile uint64_t g_queue_trigger = 1;

struct input_event {
    uint8_t scancode;
};

volatile uint64_t g_keyboard_irq_total = 0;
volatile uint64_t g_keyboard_irq_drop_total = 0;
volatile uint64_t g_ap_deferred_job_total = 0;
volatile uint64_t g_input_message_sequence = 0;
volatile uint64_t g_next_target_ap = 1;
volatile bool g_shift_active = false;
volatile uint32_t g_input_head_by_cpu[kMaxCpus] = {};
volatile uint32_t g_input_tail_by_cpu[kMaxCpus] = {};
input_event g_input_queue_by_cpu[kMaxCpus][kInputQueueCapacity] = {};
volatile uint32_t g_input_message_head = 0;
volatile uint32_t g_input_message_tail = 0;
input_message g_input_message_queue[kInputMessageQueueCapacity] = {};

uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

bool pick_target_ap(uint64_t *out_cpu) {
    if (out_cpu == 0) {
        return false;
    }
    const uint64_t total = smp_total_cpu_count();
    if (total <= 1) {
        return false;
    }
    uint64_t next = __atomic_load_n(&g_next_target_ap, __ATOMIC_RELAXED);
    if (next >= total || next == 0) {
        next = 1;
    }
    *out_cpu = next;
    next++;
    if (next >= total) {
        next = 1;
    }
    __atomic_store_n(&g_next_target_ap, next, __ATOMIC_RELAXED);
    return true;
}

uint32_t queue_depth_for_cpu(uint64_t cpu_index) {
    const uint32_t head = __atomic_load_n(&g_input_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_input_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (tail >= head) {
        return tail - head;
    }
    return (kInputQueueCapacity - head) + tail;
}

bool enqueue_input_event(uint64_t cpu_index, uint8_t scancode) {
    if (cpu_index >= kMaxCpus) {
        return false;
    }
    const uint32_t tail = __atomic_load_n(&g_input_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t next_tail = (tail + 1) % kInputQueueCapacity;
    const uint32_t head = __atomic_load_n(&g_input_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (next_tail == head) {
        return false;
    }
    g_input_queue_by_cpu[cpu_index][tail].scancode = scancode;
    __atomic_store_n(&g_input_tail_by_cpu[cpu_index], next_tail, __ATOMIC_RELAXED);
    return true;
}

bool dequeue_input_event(uint64_t cpu_index, input_event *out_event) {
    if (cpu_index >= kMaxCpus || out_event == 0) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_input_head_by_cpu[cpu_index], __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_input_tail_by_cpu[cpu_index], __ATOMIC_RELAXED);
    if (head == tail) {
        return false;
    }
    *out_event = g_input_queue_by_cpu[cpu_index][head];
    __atomic_store_n(&g_input_head_by_cpu[cpu_index], (head + 1) % kInputQueueCapacity, __ATOMIC_RELAXED);
    return true;
}

bool enqueue_input_message(const input_message &message) {
    const uint32_t tail = __atomic_load_n(&g_input_message_tail, __ATOMIC_RELAXED);
    const uint32_t next_tail = (tail + 1) % kInputMessageQueueCapacity;
    const uint32_t head = __atomic_load_n(&g_input_message_head, __ATOMIC_RELAXED);
    if (next_tail == head) {
        return false;
    }
    g_input_message_queue[tail] = message;
    __atomic_store_n(&g_input_message_tail, next_tail, __ATOMIC_RELAXED);
    return true;
}

bool decode_set1_scancode_to_char(uint8_t scancode, char *out_char);

bool decode_set1_scancode_to_message(uint8_t scancode, input_message *out_message) {
    if (out_message == 0) {
        return false;
    }
    char ch = '\0';
    if (!decode_set1_scancode_to_char(scancode, &ch)) {
        return false;
    }
    if (ch == '\n') {
        out_message->type = input_message_type::key_submit;
        out_message->value = '\0';
        out_message->raw_scancode = scancode;
        return true;
    }
    if (ch == '\b') {
        out_message->type = input_message_type::key_backspace;
        out_message->value = '\0';
        out_message->raw_scancode = scancode;
        return true;
    }
    out_message->type = input_message_type::key_char;
    out_message->value = ch;
    out_message->raw_scancode = scancode;
    return true;
}

bool decode_set1_scancode_to_char(uint8_t scancode, char *out_char) {
    if (out_char == 0) {
        return false;
    }
    if (scancode == 0x2a || scancode == 0x36) {
        __atomic_store_n(&g_shift_active, true, __ATOMIC_RELAXED);
        return false;
    }
    if (scancode == 0xaa || scancode == 0xb6) {
        __atomic_store_n(&g_shift_active, false, __ATOMIC_RELAXED);
        return false;
    }
    if ((scancode & 0x80U) != 0) {
        return false;
    }
    if (scancode == 0x1c) {
        *out_char = '\n';
        return true;
    }
    if (scancode == 0x0e) {
        *out_char = '\b';
        return true;
    }
    const bool shift = __atomic_load_n(&g_shift_active, __ATOMIC_RELAXED);
    switch (scancode) {
        case 0x02: *out_char = shift ? '!' : '1'; return true;
        case 0x03: *out_char = shift ? '@' : '2'; return true;
        case 0x04: *out_char = shift ? '#' : '3'; return true;
        case 0x05: *out_char = shift ? '$' : '4'; return true;
        case 0x06: *out_char = shift ? '%' : '5'; return true;
        case 0x07: *out_char = shift ? '^' : '6'; return true;
        case 0x08: *out_char = shift ? '&' : '7'; return true;
        case 0x09: *out_char = shift ? '*' : '8'; return true;
        case 0x0a: *out_char = shift ? '(' : '9'; return true;
        case 0x0b: *out_char = shift ? ')' : '0'; return true;
        case 0x0c: *out_char = shift ? '_' : '-'; return true;
        case 0x0d: *out_char = shift ? '+' : '='; return true;
        case 0x10: *out_char = shift ? 'Q' : 'q'; return true;
        case 0x11: *out_char = shift ? 'W' : 'w'; return true;
        case 0x12: *out_char = shift ? 'E' : 'e'; return true;
        case 0x13: *out_char = shift ? 'R' : 'r'; return true;
        case 0x14: *out_char = shift ? 'T' : 't'; return true;
        case 0x15: *out_char = shift ? 'Y' : 'y'; return true;
        case 0x16: *out_char = shift ? 'U' : 'u'; return true;
        case 0x17: *out_char = shift ? 'I' : 'i'; return true;
        case 0x18: *out_char = shift ? 'O' : 'o'; return true;
        case 0x19: *out_char = shift ? 'P' : 'p'; return true;
        case 0x1a: *out_char = shift ? '{' : '['; return true;
        case 0x1b: *out_char = shift ? '}' : ']'; return true;
        case 0x1e: *out_char = shift ? 'A' : 'a'; return true;
        case 0x1f: *out_char = shift ? 'S' : 's'; return true;
        case 0x20: *out_char = shift ? 'D' : 'd'; return true;
        case 0x21: *out_char = shift ? 'F' : 'f'; return true;
        case 0x22: *out_char = shift ? 'G' : 'g'; return true;
        case 0x23: *out_char = shift ? 'H' : 'h'; return true;
        case 0x24: *out_char = shift ? 'J' : 'j'; return true;
        case 0x25: *out_char = shift ? 'K' : 'k'; return true;
        case 0x26: *out_char = shift ? 'L' : 'l'; return true;
        case 0x27: *out_char = shift ? ':' : ';'; return true;
        case 0x28: *out_char = shift ? '"' : '\''; return true;
        case 0x29: *out_char = shift ? '~' : '`'; return true;
        case 0x2b: *out_char = shift ? '|' : '\\'; return true;
        case 0x2c: *out_char = shift ? 'Z' : 'z'; return true;
        case 0x2d: *out_char = shift ? 'X' : 'x'; return true;
        case 0x2e: *out_char = shift ? 'C' : 'c'; return true;
        case 0x2f: *out_char = shift ? 'V' : 'v'; return true;
        case 0x30: *out_char = shift ? 'B' : 'b'; return true;
        case 0x31: *out_char = shift ? 'N' : 'n'; return true;
        case 0x32: *out_char = shift ? 'M' : 'm'; return true;
        case 0x33: *out_char = shift ? '<' : ','; return true;
        case 0x34: *out_char = shift ? '>' : '.'; return true;
        case 0x35: *out_char = shift ? '?' : '/'; return true;
        case 0x39: *out_char = ' '; return true;
        default: break;
    }
    return false;
}
}

bool input_init(void) {
    __atomic_store_n(&g_keyboard_irq_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_keyboard_irq_drop_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_ap_deferred_job_total, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_input_message_sequence, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_next_target_ap, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&g_shift_active, false, __ATOMIC_RELAXED);
    __atomic_store_n(&g_input_message_head, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_input_message_tail, 0, __ATOMIC_RELAXED);
    for (uint64_t i = 0; i < kMaxCpus; i++) {
        __atomic_store_n(&g_input_head_by_cpu[i], 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_input_tail_by_cpu[i], 0, __ATOMIC_RELAXED);
    }
    log_info("input: keyboard IRQ queue + AP deferred processing ready.");
    return true;
}

void input_on_keyboard_irq(void) {
    __atomic_fetch_add(&g_keyboard_irq_total, 1, __ATOMIC_RELAXED);
    if ((inb(kPs2StatusPort) & 0x01U) == 0) {
        __atomic_fetch_add(&g_keyboard_irq_drop_total, 1, __ATOMIC_RELAXED);
        return;
    }
    const uint8_t scancode = inb(kPs2DataPort);
    uint64_t target_cpu = 0;
    const bool target_ok = pick_target_ap(&target_cpu);
    input_message message = {};
    if (decode_set1_scancode_to_message(scancode, &message)) {
        message.route_cpu = target_ok ? static_cast<uint8_t>(target_cpu) : 0;
        message.modifiers = __atomic_load_n(&g_shift_active, __ATOMIC_RELAXED) ? 0x01U : 0U;
        message.source = input_message_source::keyboard_irq;
        message.tick = timer_ticks();
        message.sequence = __atomic_add_fetch(&g_input_message_sequence, 1, __ATOMIC_RELAXED);
        if (!enqueue_input_message(message)) {
            __atomic_fetch_add(&g_keyboard_irq_drop_total, 1, __ATOMIC_RELAXED);
        }
    }
    if (!target_ok || !enqueue_input_event(target_cpu, scancode)) {
        __atomic_fetch_add(&g_keyboard_irq_drop_total, 1, __ATOMIC_RELAXED);
    }
}

bool input_poll_ap_processing_trigger(uint64_t cpu_index, uint64_t tick) {
    if (cpu_index == 0 || cpu_index >= kMaxCpus) {
        return false;
    }
    const uint64_t stride = __atomic_load_n(&g_poll_stride_ticks, __ATOMIC_RELAXED);
    if (stride == 0 || (tick % stride) != 0) {
        return false;
    }
    const uint64_t trigger = __atomic_load_n(&g_queue_trigger, __ATOMIC_RELAXED);
    if (trigger == 0 || queue_depth_for_cpu(cpu_index) < trigger) {
        return false;
    }
    return scheduler_enqueue_ap_maintenance_job(cpu_index);
}

bool input_set_ap_processing_trigger_config(uint64_t poll_stride_ticks, uint64_t queue_trigger) {
    if (poll_stride_ticks == 0 || queue_trigger == 0) {
        return false;
    }
    __atomic_store_n(&g_poll_stride_ticks, poll_stride_ticks, __ATOMIC_RELAXED);
    __atomic_store_n(&g_queue_trigger, queue_trigger, __ATOMIC_RELAXED);
    return true;
}

void input_process_ap_deferred_job(uint64_t cpu_index, uint64_t sequence) {
    (void)sequence;
    bool drained_event = false;
    for (uint64_t i = 0; i < kInputApDrainBatch; i++) {
        input_event event = {};
        if (!dequeue_input_event(cpu_index, &event)) {
            break;
        }
        (void)event.scancode;
        drained_event = true;
    }
    if (drained_event) {
        __atomic_fetch_add(&g_ap_deferred_job_total, 1, __ATOMIC_RELAXED);
    }
}

bool input_read_char(char *out_char) {
    if (out_char == 0) {
        return false;
    }
    input_message message = {};
    if (!input_read_message(&message)) {
        return false;
    }
    if (message.type == input_message_type::key_submit) {
        *out_char = '\n';
        return true;
    }
    if (message.type == input_message_type::key_backspace) {
        *out_char = '\b';
        return true;
    }
    *out_char = message.value;
    return true;
}

bool input_read_message(input_message *out_message) {
    if (out_message == 0) {
        return false;
    }
    const uint32_t head = __atomic_load_n(&g_input_message_head, __ATOMIC_RELAXED);
    const uint32_t tail = __atomic_load_n(&g_input_message_tail, __ATOMIC_RELAXED);
    if (head == tail) {
        return false;
    }
    *out_message = g_input_message_queue[head];
    __atomic_store_n(&g_input_message_head, (head + 1) % kInputMessageQueueCapacity, __ATOMIC_RELAXED);
    return true;
}

uint64_t input_keyboard_irq_total(void) {
    return __atomic_load_n(&g_keyboard_irq_total, __ATOMIC_RELAXED);
}

uint64_t input_keyboard_irq_drop_total(void) {
    return __atomic_load_n(&g_keyboard_irq_drop_total, __ATOMIC_RELAXED);
}

uint64_t input_ap_deferred_job_total(void) {
    return __atomic_load_n(&g_ap_deferred_job_total, __ATOMIC_RELAXED);
}

uint64_t input_queue_depth_for_cpu(uint64_t cpu_index) {
    if (cpu_index >= kMaxCpus) {
        return 0;
    }
    return static_cast<uint64_t>(queue_depth_for_cpu(cpu_index));
}
