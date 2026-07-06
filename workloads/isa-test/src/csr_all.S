.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # csrrwi: write 8 to mtvec, read old value (0) into x4.
    csrrwi x4, mtvec, 8
    CHECK_EQ x4, 0

    # csrrsi: set bit 4 (0x10), read old value (8) into x5.
    csrrsi x5, mtvec, 0x10
    CHECK_EQ x5, 8

    # csrrci: clear bit 3 (0x8), read old value (0x18) into x6.
    csrrci x6, mtvec, 0x8
    CHECK_EQ x6, 0x18

    # csrrw: write x6 (0x18) to mtvec, read old value (0x10) into x7.
    csrrw x7, mtvec, x6
    CHECK_EQ x7, 0x10

    # csrr: read mtvec into x8.
    csrr x8, mtvec
    CHECK_EQ x8, 0x18

    PASS

fail:
    FAIL
