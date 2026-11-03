#ifndef FORTIFY_DIAG_H
#define FORTIFY_DIAG_H

#include <stdint.h>

enum diag_phase : uint64_t {
    kDiagPhaseBootEnter = 0x1001,
    kDiagPhaseBootProtocolReady = 0x1002,
    kDiagPhaseCppRuntimeReady = 0x1003,
    kDiagPhaseConsoleReady = 0x1004,
    kDiagPhaseSubsystemInitBegin = 0x1005,
    kDiagPhaseSubsystemInitDone = 0x1006,
    kDiagPhaseSchedulerStart = 0x1007,
    kDiagPhaseIdleLoop = 0x1008,
    kDiagPhaseIrq0Enter = 0x2001,
    kDiagPhaseIrq0PostScheduler = 0x2002,
    kDiagPhaseIrq0PostEoi = 0x2003,
    kDiagPhaseWorkerCommitSelect = 0x3001,
    kDiagPhaseWorkerIrqFrame = 0x3002
};

void diag_boot_enter(void);
void diag_mark_phase(uint64_t phase, uint64_t aux);

#endif
