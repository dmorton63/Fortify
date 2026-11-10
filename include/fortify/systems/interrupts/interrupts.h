#ifndef FORTIFY_INTERRUPTS_H
#define FORTIFY_INTERRUPTS_H

#include <stddef.h>
#include <stdint.h>

struct __attribute__((packed)) interrupt_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

struct __attribute__((packed)) interrupt_stack_image {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

static_assert(sizeof(interrupt_frame) == (20 * sizeof(uint64_t)), "interrupt_frame size mismatch");
static_assert(offsetof(interrupt_frame, vector) == (15 * sizeof(uint64_t)), "interrupt_frame vector offset mismatch");
static_assert(offsetof(interrupt_frame, rip) == (17 * sizeof(uint64_t)), "interrupt_frame rip offset mismatch");
static_assert(sizeof(interrupt_stack_image) == sizeof(interrupt_frame), "interrupt_stack_image size mismatch");
static_assert(offsetof(interrupt_stack_image, vector) == (15 * sizeof(uint64_t)), "interrupt_stack_image vector offset mismatch");
static_assert(offsetof(interrupt_stack_image, rip) == (17 * sizeof(uint64_t)), "interrupt_stack_image rip offset mismatch");

void interrupts_log_delivery_invariants(const char *tag);

#endif
