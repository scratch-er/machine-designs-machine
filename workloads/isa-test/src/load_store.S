.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # RAM base in x10 (use a high address well past the program text).
    li x10, 0x20010000

    # sb / lbu
    li x4, 0xA5
    sb x4, 0(x10)
    lbu x5, 0(x10)
    CHECK_EQ x5, 0xA5
    # clear whole word and re-check sign extension
    sw x0, 0(x10)
    sb x4, 0(x10)
    lb x5, 0(x10)
    CHECK_EQ x5, 0xFFFFFFA5

    # sh / lhu little-endian
    li x4, 0x1234
    sh x4, 4(x10)
    lhu x5, 4(x10)
    CHECK_EQ x5, 0x1234
    sw x0, 4(x10)
    sh x4, 4(x10)
    lh x5, 4(x10)
    CHECK_EQ x5, 0x00001234

    # sw / lw little-endian
    li x4, 0x89ABCDEF
    sw x4, 8(x10)
    lw x5, 8(x10)
    CHECK_EQ x5, 0x89ABCDEF
    # verify byte order
    lbu x5, 8(x10)
    CHECK_EQ x5, 0xEF
    lbu x5, 9(x10)
    CHECK_EQ x5, 0xCD
    lbu x5, 10(x10)
    CHECK_EQ x5, 0xAB
    lbu x5, 11(x10)
    CHECK_EQ x5, 0x89

    # stores do not affect neighbouring bytes
    sw x0, 12(x10)
    li x4, 0x42
    sb x4, 13(x10)
    lw x5, 12(x10)
    CHECK_EQ x5, 0x00004200

    # unaligned access: these should trap and set pc=mtvec=0, so we cannot
    # recover in this simple test.  The exception.S workload covers them.
    # Here we just verify aligned word/half/byte ops work.

    PASS

fail:
    FAIL
