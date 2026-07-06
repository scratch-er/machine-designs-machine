# Common test macros for RV32E_Zicsr ISA tests.
# Include this file at the top of each standalone test source.

.set UART_BASE, 0x10000000

# Exception cause codes (matching emulator/common.h)
.set CAUSE_INST_MISALIGNED,   0
.set CAUSE_INST_ACCESS_FAULT, 1
.set CAUSE_ILLEGAL_INST,      2
.set CAUSE_BREAKPOINT,        3
.set CAUSE_LOAD_MISALIGNED,   4
.set CAUSE_LOAD_ACCESS_FAULT, 5
.set CAUSE_STORE_MISALIGNED,  6
.set CAUSE_STORE_ACCESS_FAULT,7
.set CAUSE_ECALL_M,           11

# Set x3 to the UART base address.  Call once at program start.
.macro SETUP_UART
    lui x3, (UART_BASE >> 12)
.endm

# Print "PASS\n" and stop the test with ebreak.
.macro PASS
    li x1, 'P'
    sb x1, 0(x3)
    li x1, 'A'
    sb x1, 0(x3)
    li x1, 'S'
    sb x1, 0(x3)
    sb x1, 0(x3)
    li x1, '\n'
    sb x1, 0(x3)
    ebreak
.endm

# Print "FAIL\n" and stop the test with ebreak.
.macro FAIL
    li x1, 'F'
    sb x1, 0(x3)
    li x1, 'A'
    sb x1, 0(x3)
    li x1, 'I'
    sb x1, 0(x3)
    li x1, 'L'
    sb x1, 0(x3)
    li x1, '\n'
    sb x1, 0(x3)
    ebreak
.endm

# CHECK_EQ reg, expected
#   Clobbers x14.  Branch to the file-local 'fail:' label on mismatch.
.macro CHECK_EQ reg, expected
    li x14, \expected
    bne \reg, x14, fail
.endm

# CHECK_NE reg, unexpected
#   Clobbers x14.  Branch to the file-local 'fail:' label on match.
.macro CHECK_NE reg, unexpected
    li x14, \unexpected
    beq \reg, x14, fail
.endm

# CHECK_MEM addr_reg, expected
#   Load a word from addr_reg and compare with expected.  Clobbers x14, x15.
.macro CHECK_MEM addr_reg, expected
    lw x15, 0(\addr_reg)
    li x14, \expected
    bne x15, x14, fail
.endm

# SET_REG reg, value
#   Convenience wrapper around li.
.macro SET_REG reg, value
    li \reg, \value
.endm

# HALT
#   Stop with ebreak without printing anything.
.macro HALT
    ebreak
.endm
