.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # Read-only CSRs
    csrrc x4, mvendorid, x5
    CHECK_EQ x4, 0
    csrrs x4, marchid, x5
    CHECK_EQ x4, 0

    # mstatus: MPP hardcoded to M-mode (0x1800), writes ignored
    csrrw x4, mstatus, x0
    CHECK_EQ x4, 0x00001800
    li x5, 0xFFFFFFFF
    csrrw x4, mstatus, x5
    CHECK_EQ x4, 0x00001800
    csrrc x4, mstatus, x5
    CHECK_EQ x4, 0x00001800

    # mtvec: low two bits masked on write
    li x5, 0x20001003
    csrrw x0, mtvec, x5
    csrrs x4, mtvec, x0
    CHECK_EQ x4, 0x20001000

    # mepc: low two bits masked on write
    li x5, 0x2000000F
    csrrw x0, mepc, x5
    csrrs x4, mepc, x0
    CHECK_EQ x4, 0x2000000C

    # mcause: writable
    li x5, 0x12345678
    csrrw x4, mcause, x5
    CHECK_EQ x4, 0
    csrrs x4, mcause, x0
    CHECK_EQ x4, 0x12345678

    # csrrw with rd=x0 suppresses read but still writes
    li x5, 0x11111111
    csrrw x0, mtvec, x5
    csrrs x4, mtvec, x0
    CHECK_EQ x4, 0x11111110

    # csrrs / csrrc with rs1=x0 do not write
    li x5, 0xFFFFFFFF
    csrrw x0, mepc, x5
    csrrs x4, mepc, x0
    CHECK_EQ x4, 0xFFFFFFFC
    csrrs x0, mepc, x0   # rs1=x0, no write
    csrrs x4, mepc, x0
    CHECK_EQ x4, 0xFFFFFFFC
    csrrc x0, mepc, x0   # rs1=x0, no write
    csrrs x4, mepc, x0
    CHECK_EQ x4, 0xFFFFFFFC

    # csrrwi / csrrsi / csrrci with immediate
    csrrwi x4, mcause, 0x1F
    CHECK_EQ x4, 0x12345678     # old mcause value read into rd
    csrrs x4, mcause, x0
    CHECK_EQ x4, 0x1F           # mcause was written with uimm
    csrrsi x0, mcause, 0x5
    csrrs x4, mcause, x0
    CHECK_EQ x4, (0x1F | 5)
    csrrci x0, mcause, 0x4
    csrrs x4, mcause, x0
    CHECK_EQ x4, ((0x1F | 5) & ~4)

    # csrrsi / csrrci with uimm=0 do not write
    li x5, 0xAAAAAAAA
    csrrw x0, mcause, x5
    csrrsi x0, mcause, 0
    csrrs x4, mcause, x0
    CHECK_EQ x4, 0xAAAAAAAA
    csrrci x0, mcause, 0
    csrrs x4, mcause, x0
    CHECK_EQ x4, 0xAAAAAAAA

    PASS

fail:
    FAIL
