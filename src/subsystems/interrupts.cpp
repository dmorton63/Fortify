#include "fortify/diag.h"
#include "fortify/interrupts.h"
#include "fortify/log.h"
#include "fortify/scheduler.h"
#include "fortify/serial.h"
#include "fortify/subsystems.h"

#include <stdint.h>

extern "C" volatile uint64_t g_irq0_handoff_probe_armed = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_rip = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_cs = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_rflags = 0;
extern "C" volatile uint64_t g_irq0_handoff_gpr_probe_armed = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_r15 = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_r14 = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_r13 = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_r12 = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_rbp = 0;
extern "C" volatile uint64_t g_irq0_handoff_expect_rbx = 0;
extern "C" volatile uint64_t g_worker_second_irq_stub_probe_armed = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_enabled = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_frame_addr = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_canary_lo_addr = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_canary_hi_addr = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_canary_lo_expect = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_canary_hi_expect = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_c_mismatch_count = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_asm_mismatch_count = 0;
extern "C" volatile uint64_t g_worker_frame_slot_probe_owner_index = ~0ULL;
extern "C" volatile uint64_t g_worker_frame_slot_probe_resume_rsp = 0;
extern "C" uint8_t irq_dispatch_stack;
extern "C" uint8_t irq_dispatch_stack_top;

namespace {
struct __attribute__((packed)) idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};

struct __attribute__((packed)) idtr {
    uint16_t limit;
    uint64_t base;
};

static idt_entry g_idt[256] = {};
static volatile uint64_t g_timer_ticks = 0;
static uint64_t g_worker_irq0_trace_count = 0;
static uint64_t g_irq0_return_probe_count = 0;
static uint64_t g_worker_irq0_entry_count = 0;
static bool g_worker_irq0_mask_probe_armed = true;

constexpr uint8_t kMasterPicCmd = 0x20;
constexpr uint8_t kMasterPicData = 0x21;
constexpr uint8_t kSlavePicCmd = 0xA0;
constexpr uint8_t kSlavePicData = 0xA1;
constexpr uint8_t kPicEoi = 0x20;
constexpr uint8_t kIrqBase = 32;
constexpr bool kForensicsMode = false;
constexpr bool kEnablePitSlowTickProbe = false;
constexpr bool kEnableMaskIrq0AfterFirstWorkerEoiProbe = false;
constexpr bool kEnableIrq0DispatchReturnMarker = kForensicsMode;
constexpr bool kEnableIrq0ReturnTripleProbe = kForensicsMode;
constexpr bool kEnableIrq0HandoffCompareProbe = kForensicsMode;
constexpr bool kEnableIrq0HandoffGprCompareProbe = kForensicsMode;
constexpr bool kEnableSecondWorkerIrqEntryStubProbe = kForensicsMode;

extern "C" void isr0(void);
extern "C" void isr1(void);
extern "C" void isr2(void);
extern "C" void isr3(void);
extern "C" void isr4(void);
extern "C" void isr5(void);
extern "C" void isr6(void);
extern "C" void isr7(void);
extern "C" void isr8(void);
extern "C" void isr9(void);
extern "C" void isr10(void);
extern "C" void isr11(void);
extern "C" void isr12(void);
extern "C" void isr13(void);
extern "C" void isr14(void);
extern "C" void isr15(void);
extern "C" void isr16(void);
extern "C" void isr17(void);
extern "C" void isr18(void);
extern "C" void isr19(void);
extern "C" void isr20(void);
extern "C" void isr21(void);
extern "C" void isr22(void);
extern "C" void isr23(void);
extern "C" void isr24(void);
extern "C" void isr25(void);
extern "C" void isr26(void);
extern "C" void isr27(void);
extern "C" void isr28(void);
extern "C" void isr29(void);
extern "C" void isr30(void);
extern "C" void isr31(void);
extern "C" void isr32(void);
extern "C" void isr33(void);
extern "C" void isr34(void);
extern "C" void isr35(void);
extern "C" void isr36(void);
extern "C" void isr37(void);
extern "C" void isr38(void);
extern "C" void isr39(void);
extern "C" void isr40(void);
extern "C" void isr41(void);
extern "C" void isr42(void);
extern "C" void isr43(void);
extern "C" void isr44(void);
extern "C" void isr45(void);
extern "C" void isr46(void);
extern "C" void isr47(void);

