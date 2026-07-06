.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART
    la x1, handler
    csrrw x0, mtvec, x1
    li x1, 1
    li x2, 1
    # Raw beq x1, x2, +10: target is PC+10 (misaligned).
    .word 0x00208563
    j fail

    .byte 0
    .byte 0
    .align 2
after_branch:
    PASS

handler:
    csrr x4, mcause
    li x5, CAUSE_INST_MISALIGNED
    bne x4, x5, fail
    la x4, after_branch
    csrrw x0, mepc, x4
    mret

fail:
    FAIL
