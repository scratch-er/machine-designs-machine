.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART
    la x1, handler
    csrrw x0, mtvec, x1

    # Jump to a misaligned address. The fetch must raise instruction-address
    # misaligned (priority 1), not illegal instruction.
    la x1, misaligned_addr
    jalr x0, 2(x1)
    j fail

    .align 2
misaligned_addr:
    .word 0xFFFFFFFF          # would be illegal if fetched, but fetch fails first
    .align 2
after_jump:
    PASS

handler:
    csrr x4, mcause
    li x5, CAUSE_INST_MISALIGNED
    bne x4, x5, fail
    la x4, after_jump
    csrrw x0, mepc, x4
    mret

fail:
    FAIL
