# Overall Project Plan

## Goal

Design a RISC-V CPU core (RV32E_Zicsr, M-mode only) and verify it against a reference emulator using differential testing.

## Milestones

### M1 — Reference emulator
*Completion gate: a C++ emulator can load a RISC-V binary and run it to a UART pass marker.* ✅

- `emulator/` build system with CMake.
- RV32E_Zicsr decoder and interpreter.
- Flat memory model, CLINT, and virtual UART.
- Shell command loop for interactive and scripted use.
- Binary loader; ELF loader deferred until C workloads arrive.
- Basic tracing and a small assembly workload that prints "PASS".

### M2 — Self-tested emulator
*Completion gate: unit tests pass and a difftest of two emulator instances stays locked.*

- Unit tests for decoder, ALU, memory access, CSRs, and exceptions.
- End-to-end tests for load/store, branches, ecall/ebreak, mret.
- Difftest harness comparing two `EmulatorISS` instances.

### M3 — Workload library
*Completion gate: AbstractMachine kernels build with Clang/LLVM and run on the emulator.* ✅

- Complete `riscv32e-npc` AM port (TRM, IOE, CTE, MPE stubs, VME stubs).
- Implement klib string/stdlib/stdio functions.
- Switch AM build system to configurable Clang/LLVM toolchain.
- Implement emulator ELF loader.
- Run `hello` and `yield-os` AM kernels successfully.

### M4 — Processor core
*Completion gate: RTL implements the spec and builds under Verilator.*

- Choose microarchitecture after M2 (single-cycle baseline first, then pipeline).
- AXI4 master, built-in CLINT, flip-flop instruction cache.
- Commit interface exposed for difftest.

### M5 — Differential verification
*Completion gate: RTL passes the same workload suite as the emulator.*

- Verilator adapter implementing the `ISS` interface.
- AXI testbench memory that reuses the emulator memory model.
- Commit-event difftest between emulator (reference) and RTL (DUT).

### M6 — Optimization and PPA
*Completion gate: core meets area/timing targets and performance is measured.*

- Pipeline tuning, critical-path reduction.
- Synthesis experiments and benchmark runs.

## Tooling

- C++17, CMake >= 3.16.
- Verilator for RTL simulation.
- `riscv64-unknown-elf-gcc` for workloads.
- GoogleTest for C++ unit tests (fetched by CMake or vendored).

## Notes

- Keep the emulator simple and correct; fancy features only when they help verification.
- Spike adapter is optional and deferred until after the RTL difftest works.
- All design details live in `notes/emulator-design.md`, `notes/difftest-design.md`, and `notes/core-design.md`.

## Current Next Step

Begin processor core design (M4): implement a single-cycle or pipelined RV32E_Zicsr core in Verilog, then verify it against the emulator using the difftest harness.
