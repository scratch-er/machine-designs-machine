.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # Install a handler that records the trap cause and resumes.
    la x4, trap_handler
    csrrw x0, mtvec, x4

    # 1) Unknown major opcode (0x00 is not implemented).
    li x7, 0
    .word 0x00000000
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    # 2) Bad funct3 for OP-IMM (addi with funct3 = 111 is andi, but opcode still 0x13).
    # Use funct3 = 111 with a pattern that is not andi? Actually andi is legal.
    # Use funct3 = 111 with opcode 0x13 and a non-immediate-looking encoding? No,
    # andi covers all of funct3=111 for opcode 0x13.  Instead use funct3=010
    # for OP-IMM which is slti (legal).  We need an unimplemented funct3.
    # For OP-IMM, funct3 values 011, 101, 110 are legal (sltiu, srli/srai, ori).
    # funct3 = 111 is andi.  So all OP-IMM funct3 are legal except shifts need
    # funct7.  Use a bad funct7 for slli: funct3=001, funct7=0100000.
    li x7, 0
    .word 0x20011093        # xori? no, funct3=001, funct7=0100000, rd=x1, rs1=x1 -> illegal slli encoding
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    # 3) Bad funct3 for LOAD (funct3 = 011 or 111 are not implemented).
    li x7, 0
    .word 0x0001B183        # funct3=011, rd=x3, rs1=x3, imm=0 -> illegal
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    # 4) Bad funct3 for STORE (funct3 = 011 or 111 are not implemented).
    li x7, 0
    .word 0x0001B023        # funct3=011, rs1=x3, rs2=x0, imm=0 -> illegal
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    # 5) SYSTEM with funct3=000 but unknown funct12.
    li x7, 0
    .word 0x00200073        # funct12=002, funct3=000 -> not ecall/ebreak/mret/wfi
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    # 6) CSR with funct3=000 is also illegal (treated as SYSTEM funct3=000).
    li x7, 0
    .word 0x30000073        # funct12=0x300, funct3=000, rd/rs1=0 -> illegal
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    # 7) CSR access to unimplemented CSR address.
    li x7, 0
    csrrs x5, 0x001, x0
    CHECK_EQ x7, 1
    csrrs x5, mcause, x0
    CHECK_EQ x5, CAUSE_ILLEGAL_INST

    PASS

trap_handler:
    li x7, 1
    csrrs x15, mepc, x0
    addi x15, x15, 4
    csrrw x0, mepc, x15
    mret

fail:
    FAIL
