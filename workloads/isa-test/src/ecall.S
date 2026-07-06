.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART
    la x1, handler
    csrrw x0, mtvec, x1
    li x6, 0

ecall_here:
    ecall
    # Resume here after handler returns.
    CHECK_EQ x6, 1
    PASS

handler:
    csrr x4, mcause
    li x5, CAUSE_ECALL_M
    bne x4, x5, fail
    csrr x4, mepc
    la x5, ecall_here
    bne x4, x5, fail
    addi x6, x6, 1
    # Advance mepc past the ecall instruction.
    csrr x4, mepc
    addi x4, x4, 4
    csrrw x0, mepc, x4
    mret

fail:
    FAIL
