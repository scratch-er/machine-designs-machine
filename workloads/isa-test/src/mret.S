.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART
    la x1, handler
    csrrw x0, mtvec, x1

ecall_here:
    ecall
    # Resume here after the handler returns via mret.
    PASS

handler:
    csrr x4, mcause
    li x5, CAUSE_ECALL_M
    bne x4, x5, fail
    csrr x4, mepc
    la x5, ecall_here
    bne x4, x5, fail
    # Advance mepc past the ecall and return.
    csrr x4, mepc
    addi x4, x4, 4
    csrrw x0, mepc, x4
    mret

fail:
    FAIL