void *k_exception_stubs[32] = {
    reinterpret_cast<void *>(isr0), reinterpret_cast<void *>(isr1),
    reinterpret_cast<void *>(isr2), reinterpret_cast<void *>(isr3),
    reinterpret_cast<void *>(isr4), reinterpret_cast<void *>(isr5),
    reinterpret_cast<void *>(isr6), reinterpret_cast<void *>(isr7),
    reinterpret_cast<void *>(isr8), reinterpret_cast<void *>(isr9),
    reinterpret_cast<void *>(isr10), reinterpret_cast<void *>(isr11),
    reinterpret_cast<void *>(isr12), reinterpret_cast<void *>(isr13),
    reinterpret_cast<void *>(isr14), reinterpret_cast<void *>(isr15),
    reinterpret_cast<void *>(isr16), reinterpret_cast<void *>(isr17),
    reinterpret_cast<void *>(isr18), reinterpret_cast<void *>(isr19),
    reinterpret_cast<void *>(isr20), reinterpret_cast<void *>(isr21),
    reinterpret_cast<void *>(isr22), reinterpret_cast<void *>(isr23),
    reinterpret_cast<void *>(isr24), reinterpret_cast<void *>(isr25),
    reinterpret_cast<void *>(isr26), reinterpret_cast<void *>(isr27),
    reinterpret_cast<void *>(isr28), reinterpret_cast<void *>(isr29),
    reinterpret_cast<void *>(isr30), reinterpret_cast<void *>(isr31)
};

void *k_irq_stubs[16] = {
    reinterpret_cast<void *>(isr32), reinterpret_cast<void *>(isr33),
    reinterpret_cast<void *>(isr34), reinterpret_cast<void *>(isr35),
    reinterpret_cast<void *>(isr36), reinterpret_cast<void *>(isr37),
    reinterpret_cast<void *>(isr38), reinterpret_cast<void *>(isr39),
    reinterpret_cast<void *>(isr40), reinterpret_cast<void *>(isr41),
    reinterpret_cast<void *>(isr42), reinterpret_cast<void *>(isr43),
    reinterpret_cast<void *>(isr44), reinterpret_cast<void *>(isr45),
    reinterpret_cast<void *>(isr46), reinterpret_cast<void *>(isr47)
};

const char *k_exception_names[32] = {
    "Divide Error",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Bound Range",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization",
    "Control Protection",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection",
    "VMM Communication",
    "Security",
    "Reserved"
};

uint16_t read_cs(void) {
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return cs;
}

void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void io_wait(void) {
    outb(0x80, 0);
}

void set_gate(uint8_t vector, void *handler, uint16_t selector, uint8_t type_attr) {
    uint64_t addr = reinterpret_cast<uint64_t>(handler);
    idt_entry &entry = g_idt[vector];
    entry.offset_low = static_cast<uint16_t>(addr & 0xFFFF);
    entry.selector = selector;
    entry.ist = 0;
    entry.type_attr = type_attr;
    entry.offset_mid = static_cast<uint16_t>((addr >> 16) & 0xFFFF);
    entry.offset_high = static_cast<uint32_t>((addr >> 32) & 0xFFFFFFFF);
    entry.zero = 0;
}

void serial_write_hex64(uint64_t value) {
    static const char k_hex[] = "0123456789abcdef";
    char out[19] = "0x0000000000000000";
    for (uint64_t i = 0; i < 16; i++) {
        out[17 - i] = k_hex[value & 0x0f];
        value >>= 4;
    }
    serial_write(out);
}

bool should_trace_worker_irq0(uint64_t seq) {
    if (!kForensicsMode) {
        return false;
    }
    return seq < 16 || (seq % 64) == 0;
}

