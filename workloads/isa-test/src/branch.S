.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # beq taken
    li x4, 7
    li x5, 7
    beq x4, x5, beq_taken
    j fail
beq_taken:

    # beq not taken
    li x4, 7
    li x5, 8
    beq x4, x5, fail

    # bne taken
    bne x4, x5, bne_taken
    j fail
bne_taken:

    # bne not taken
    li x4, 9
    li x5, 9
    bne x4, x5, fail

    # blt signed: -1 < 1
    li x4, -1
    li x5, 1
    blt x4, x5, blt_taken
    j fail
blt_taken:

    # blt not taken: 1 < -1 is false
    blt x5, x4, fail

    # bge signed: 1 >= -1 and 1 >= 1
    bge x5, x4, bge_taken1
    j fail
bge_taken1:
    li x4, 1
    bge x5, x4, bge_taken2
    j fail
bge_taken2:

    # bltu unsigned: 1 < 0xFFFFFFFF
    li x4, 1
    li x5, 0xFFFFFFFF
    bltu x4, x5, bltu_taken
    j fail
bltu_taken:

    # bltu not taken
    bltu x5, x4, fail

    # bgeu unsigned: 0xFFFFFFFF >= 1 and equal
    bgeu x5, x4, bgeu_taken1
    j fail
bgeu_taken1:
    li x4, 0xFFFFFFFF
    bgeu x5, x4, bgeu_taken2
    j fail
bgeu_taken2:

    # backward branch
    li x6, 0
backward_loop:
    addi x6, x6, 1
    li x7, 3
    blt x6, x7, backward_loop
    CHECK_EQ x6, 3

    PASS

fail:
    FAIL
