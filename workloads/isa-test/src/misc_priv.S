.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # fence, fence.i, and wfi are architectural nops in this core.
    fence
    fence.i
    wfi

    # mret returns to mepc.
    la x4, after_mret
    csrrw x0, mepc, x4
    mret
    j fail                  # should be skipped by mret
after_mret:

    PASS

fail:
    FAIL
