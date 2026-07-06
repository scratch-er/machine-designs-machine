.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # CLINT mtime / mtimeh addresses.
    li x11, 0x0200BFF8     # mtime
    li x12, 0x0200BFFC     # mtimeh

    # Read mtime twice; it should be non-decreasing.
    lw x4, 0(x11)
    lw x5, 0(x11)
    bgeu x5, x4, clint_ok1
    j fail
clint_ok1:

    # Write a known value to mtimeh and read it back (high word does not
    # auto-increment unless the low word overflows, which is not the case here).
    li x6, 0x5A5A5A5A
    sw x6, 0(x12)
    lw x4, 0(x12)
    CHECK_EQ x4, 0x5A5A5A5A

    # Write a known value to mtime.  Because mtime increments every cycle, the
    # read-back may be the written value or written value + 1.
    li x6, 0xA5A5A5A5
    sw x6, 0(x11)
    lw x4, 0(x11)
    li x14, 0xA5A5A5A5
    beq x4, x14, clint_ok2
    addi x14, x14, 1
    beq x4, x14, clint_ok2
    j fail
clint_ok2:

    PASS

fail:
    FAIL