const char *owner_label(uint64_t idx) {
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
    const uint64_t hi = reinterpret_cast<uint64_t>(&irq_dispatch_stack_top);
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

bool worker_slot_canary_ok(uint64_t *lo, uint64_t *hi) {
    if (g_worker_frame_slot_probe_enabled == 0 ||
        g_worker_frame_slot_probe_canary_lo_addr == 0 ||
        g_worker_frame_slot_probe_canary_hi_addr == 0) {
        return false;
    }

    *lo = *reinterpret_cast<const uint64_t *>(g_worker_frame_slot_probe_canary_lo_addr);
    *hi = *reinterpret_cast<const uint64_t *>(g_worker_frame_slot_probe_canary_hi_addr);
    return (*lo == g_worker_frame_slot_probe_canary_lo_expect) &&
           (*hi == g_worker_frame_slot_probe_canary_hi_expect);
}

[[noreturn]] void panic_exception(interrupt_frame *ctx) {
    uint64_t vec = ctx->vector & 0xff;
    serial_write("\n\n[FATAL] CPU exception: ");
    if (vec < 32) {
        serial_write(k_exception_names[vec]);
    } else {
        serial_write("Unknown");
    }
    serial_write("\n[FATAL] vector=");
    serial_write_hex64(ctx->vector);
    serial_write(" error=");
    serial_write_hex64(ctx->error_code);
    serial_write("\n[FATAL] rip=");
    serial_write_hex64(ctx->rip);
    serial_write(" cs=");
    serial_write_hex64(ctx->cs);
    serial_write(" rflags=");
    serial_write_hex64(ctx->rflags);
    serial_write("\n");

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void pic_send_eoi(uint8_t vector) {
    if (vector >= (kIrqBase + 8)) {
        outb(kSlavePicCmd, kPicEoi);
    }
    outb(kMasterPicCmd, kPicEoi);
}

void pic_set_irq0_masked(bool masked) {
    const uint8_t master_mask = inb(kMasterPicData);
    const uint8_t next_mask = masked
                                  ? static_cast<uint8_t>(master_mask | 0x01U)
                                  : static_cast<uint8_t>(master_mask & ~0x01U);
    outb(kMasterPicData, next_mask);
}

void pic_remap_and_configure(void) {
    const uint8_t master_mask = inb(kMasterPicData);
    (void)inb(kSlavePicData);

    outb(kMasterPicCmd, 0x11);
    io_wait();
    outb(kSlavePicCmd, 0x11);
    io_wait();

    outb(kMasterPicData, kIrqBase);
    io_wait();
    outb(kSlavePicData, kIrqBase + 8);
    io_wait();

    outb(kMasterPicData, 0x04);
    io_wait();
    outb(kSlavePicData, 0x02);
    io_wait();

    outb(kMasterPicData, 0x01);
    io_wait();
    outb(kSlavePicData, 0x01);
    io_wait();

    outb(kMasterPicData, static_cast<uint8_t>((master_mask | 0xFEU) & ~0x01U));
    outb(kSlavePicData, 0xFF);
}

void pit_init_100hz(void) {
    constexpr uint16_t kDivisor100Hz = 11932;
    constexpr uint16_t kDivisorSlowProbe = 59659;  // ~20 Hz (slowest practical while staying above 18.2 Hz floor)
    const uint16_t divisor = kEnablePitSlowTickProbe ? kDivisorSlowProbe : kDivisor100Hz;
    outb(0x43, 0x36);
    outb(0x40, static_cast<uint8_t>(divisor & 0xFF));
    outb(0x40, static_cast<uint8_t>((divisor >> 8) & 0xFF));
}
}

extern "C" interrupt_frame *interrupts_dispatch(interrupt_frame *ctx, uint64_t entry_rsp) {
    const uint8_t vector = static_cast<uint8_t>(ctx->vector & 0xff);
    if (vector < 32) {
        panic_exception(ctx);
    }

    if (vector >= kIrqBase && vector < (kIrqBase + 16)) {
        bool irq0_touched_worker = false;
        if (vector == kIrqBase) {
            const uint64_t before_idx = scheduler_current_thread_index();
            irq0_touched_worker = (before_idx == 1);
            if (irq0_touched_worker) {
                g_worker_irq0_entry_count++;
            }
            diag_mark_phase(kDiagPhaseIrq0Enter, before_idx);
            if (kForensicsMode && before_idx == 1) {
                const uint64_t seq = g_worker_irq0_trace_count++;
                if (should_trace_worker_irq0(seq)) {
                    const uint64_t entry_owner = scheduler_stack_owner_index_for_address(entry_rsp);
                    const uint64_t slot_owner = scheduler_stack_owner_index_for_address(g_worker_frame_slot_probe_frame_addr);
                    const uint64_t slot_rsp_owner = scheduler_stack_owner_index_for_address(g_worker_frame_slot_probe_resume_rsp);
                    serial_write("[TRACE] irq0: before scheduler frame=");
                    serial_write_hex64(reinterpret_cast<uint64_t>(ctx));
                    serial_write(" entry_rsp=");
                    serial_write_hex64(entry_rsp);
                    serial_write(" entry_owner=");
                    serial_write(owner_label(entry_owner));
                    serial_write(" entry_class=");
                    serial_write(stack_class_label(entry_rsp));
                    serial_write(" slot_frame=");
                    serial_write_hex64(g_worker_frame_slot_probe_frame_addr);
                    serial_write(" slot_owner=");
                    serial_write(owner_label(slot_owner));
                    serial_write(" slot_class=");
                    serial_write(stack_class_label(g_worker_frame_slot_probe_frame_addr));
                    serial_write(" slot_intended_owner=");
                    serial_write(owner_label(g_worker_frame_slot_probe_owner_index));
                    serial_write(" slot_resume_rsp=");
                    serial_write_hex64(g_worker_frame_slot_probe_resume_rsp);
                    serial_write(" slot_rsp_owner=");
                    serial_write(owner_label(slot_rsp_owner));
                    serial_write(" slot_rsp_class=");
                    serial_write(stack_class_label(g_worker_frame_slot_probe_resume_rsp));
                    serial_write(" rip=");
                    serial_write_hex64(ctx->rip);
                    serial_write(" rflags=");
                    serial_write_hex64(ctx->rflags);
                    if (g_worker_frame_slot_probe_enabled != 0) {
                        uint64_t canary_lo = 0;
                        uint64_t canary_hi = 0;
                        const bool canary_ok = worker_slot_canary_ok(&canary_lo, &canary_hi);
                        serial_write(" canary_ok=");
                        serial_write(canary_ok ? "1" : "0");
                        serial_write(" canary_lo=");
                        serial_write_hex64(canary_lo);
                        serial_write(" canary_hi=");
                        serial_write_hex64(canary_hi);
                        if (!canary_ok) {
                            g_worker_frame_slot_probe_c_mismatch_count++;
                        }
                    }
                    serial_write("\n");
                }
            }
            g_timer_ticks++;
            ctx = scheduler_on_timer_tick(ctx);
            const uint64_t after_idx = scheduler_current_thread_index();
            irq0_touched_worker = irq0_touched_worker || (after_idx == 1);
            diag_mark_phase(kDiagPhaseIrq0PostScheduler, after_idx);
            if (kForensicsMode && (before_idx == 1 || after_idx == 1)) {
                const uint64_t seq = g_worker_irq0_trace_count++;
                if (should_trace_worker_irq0(seq)) {
                    const uint64_t entry_owner = scheduler_stack_owner_index_for_address(entry_rsp);
                    const uint64_t slot_owner = scheduler_stack_owner_index_for_address(g_worker_frame_slot_probe_frame_addr);
                    const uint64_t slot_rsp_owner = scheduler_stack_owner_index_for_address(g_worker_frame_slot_probe_resume_rsp);
                    serial_write("[TRACE] irq0: before EOI frame=");
                    serial_write_hex64(reinterpret_cast<uint64_t>(ctx));
                    serial_write(" entry_rsp=");
                    serial_write_hex64(entry_rsp);
                    serial_write(" entry_owner=");
                    serial_write(owner_label(entry_owner));
                    serial_write(" entry_class=");
                    serial_write(stack_class_label(entry_rsp));
                    serial_write(" slot_frame=");
                    serial_write_hex64(g_worker_frame_slot_probe_frame_addr);
                    serial_write(" slot_owner=");
                    serial_write(owner_label(slot_owner));
                    serial_write(" slot_class=");
                    serial_write(stack_class_label(g_worker_frame_slot_probe_frame_addr));
                    serial_write(" slot_intended_owner=");
                    serial_write(owner_label(g_worker_frame_slot_probe_owner_index));
                    serial_write(" slot_resume_rsp=");
                    serial_write_hex64(g_worker_frame_slot_probe_resume_rsp);
                    serial_write(" slot_rsp_owner=");
                    serial_write(owner_label(slot_rsp_owner));
                    serial_write(" slot_rsp_class=");
                    serial_write(stack_class_label(g_worker_frame_slot_probe_resume_rsp));
                    serial_write(" cur_idx=");
                    serial_write_hex64(after_idx);
                    serial_write(" rip=");
                    serial_write_hex64(ctx->rip);
                    serial_write(" rflags=");
                    serial_write_hex64(ctx->rflags);
                    serial_write("\n");
                }
            }
        }
        pic_send_eoi(vector);
        if (vector == kIrqBase) {
            const uint64_t after_idx = scheduler_current_thread_index();
            diag_mark_phase(kDiagPhaseIrq0PostEoi, after_idx);
            if (kEnableMaskIrq0AfterFirstWorkerEoiProbe &&
                g_worker_irq0_mask_probe_armed &&
                irq0_touched_worker) {
                g_worker_irq0_mask_probe_armed = false;
                pic_set_irq0_masked(true);
                serial_write("[TRACE] irq0-mask-probe: masked IRQ0 on master PIC after first worker EOI.\n");
            }
            if (kForensicsMode && after_idx == 1) {
                const uint64_t seq = g_worker_irq0_trace_count++;
                if (should_trace_worker_irq0(seq)) {
                    serial_write("[TRACE] irq0: after EOI.\n");
                }
            }
            if (kEnableIrq0ReturnTripleProbe) {
                const uint64_t seq = g_irq0_return_probe_count++;
                if (seq < 24 || (seq % 64) == 0 || after_idx == 1 || irq0_touched_worker) {
                    serial_write("[TRACE] irq0-return-probe idx=");
                    serial_write_hex64(after_idx);
                    serial_write(" frame=");
                    serial_write_hex64(reinterpret_cast<uint64_t>(ctx));
                    serial_write(" rip=");
                    serial_write_hex64(ctx->rip);
                    serial_write(" cs=");
                    serial_write_hex64(ctx->cs);
                    serial_write(" rflags=");
                    serial_write_hex64(ctx->rflags);
                    serial_write("\n");
                }
            }
            if (kEnableIrq0DispatchReturnMarker && irq0_touched_worker) {
                serial_write("D");
            }
            if (kForensicsMode && irq0_touched_worker && g_worker_frame_slot_probe_enabled != 0) {
                uint64_t canary_lo = 0;
                uint64_t canary_hi = 0;
                const bool canary_ok = worker_slot_canary_ok(&canary_lo, &canary_hi);
                const uint64_t slot_owner = scheduler_stack_owner_index_for_address(g_worker_frame_slot_probe_frame_addr);
                const uint64_t slot_rsp_owner = scheduler_stack_owner_index_for_address(g_worker_frame_slot_probe_resume_rsp);
                const uint64_t seq = g_worker_irq0_trace_count++;
                if (should_trace_worker_irq0(seq) || !canary_ok) {
                    serial_write("[TRACE] irq0: pre-iret canary_ok=");
                    serial_write(canary_ok ? "1" : "0");
                    serial_write(" slot_owner=");
                    serial_write(owner_label(slot_owner));
                    serial_write(" slot_class=");
                    serial_write(stack_class_label(g_worker_frame_slot_probe_frame_addr));
                    serial_write(" slot_intended_owner=");
                    serial_write(owner_label(g_worker_frame_slot_probe_owner_index));
                    serial_write(" slot_resume_rsp=");
                    serial_write_hex64(g_worker_frame_slot_probe_resume_rsp);
                    serial_write(" slot_rsp_owner=");
                    serial_write(owner_label(slot_rsp_owner));
                    serial_write(" slot_rsp_class=");
                    serial_write(stack_class_label(g_worker_frame_slot_probe_resume_rsp));
                    serial_write(" canary_lo=");
                    serial_write_hex64(canary_lo);
                    serial_write(" canary_hi=");
                    serial_write_hex64(canary_hi);
                    serial_write(" c_mismatch_count=");
                    serial_write_hex64(g_worker_frame_slot_probe_c_mismatch_count);
                    serial_write(" asm_mismatch_count=");
                    serial_write_hex64(g_worker_frame_slot_probe_asm_mismatch_count);
                    serial_write("\n");
                }
                if (!canary_ok) {
                    g_worker_frame_slot_probe_c_mismatch_count++;
                }
            }
            if (kEnableIrq0HandoffCompareProbe && irq0_touched_worker) {
                g_irq0_handoff_expect_rip = ctx->rip;
                g_irq0_handoff_expect_cs = ctx->cs;
                g_irq0_handoff_expect_rflags = ctx->rflags;
                g_irq0_handoff_probe_armed = 1;
            }
            if (kEnableIrq0HandoffGprCompareProbe && irq0_touched_worker) {
                g_irq0_handoff_expect_r15 = ctx->r15;
                g_irq0_handoff_expect_r14 = ctx->r14;
                g_irq0_handoff_expect_r13 = ctx->r13;
                g_irq0_handoff_expect_r12 = ctx->r12;
                g_irq0_handoff_expect_rbp = ctx->rbp;
                g_irq0_handoff_expect_rbx = ctx->rbx;
                g_irq0_handoff_gpr_probe_armed = 1;
            }
            if (kEnableSecondWorkerIrqEntryStubProbe &&
                g_worker_irq0_entry_count == 1 &&
                irq0_touched_worker) {
                g_worker_second_irq_stub_probe_armed = 1;
            }
        }
        return ctx;
    }

    panic_exception(ctx);
}

bool interrupts_init(void) {
    const uint16_t code_selector = read_cs();
    for (uint8_t i = 0; i < 32; i++) {
        set_gate(i, k_exception_stubs[i], code_selector, 0x8e);
    }
    for (uint8_t i = 0; i < 16; i++) {
        set_gate(static_cast<uint8_t>(kIrqBase + i), k_irq_stubs[i], code_selector, 0x8e);
    }

    idtr table = {};
    table.limit = sizeof(g_idt) - 1;
    table.base = reinterpret_cast<uint64_t>(&g_idt[0]);
    __asm__ volatile ("lidt %0" : : "m"(table));

    pic_remap_and_configure();
    pit_init_100hz();
    __asm__ volatile ("sti");

    if (kEnablePitSlowTickProbe) {
        log_warn("interrupts: IDT+PIC+PIT initialized (slow tick probe ~20Hz).");
    } else {
        log_info("interrupts: IDT+PIC+PIT initialized (100Hz tick).");
    }
    return true;
}

uint64_t timer_ticks(void) {
    return g_timer_ticks;
}

void interrupts_log_delivery_invariants(const char *tag) {
    if (!kForensicsMode) {
        return;
    }

    idtr loaded = {};
    __asm__ volatile ("sidt %0" : "=m"(loaded));

    uint16_t tr_selector = 0;
    __asm__ volatile ("str %0" : "=r"(tr_selector));

    const idt_entry &gate = g_idt[32];
    const uint64_t gate_target =
        static_cast<uint64_t>(gate.offset_low) |
        (static_cast<uint64_t>(gate.offset_mid) << 16) |
        (static_cast<uint64_t>(gate.offset_high) << 32);
    const uint64_t expected_target = reinterpret_cast<uint64_t>(isr32);

    serial_write("[TRACE] irq-delivery-invariants ");
    serial_write(tag != nullptr ? tag : "(null)");
    serial_write(" idtr.base=");
    serial_write_hex64(loaded.base);
    serial_write(" idtr.limit=");
    serial_write_hex64(loaded.limit);
    serial_write(" tr=");
    serial_write_hex64(tr_selector);
    serial_write(" gate32.sel=");
    serial_write_hex64(gate.selector);
    serial_write(" gate32.ist=");
    serial_write_hex64(gate.ist);
    serial_write(" gate32.type=");
    serial_write_hex64(gate.type_attr);
    serial_write(" gate32.present=");
    serial_write((gate.type_attr & 0x80) != 0 ? "1" : "0");
    serial_write(" gate32.target=");
    serial_write_hex64(gate_target);
    serial_write(" isr32=");
    serial_write_hex64(expected_target);
    serial_write(" gate32.target_match=");
    serial_write(gate_target == expected_target ? "1" : "0");
    serial_write("\n");
}
