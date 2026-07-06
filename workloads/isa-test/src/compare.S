.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # slti signed comparisons
    li x4, -5
    slti x5, x4, -1
    CHECK_EQ x5, 1
    slti x5, x4, -10
    CHECK_EQ x5, 0
    slti x5, x4, 100
    CHECK_EQ x5, 1

    # slti immediate is sign-extended, then compared as unsigned for sltiu
    li x4, 5
    sltiu x5, x4, -1     # -1 sign-extended = 0xFFFFFFFF unsigned
    CHECK_EQ x5, 1
    li x4, 0xFFFFFFFF
    sltiu x5, x4, -1     # equal
    CHECK_EQ x5, 0
    li x4, 0
    sltiu x5, x4, 1
    CHECK_EQ x5, 1

    # slt signed
    li x4, -1
    li x6, 1
    slt x5, x4, x6
    CHECK_EQ x5, 1
    slt x5, x6, x4
    CHECK_EQ x5, 0
    slt x5, x4, x4
    CHECK_EQ x5, 0

    # sltu unsigned
    li x4, 0xFFFFFFFF
    li x6, 1
    sltu x5, x4, x6
    CHECK_EQ x5, 0
    sltu x5, x6, x4
    CHECK_EQ x5, 1
    li x7, 0x80000000
    li x8, 0x7FFFFFFF
    sltu x5, x7, x8
    CHECK_EQ x5, 0

    # INT_MIN vs INT_MAX signed
    li x4, 0x80000000
    li x6, 0x7FFFFFFF
    slt x5, x4, x6
    CHECK_EQ x5, 1
    sltu x5, x4, x6
    CHECK_EQ x5, 0

    PASS

fail:
    FAIL
