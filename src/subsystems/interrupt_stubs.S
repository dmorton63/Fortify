.section .text

.extern interrupts_dispatch
.extern g_irq0_handoff_probe_armed
.extern g_irq0_handoff_expect_rip
.extern g_irq0_handoff_expect_cs
.extern g_irq0_handoff_expect_rflags
.extern g_irq0_handoff_gpr_probe_armed
.extern g_irq0_handoff_expect_r15
.extern g_irq0_handoff_expect_r14
.extern g_irq0_handoff_expect_r13
.extern g_irq0_handoff_expect_r12
.extern g_irq0_handoff_expect_rbp
.extern g_irq0_handoff_expect_rbx
.extern g_worker_second_irq_stub_probe_armed
.extern g_worker_frame_slot_probe_enabled
.extern g_worker_frame_slot_probe_canary_lo_addr
.extern g_worker_frame_slot_probe_canary_hi_addr
.extern g_worker_frame_slot_probe_canary_lo_expect
.extern g_worker_frame_slot_probe_canary_hi_expect
.extern g_worker_frame_slot_probe_asm_mismatch_count

.set kEnableIrq0ReturnMarker, 0
.set kEnableIrq0EntryMarker, 0
.set kEnableExceptionEntryMarkers, 0
.set kEnableCommonIrqEntryMarker, 0

.macro PUSH_GPRS
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
.endm

.macro POP_GPRS
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
.endm

isr_common:
.if kEnableCommonIrqEntryMarker
    cmpq $32, 0(%rsp)
    jne 4f
    movw $0x3f8, %dx
    movb $0x63, %al
    outb %al, %dx
4:
.endif
    cld
    PUSH_GPRS
    movq %rsp, %rdi
    movq %rsp, %rsi
    leaq irq_dispatch_stack_top(%rip), %rsp
    call interrupts_dispatch
    movq %rax, %rsp
    cmpq $32, 120(%rsp)
    jne 8f
    movq g_irq0_handoff_gpr_probe_armed(%rip), %rcx
    testq %rcx, %rcx
    je 8f
    movq 0(%rsp), %rcx
    cmpq g_irq0_handoff_expect_r15(%rip), %rcx
    jne 7f
    movq 8(%rsp), %rcx
    cmpq g_irq0_handoff_expect_r14(%rip), %rcx
    jne 7f
    movq 16(%rsp), %rcx
    cmpq g_irq0_handoff_expect_r13(%rip), %rcx
    jne 7f
    movq 24(%rsp), %rcx
    cmpq g_irq0_handoff_expect_r12(%rip), %rcx
    jne 7f
    movq 80(%rsp), %rcx
    cmpq g_irq0_handoff_expect_rbp(%rip), %rcx
    jne 7f
    movq 104(%rsp), %rcx
    cmpq g_irq0_handoff_expect_rbx(%rip), %rcx
    jne 7f
    movq $0, g_irq0_handoff_gpr_probe_armed(%rip)
    jmp 8f
7:
    movw $0x3f8, %dx
    movb $0x47, %al
    outb %al, %dx
    movq $0, g_irq0_handoff_gpr_probe_armed(%rip)
8:
    cmpq $32, 120(%rsp)
    jne 0f
    movq g_irq0_handoff_probe_armed(%rip), %rcx
    testq %rcx, %rcx
    je 0f
    movq 136(%rsp), %rcx
    cmpq g_irq0_handoff_expect_rip(%rip), %rcx
    jne 9f
    movq 144(%rsp), %rcx
    cmpq g_irq0_handoff_expect_cs(%rip), %rcx
    jne 9f
    movq 152(%rsp), %rcx
    cmpq g_irq0_handoff_expect_rflags(%rip), %rcx
    jne 9f
    movq $0, g_irq0_handoff_probe_armed(%rip)
    jmp 0f
9:
    movw $0x3f8, %dx
    movb $0x4d, %al
    outb %al, %dx
    movq $0, g_irq0_handoff_probe_armed(%rip)
0:
    cmpq $32, 120(%rsp)
    jne 6f
    movq g_worker_frame_slot_probe_enabled(%rip), %rcx
    testq %rcx, %rcx
    je 6f
    movq g_worker_frame_slot_probe_canary_lo_addr(%rip), %rcx
    movq (%rcx), %rax
    cmpq g_worker_frame_slot_probe_canary_lo_expect(%rip), %rax
    jne 5f
    movq g_worker_frame_slot_probe_canary_hi_addr(%rip), %rcx
    movq (%rcx), %rax
    cmpq g_worker_frame_slot_probe_canary_hi_expect(%rip), %rax
    jne 5f
    jmp 6f
5:
    addq $1, g_worker_frame_slot_probe_asm_mismatch_count(%rip)
    movw $0x3f8, %dx
    movb $0x4b, %al
    outb %al, %dx
6:
.if kEnableIrq0ReturnMarker
    cmpq $32, 120(%rsp)
    jne 1f
    movw $0x3f8, %dx
    movb $0x7e, %al
    outb %al, %dx
1:
.endif
    POP_GPRS
    addq $16, %rsp
.if kEnableIrq0ReturnMarker
    cmpq $32, -16(%rsp)
    jne 2f
    movw $0x3f8, %dx
    movb $0x5e, %al
    outb %al, %dx
2:
.endif
    iretq

.macro ISR_NOERR num
    .global isr\num
isr\num:
    pushq $0
    pushq $\num
    jmp isr_common
.endm

.macro ISR_ERR num
    .global isr\num
isr\num:
.if kEnableExceptionEntryMarkers
    movw $0x3f8, %dx
.if \num == 8
    movb $0x38, %al
    outb %al, %dx
.elseif \num == 11
    movb $0x4e, %al
    outb %al, %dx
.elseif \num == 12
    movb $0x53, %al
    outb %al, %dx
.elseif \num == 13
    movb $0x47, %al
    outb %al, %dx
.elseif \num == 14
    movb $0x50, %al
    outb %al, %dx
.endif
.endif
    pushq $\num
    jmp isr_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31
.global isr32
isr32:
.if kEnableIrq0EntryMarker
    movw $0x3f8, %dx
    movb $0x74, %al
    outb %al, %dx
.endif
.if kEnableIrq0EntryMarker
    movw $0x3f8, %dx
    movb $0x74, %al
    outb %al, %dx
.endif
    cmpq $0, g_worker_second_irq_stub_probe_armed(%rip)
    je 3f
    movw $0x3f8, %dx
    movb $0x32, %al
    outb %al, %dx
    movq $0, g_worker_second_irq_stub_probe_armed(%rip)
3:
    pushq $0
    pushq $32
    jmp isr_common
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

.section .bss
.balign 16
.global irq_dispatch_stack
irq_dispatch_stack:
    .skip 16384
.global irq_dispatch_stack_top
irq_dispatch_stack_top:

.section .note.GNU-stack,"",@progbits
