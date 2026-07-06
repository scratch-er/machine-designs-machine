.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # Install a simple handler that records the trap and resumes at the
    # instruction after the one that caused it.
    la x4, trap_handler
    csrrw x0, mtvec, x4

    # 1) ecall must raise environment call from M-mode (mcause = 11).
    li x7, 0
    ecall
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ECALL_M

    # 2) Illegal instruction: all ones is not a known encoding.
    li x7, 0
    .word 0xFFFFFFFF
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    # 3) Reserved register (x16) in an otherwise legal add encoding.
    # add x16, x0, x0  = 0x00000833 | (16 << 7) = 0x00080833
    li x7, 0
    .word 0x00080833
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    # 4) Misaligned jump target via jalr to an odd address.
    li x7, 0
    li x6, 0x20000033
    jalr x0, 0(x6)
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_INST_MISALIGNED

    # Use a high RAM address for misaligned access tests.
    li x10, 0x20010000

    # 5) Misaligned load must raise load address misaligned.
    li x7, 0
    lh x5, 1(x10)
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_LOAD_MISALIGNED

    # 6) Misaligned store must raise store address misaligned.
    li x7, 0
    sh x5, 1(x10)
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_STORE_MISALIGNED

    # 7) Illegal CSR access (address 0x001 is not implemented).
    li x7, 0
    csrrs x5, 0x001, x0
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    PASS

trap_handler:
    # Record that a trap occurred.
    li x7, 1
    # Advance mepc by 4 to resume after the trapping instruction.
    csrrs x15, mepc, x0
    addi x15, x15, 4
    csrrw x0, mepc, x15
    mret

fail:
    FAIL
