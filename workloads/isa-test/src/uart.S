.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # The harness pre-loads UART input with the bytes 0x48, 0x69, 0x0A
    # ("Hi\n").  Read them back one byte at a time and check.
    lbu x4, 0(x3)
    CHECK_EQ x4, 'H'
    lbu x4, 0(x3)
    CHECK_EQ x4, 'i'
    lbu x4, 0(x3)
    CHECK_EQ x4, '\n'

    # Reading past the input sequence returns 0xFF.
    lbu x4, 0(x3)
    CHECK_EQ x4, 0xFF

    PASS

fail:
    FAIL
