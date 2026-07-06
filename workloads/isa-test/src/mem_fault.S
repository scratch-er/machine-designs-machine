.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # Install a handler that records the trap cause and resumes after the
    # trapping instruction.
    la x4, trap_handler
    csrrw x0, mtvec, x4

    # Test address: well above RAM so strict_mem will fault.
    li x10, 0x30000000

    # 1) Unmapped load must raise load access fault (cause = 5).
    li x7, 0
    lw x5, 0(x10)
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_LOAD_ACCESS_FAULT

    # 2) Unmapped store must raise store access fault (cause = 7).
    li x7, 0
    sw x5, 0(x10)
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_STORE_ACCESS_FAULT

    # 3) A RAM access still works under strict_mem.
    li x10, 0x20010000
    li x6, 0x12345678
    sw x6, 0(x10)
    lw x5, 0(x10)
    CHECK_EQ x5, 0x12345678

    PASS

trap_handler:
    li x7, 1
    csrrs x15, mepc, x0
    addi x15, x15, 4
    csrrw x0, mepc, x15
    mret

fail:
    FAIL
