# ISA Tests for the Emulator

This directory contains self-checking RISC-V RV32E_Zicsr assembly workloads that
run on the emulator and report **PASS** or **FAIL** over the virtual UART.
They are the foundation for verifying the emulator before it is used as a
differential reference for the processor core.

## Directory Layout

```
workloads/isa-test/
├── README.md              # this file
├── run_tests.sh           # build and run every test, checking UART output
├── common/
│   └── test_macros.S      # UART setup, PASS/FAIL, CHECK_* macros
├── src/
│   ├── alu.S              # integer OP-IMM / OP instructions
│   ├── shift.S            # shift edge cases (by 0, 1, 31, >31)
│   ├── compare.S          # slt/sltu/slti/sltiu boundary values
│   ├── lui_auipc.S        # upper-immediate instructions
│   ├── branch.S           # conditional branches
│   ├── jal_jalr.S         # jumps and link values
│   ├── load_store.S       # loads/stores, endianness, sign/zero extension
│   ├── csr.S              # CSR read/write and access rules
│   ├── exception.S        # exceptions and trap handling
│   ├── misc_priv.S        # fence, fence.i, wfi, mret
│   ├── clint.S            # CLINT mtime/mtimeh
│   └── uart.S             # virtual UART input/output
└── generated/
    ├── alu_comb.S         # combinatorial ALU tests (generated)
    └── branch_comb.S      # combinatorial branch tests (generated)
```

## Test Philosophy

Each workload is a standalone flat binary loaded at `0x20000000`.  It:

1. sets up the UART base address in `x3`,
2. executes a series of self-checking subtests,
3. prints `PASS\n` and executes `ebreak` on success, or
4. prints `FAIL\n` and executes `ebreak` on the first failure.

The runner compiles every `.S` file with the `workload-build` skill, executes it
in the emulator with UART output captured to a file, and verifies the file
contains exactly `PASS\n`.

## Coverage

### Hand-written tests (`src/`)

| File | What's tested | Edge cases |
|------|---------------|------------|
| `alu.S` | `addi/add/sub`, `xori/xor`, `ori/or`, `andi/and` | zero/sign-extended immediates, overflow, `x0` source/dest |
| `shift.S` | `slli/sll`, `srli/srl`, `srai/sra` | shift by 0, 1, 31, and values whose low 5 bits differ from the raw value |
| `compare.S` | `slti/slt`, `sltiu/sltu` | `-1` vs `0`, `INT_MIN` vs `INT_MAX`, mixed signs, unsigned wrap |
| `lui_auipc.S` | `lui`, `auipc` | large immediates, PC-relative result |
| `branch.S` | `beq/bne/blt/bge/bltu/bgeu` | taken/not-taken, forward/backward, boundary comparisons |
| `jal_jalr.S` | `jal`, `jalr` | forward/backward, `rs1+imm` target, bit-0 clearing, misaligned targets |
| `load_store.S` | `lb/lh/lw/lbu/lhu`, `sb/sh/sw` | endianness, sign/zero extension, misaligned faults |
| `csr.S` | all implemented CSRs, all six CSR instructions | read-only CSRs, write-side effects, `x0`/`uimm=0` suppression, illegal CSR |
| `exception.S` | `ecall`, illegal instructions, reserved registers, misaligned fetch/load/store, illegal CSR | `mcause` values, `mepc`/`mtvec` behavior |
| `misc_priv.S` | `fence`, `fence.i`, `wfi`, `mret` | architectural nop behaviour, return from mepc |
| `clint.S` | `mtime`/`mtimeh` | read, write, cross-increment observation |
| `uart.S` | UART TX/RX | output bytes, input bytes from a pre-loaded sequence |

### Generated tests (`generated/`)

| File | Generator | What's covered |
|------|-----------|----------------|
| `alu_comb.S` | `scripts/gen_alu_tests.py` | Every OP-IMM and OP instruction against a grid of operands (zero, ±1, small, large, sign extremes) |
| `branch_comb.S` | `scripts/gen_branch_tests.py` | Every branch condition against a grid of signed/unsigned operand pairs |

The generators make it easy to add more operand values or new instructions
later without hand-editing hundreds of cases.

## Running the Tests

From the repository root:

```bash
./workloads/isa-test/run_tests.sh
```

The script returns `0` when every workload prints `PASS\n` and non-zero
otherwise.  It leaves build artifacts (`*.o`, `*.bin`, `*.lst`) and UART
output files (`*.out`) next to each source file, and it re-runs any failing
test with `--log 1` to capture the final instructions for debugging.

If a workload needs non-default emulator flags (e.g. `--strict-mem`), create
a `<stem>.flags` file next to the source containing the extra flags; the
runner will append them to the emulator command line.

To run a single test manually:

```bash
source data/workload-build/config.sh
./skills/workload-build/build.sh workloads/isa-test/src/alu.S workloads/isa-test/src/alu
./emulator/build/emulator -e "load workloads/isa-test/src/alu.bin 0x20000000; uart output workloads/isa-test/src/alu.out; run; exit"
cat workloads/isa-test/src/alu.out
```

## Adding a New Test

1. Create `src/<name>.S`.
2. Include the macros with `.include \"common/test_macros.S\"` at the top.
3. Call `SETUP_UART` once, then run subtests using `CHECK_EQ` / `CHECK_NE`.
4. Fall through to `PASS` or branch to the local `fail:` label.
5. Re-run `./workloads/isa-test/run_tests.sh`.
