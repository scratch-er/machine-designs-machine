.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # addi basic cases
    addi x4, x0, 0
    CHECK_EQ x4, 0
    addi x4, x0, 42
    CHECK_EQ x4, 42
    addi x4, x0, -1
    CHECK_EQ x4, 0xFFFFFFFF
    addi x4, x4, 1
    CHECK_EQ x4, 0

    # addi with non-zero rs1
    li x5, 100
    addi x6, x5, -30
    CHECK_EQ x6, 70

    # add overflow wraps
    li x5, 0xFFFFFFFF
    addi x6, x5, 1
    CHECK_EQ x6, 0

    # add
    li x5, 0x70000000
    li x6, 0x10000000
    add x7, x5, x6
    CHECK_EQ x7, 0x80000000

    # sub
    li x5, 5
    li x6, 10
    sub x7, x5, x6
    CHECK_EQ x7, 0xFFFFFFFB

    # xori (immediate is sign-extended 12-bit; use 0x7FF to keep upper bits 0)
    li x5, 0x0F0F0F0F
    xori x6, x5, 0x7FF
    CHECK_EQ x6, 0x0F0F08F0

    # ori
    li x5, 0x00FF00FF
    ori x6, x5, 0x7FF
    CHECK_EQ x6, 0x00FF07FF

    # andi
    li x5, 0xFFFFFFFF
    andi x6, x5, 0x123
    CHECK_EQ x6, 0x123

    # xor
    li x5, 0xAAAAAAAA
    li x6, 0x55555555
    xor x7, x5, x6
    CHECK_EQ x7, 0xFFFFFFFF

    # or
    or x7, x5, x6
    CHECK_EQ x7, 0xFFFFFFFF

    # and
    and x7, x5, x6
    CHECK_EQ x7, 0

    # x0 destination must be ignored
    addi x0, x0, 123
    CHECK_EQ x0, 0

    # x0 source is zero
    add x7, x0, x0
    CHECK_EQ x7, 0

    PASS

fail:
    FAIL
