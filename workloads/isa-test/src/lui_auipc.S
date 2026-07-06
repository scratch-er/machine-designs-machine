.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # lui
    lui x4, 0x12345
    CHECK_EQ x4, 0x12345000
    lui x4, 0xFFFFF
    CHECK_EQ x4, 0xFFFFF000
    lui x4, 0
    CHECK_EQ x4, 0

    # auipc: result = pc + (imm << 12)
auipc_1:
    auipc x4, 0x00001
    la x15, auipc_1
    li x14, 0x1000
    add x15, x15, x14
    bne x4, x15, fail

auipc_2:
    auipc x4, 0x12345
    la x15, auipc_2
    lui x14, 0x12345
    add x15, x15, x14
    bne x4, x15, fail

    # x0 destination
    auipc x0, 0x1
    CHECK_EQ x0, 0

    PASS

fail:
    FAIL
