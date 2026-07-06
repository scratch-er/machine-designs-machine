#!/usr/bin/env python3
"""Generate combinatorial branch tests for RV32E_Zicsr.

For every branch condition we generate pairs of operands and verify that the
branch is taken exactly when the condition is true.  The test uses a helper
macro: for each case we load the operands, execute the branch, and if the
branch is taken we jump over a fail instruction.
"""

import sys

MASK = 0xFFFFFFFF


def sext32(v):
    v = v & MASK
    if v & 0x80000000:
        v -= 0x100000000
    return v


def u32(v):
    return v & MASK


# Operand grid covering signed/unsigned extremes and simple values.
OPS = [
    0,
    1,
    0xFFFFFFFF,  # -1
    0x7FFFFFFF,
    0x80000000,
    42,
    0xFFFFFFD6,  # -42
    0x55555555,
    0xAAAAAAAA,
]


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


def gen_branches():
    print("    # ----- branch tests -----")
    case = 0
    for a in OPS:
        for b in OPS:
            case += 1
            sa, sb = sext32(a), sext32(b)
            ua, ub = u32(a), u32(b)
            conditions = [
                ("beq", ua == ub),
                ("bne", ua != ub),
                ("blt", sa < sb),
                ("bge", sa >= sb),
                ("bltu", ua < ub),
                ("bgeu", ua >= ub),
            ]
            for mnem, expected in conditions:
                label_taken = f"br_{case}_{mnem}_taken"
                label_end = f"br_{case}_{mnem}_end"
                print(f"    li x4, {u32(a)}")
                print(f"    li x5, {u32(b)}")
                print(f"    {mnem} x4, x5, {label_taken}")
                if expected:
                    # Branch should have been taken; falling through is a failure.
                    print("    j fail")
                else:
                    # Branch should not be taken; falling through is correct.
                    print(f"    j {label_end}")
                print(f"{label_taken}:")
                if not expected:
                    # Branch was taken but should not have been.
                    print("    j fail")
                print(f"{label_end}:")
            print()


def main():
    emit_header()
    gen_branches()
    emit_footer()


if __name__ == "__main__":
    main()
