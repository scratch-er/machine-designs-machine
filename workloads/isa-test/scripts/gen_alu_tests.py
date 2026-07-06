#!/usr/bin/env python3
"""Generate combinatorial ALU tests for RV32E_Zicsr.

The output is a standalone assembly file that can be built and run by the
emulator.  Each OP-IMM and OP instruction is exercised against a grid of
operands; results are checked inline and the workload prints PASS/FAIL.
"""

import sys

MASK = 0xFFFFFFFF
GPRS = list(range(16))  # x0-x15 for RV32E


def sext32(v):
    """Sign-extend a 32-bit value to a signed Python int."""
    v = v & MASK
    if v & 0x80000000:
        v -= 0x100000000
    return v


def sext12(v):
    """Sign-extend a 12-bit value to a signed Python int."""
    v = v & 0xFFF
    if v & 0x800:
        v -= 0x1000
    return v


def u32(v):
    return v & MASK


# Operands used for register-register operations.
REG_OPS = [
    0,
    1,
    0xFFFFFFFF,  # -1 unsigned
    0x7FFFFFFF,
    0x80000000,
    0x55555555,
    0xAAAAAAAA,
    42,
    0xFFFFFFD6,  # -42
]

# 12-bit signed immediates for addi/xori/ori/andi/slti/sltiu.
IMM12_OPS = [
    0,
    1,
    -1,
    42,
    -42,
    2047,
    -2048,
]

# Shift amounts for immediate shifts (assembler accepts only 0..31).
SHIFT_AMTS = [0, 1, 5, 16, 31]


def emit_header():
    print(".section .text")
    print(".globl _start")
    print('.include "workloads/isa-test/common/test_macros.S"')
    print()
    print("_start:")
    print("    SETUP_UART")
    print()


def emit_footer():
    print("    PASS")
    print()
    print("fail:")
    print("    FAIL")


def check(label, rd, expected):
    print(f"    li x14, {expected}")
    print(f"    bne {rd}, x14, fail")


def gen_op_imm():
    print("    # ----- OP-IMM tests -----")
    for a in REG_OPS:
        for imm in IMM12_OPS:
            imm_se = sext12(imm)  # sign-extended 12-bit immediate

            print(f"    li x4, {u32(a)}")
            print(f"    addi x6, x4, {imm}")
            check("addi", "x6", u32(sext32(a) + sext12(imm)))

            print(f"    li x4, {u32(a)}")
            print(f"    xori x6, x4, {imm}")
            check("xori", "x6", u32(a ^ sext12(imm)))

            print(f"    li x4, {u32(a)}")
            print(f"    ori x6, x4, {imm}")
            check("ori", "x6", u32(a | sext12(imm)))

            print(f"    li x4, {u32(a)}")
            print(f"    andi x6, x4, {imm}")
            check("andi", "x6", u32(a & sext12(imm)))

            print(f"    li x4, {u32(a)}")
            print(f"    slti x6, x4, {imm}")
            check("slti", "x6", 1 if sext32(a) < sext12(imm) else 0)

            print(f"    li x4, {u32(a)}")
            print(f"    sltiu x6, x4, {imm}")
            check("sltiu", "x6", 1 if u32(a) < u32(sext12(imm)) else 0)


def gen_shifts_imm():
    print("    # ----- shift-immediate tests -----")
    for a in REG_OPS:
        for shamt in SHIFT_AMTS:
            sa = shamt & 0x1F
            print(f"    li x4, {u32(a)}")
            print(f"    slli x6, x4, {shamt}")
            check("slli", "x6", u32(a << sa))

            print(f"    li x4, {u32(a)}")
            print(f"    srli x6, x4, {shamt}")
            check("srli", "x6", u32(a) >> sa)

            print(f"    li x4, {u32(a)}")
            print(f"    srai x6, x4, {shamt}")
            check("srai", "x6", u32(sext32(a) >> sa))


def gen_op():
    print("    # ----- OP tests -----")
    for a in REG_OPS:
        for b in REG_OPS:
            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    add x6, x4, x5")
            check("add", "x6", u32(sext32(a) + sext32(b)))

            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    sub x6, x4, x5")
            check("sub", "x6", u32(sext32(a) - sext32(b)))

            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    xor x6, x4, x5")
            check("xor", "x6", u32(a ^ b))

            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    or x6, x4, x5")
            check("or", "x6", u32(a | b))

            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    and x6, x4, x5")
            check("and", "x6", u32(a & b))

            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    slt x6, x4, x5")
            check("slt", "x6", 1 if sext32(a) < sext32(b) else 0)

            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    sltu x6, x4, x5")
            check("sltu", "x6", 1 if u32(a) < u32(b) else 0)


def gen_shifts_reg():
    print("    # ----- shift-register tests -----")
    for a in REG_OPS:
        for b in REG_OPS:
            sa = u32(b) & 0x1F
            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    sll x6, x4, x5")
            check("sll", "x6", u32(a << sa))

            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    srl x6, x4, x5")
            check("srl", "x6", u32(a) >> sa)

            print(f"    li x4, {u32(a)}")
            print(f"    li x5, {u32(b)}")
            print("    sra x6, x4, x5")
            check("sra", "x6", u32(sext32(a) >> sa))


def main():
    emit_header()
    gen_op_imm()
    gen_shifts_imm()
    gen_op()
    gen_shifts_reg()
    emit_footer()


if __name__ == "__main__":
    main()
