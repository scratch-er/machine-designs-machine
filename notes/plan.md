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

- Single-cycle AXI baseline first, then required flip-flop I-cache, then five-stage pipeline.
- AXI4 master, built-in CLINT, flip-flop instruction cache.
- Commit interface exposed for difftest.

### M5 — Differential verification
*Completion gate: RTL passes the same workload suite as the emulator.*

- Verilator adapter implementing the `ISS` interface.
- AXI testbench memory that reuses the emulator memory model.
- Commit-event difftest between emulator (reference) and RTL (DUT).

### M6 — ysyxSoC integration
*Completion gate: pipelined RTL boots or runs a smoke workload through real ysyxSoC AXI peripherals.*

- Connect the core to the ysyxSoC bus and memory devices after I-cache and pipeline are stable.
- Refactor the ISS/memory model from a single flat RAM assumption to explicit memory devices.
- Model ysyxSoC memories with these properties:
  - MROM: loadable by simulator, read-only to processor (`l-`).
  - flash: loadable by simulator, read-only to processor (`l-`).
  - PSRAM: not loadable by simulator, writable by processor (`-w`).
  - All memories are readable by the processor.
- Add DPI-C hooks for RTL/core reads from MROM and flash.
- Build VCD/AXI waveform analysis tooling before deep real-peripheral debugging.

### M7 — Optimization and PPA
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

Continue M4/M5 in the controlled RTL difftest environment: implement the required flip-flop I-cache on the existing AXI baseline, then refactor into the five-stage pipeline. Defer ysyxSoC peripheral integration until the pipelined core is stable, because connecting real peripherals now would force an ISS/memory-map refactor and likely require re-debugging AXI after cache/pipeline timing changes.
