.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # slli by 0 and 31
    li x4, 0x00000001
    slli x5, x4, 0
    CHECK_EQ x5, 0x00000001
    slli x5, x4, 31
    CHECK_EQ x5, 0x80000000

    # srli by 0 and 31
    li x4, 0x80000000
    srli x5, x4, 0
    CHECK_EQ x5, 0x80000000
    srli x5, x4, 31
    CHECK_EQ x5, 1

    # srai: sign extension from MSB
    li x4, 0x80000000
    srai x5, x4, 31
    CHECK_EQ x5, 0xFFFFFFFF
    li x4, 0x40000000
    srai x5, x4, 30
    CHECK_EQ x5, 1

    # shift amount uses only low 5 bits (RV32E/RV32).
    # Immediate shifts are rejected by the assembler for shamt > 31, so the
    # masking behaviour is exercised with register-register shifts below.

    # register-register shifts also mask rs2[4:0]
    li x4, 1
    li x6, 32
    sll x5, x4, x6
    CHECK_EQ x5, 1
    li x6, 33
    sll x5, x4, x6
    CHECK_EQ x5, 2

    li x4, 0xFFFFFFFF
    li x6, 32
    srl x5, x4, x6
    CHECK_EQ x5, 0xFFFFFFFF
    li x6, 33
    srl x5, x4, x6
    CHECK_EQ x5, 0x7FFFFFFF

    li x4, 0x80000000
    li x6, 32
    sra x5, x4, x6
    CHECK_EQ x5, 0x80000000
    li x6, 33
    sra x5, x4, x6
    CHECK_EQ x5, 0xC0000000

    PASS

fail:
    FAIL
