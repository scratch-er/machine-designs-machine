# Workload Build Guide

This document explains how to build and run software on the emulator.

## Toolchain

The emulator itself does not need a RISC-V compiler. To build workloads you need either:

- A RISC-V cross-GCC such as `riscv64-unknown-elf-gcc` with `-march=rv32e -mabi=ilp32e`.
- LLVM with RISC-V support (`llvm-mc`, `llvm-objcopy`).

On macOS with Homebrew LLVM you might use:

```bash
export LLVM=/opt/homebrew/Cellar/llvm/22.1.7_1/bin
$LLVM/llvm-mc -triple=riscv32 -mattr=+e -filetype=obj pass.S -o pass.o
$LLVM/llvm-objcopy -O binary pass.o pass.bin
```

Adjust the `LLVM` path to match your installation.

## Writing an Assembly Test

A minimal test follows the standard ABI:

```asm
.section .text
.globl _start
_start:
    # Set UART base address in x3
    lui     x3, 0x10000

    # Print "PASS\n"
    li      x1, 'P'
    sb      x1, 0(x3)
    li      x1, 'A'
    sb      x1, 0(x3)
    li      x1, 'S'
    sb      x1, 0(x3)
    li      x1, 'S'
    sb      x1, 0(x3)
    li      x1, '\n'
    sb      x1, 0(x3)

    # Finish
    ebreak
```

## Building with llvm-mc

```bash
$LLVM/llvm-mc -triple=riscv32 -mattr=+e -filetype=obj pass.S -o pass.o
$LLVM/llvm-objcopy -O binary pass.o pass.bin
```

`-mattr=+e` enables the RV32E register set. Without it, the assembler accepts `x16`–`x31` and the emulator will trap on those instructions.

## Running on the Emulator

```bash
./emulator/build/emulator -e "load pass.bin 0x20000000; run; exit"
```

Expected output:

```
PASS
ran 12 instructions
```

## Building with a GCC Cross-Compiler

If you install `riscv64-unknown-elf-gcc`:

```bash
riscv64-unknown-elf-gcc -march=rv32e -mabi=ilp32e -nostdlib -Ttext=0x20000000 pass.S -o pass.elf
riscv64-unknown-elf-objcopy -O binary pass.elf pass.bin
```

The emulator's ELF loader is deferred, so use `objcopy` to produce a raw binary for now.

## Standard Test ABI

Tests should communicate pass/fail by writing to the virtual UART and then stopping with `ebreak`:

- Pass: write `"PASS\n"`, then `ebreak`.
- Fail: write `"FAIL\n"`, then `ebreak`.

The emulator prints UART bytes to stdout, so a human can read the result directly. Automated harnesses can capture the UART output string or check the exit/halt state.

## Common Pitfalls

- **Wrong base address**: the default reset vector is `0x20000000`. Load binaries there unless you override `--reset-vector`.
- **UART base mismatch**: the virtual UART is at `0x10000000`. Match this in your code.
- **Compressed instructions**: RV32E does not include the C extension. Do not use `.option rvc`.
- **Register range**: `x16`–`x31` are reserved in RV32E and will cause an illegal-instruction exception.
- **Alignment**: all loads/stores/fetches must be naturally aligned.

## Future Additions

- C runtime and newlib support once the ELF loader is implemented.
- Makefile rules under `workloads/` for common build patterns.
- A test harness that runs every workload and checks PASS/FAIL output.
