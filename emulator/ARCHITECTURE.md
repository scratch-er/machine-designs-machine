# Emulator Architecture

This document describes the internal architecture of the reference emulator for maintainers and anyone extending it. For user-facing usage instructions, see `emulator/README.md`.

## Design Goals

1. **Correctness first** — match `specs/core.md` and the RISC-V ISA manual exactly.
2. **Observability** — expose every architectural event needed for difftest.
3. **Scriptability** — run interactively or from files/commands.
4. **Extensibility** — share the `ISS` interface with a future Verilator RTL adapter.

## Component Overview

```
+----------------------------------+
|              Shell               |
+----------------------------------+
|               ISS                |  (abstract interface)
+----------------------------------+
|           EmulatorISS            |
+-------+------+---------+---------+
| Hart  |Memory|  Clint  |  Uart   |
+-------+------+---------+---------+
|        Trace / Config            |
+----------------------------------+
```

## `ISS` Interface

`include/emulator/iss.h` defines the contract shared by all execution engines. The reference interpreter implements it today; the RTL Verilator adapter will implement it later.

Key methods:

- `reset(addr)` — reset architectural state.
- `step_cycle()` — advance one clock cycle.
- `step_inst(event)` — advance until one instruction retires or an exception is taken, filling a `CommitEvent`.
- State queries: `pc()`, `reg(idx)`, `csr(addr)`, `read_mem(...)`, `write_mem(...)`.
- Loading and checkpointing: `load_bin`, `load_elf`, `save_checkpoint`, `load_checkpoint`.

`CommitEvent` is the atomic unit of comparison in difftest. It records the PC, instruction, destination register, register value, exception flag, cause, and next PC at retirement.

## `Hart` — Architectural State

`include/emulator/hart.h` stores:

- `pc`
- `x[16]` for RV32E
- CSRs: `mstatus`, `mepc`, `mtvec`, `mcause`
- Read-only `mvendorid` and `marchid`
- `halted` flag

Rules enforced in `Hart`:

- `x0` reads as zero; writes are ignored.
- CSR writes to `mepc`/`mtvec` mask the low two bits (IALIGN = 32).
- `mstatus` keeps MPP hardcoded to M-mode (`0x00001800`).
- `take_exception(cause, tval)` sets `mepc` to the current PC, `mcause` to the cause, and jumps to `mtvec`.

## Decoder

`src/decoder.cpp` walks a table of `(mask, match, InstType)` entries. The table uses full masks so that instructions are not confused with one another (e.g. `lui` vs. any other opcode-only encoding, or `add` vs. `sub`).

After matching, the decoder extracts fixed fields and reconstructs immediates:

- I/S/B/U/J formats per the RISC-V spec.
- CSR instructions set `is_csr_imm` and decode the 5-bit zero-extended immediate.

Any 32-bit encoding that does not match a table entry becomes `InstType::UNKNOWN` and raises an illegal-instruction exception during execution.

## Execution Loop

`EmulatorISS::step_inst` performs:

1. Fetch a 32-bit word from memory at `pc`.
2. Decode it.
3. Validate register specifiers against RV32E (`x0`–`x15`).
4. Execute the instruction and compute `next_pc`.
5. Handle load/store through the memory/MMIO dispatch.
6. Commit the result, update `pc`, tick the CLINT, increment the cycle counter.

Fetch faults and misaligned targets are handled before execution. Load/store faults are handled inside the memory access path.

## Memory Model

`Memory` owns a flat byte vector for RAM. All accesses are little-endian and naturally aligned. Access outside RAM returns `0` for loads and is ignored for stores unless `strict_mem` is enabled.

MMIO dispatch in `EmulatorISS::execute` routes CLINT and UART accesses before calling `Memory`.

## CLINT

`Clint` models only `mtime` and `mtimeh` as a 64-bit counter. `tick()` increments by one per cycle. Other CLINT offsets are silently ignored. This matches the project spec override in `specs/core.md`.

## UART

`Uart` is a minimal byte channel:

- Write appends a byte to the output buffer and stdout (or a file if configured).
- Read pops a byte from the configured input sequence, or returns `0xFF` when empty.
- No status/control registers are implemented.

## Shell

`Shell` tokenizes lines, splits on semicolons, and dispatches commands. It is intentionally simple: no quoting, no variables, no control flow. It is enough for automated test scripts and light interactive debugging.

## Tracing

`Tracer` prints a line per retired instruction when log level is at least 1:

```
R=<retire_idx> C=<cycle> PC=<pc> I=<inst> RD=<rd> RV=<value> NPC=<next_pc> EXC=<exc> CAUSE=<cause>
```

This format is designed to be diffed against an RTL trace log.

## Difftest Preparation

The emulator already produces `CommitEvent`s. A future `RtlISS` will implement the same `ISS` interface, drive Verilator and the AXI memory model, and return one `CommitEvent` per retired instruction. The harness can compare events as they arrive or buffer them and compare later.

See `notes/difftest-design.md` for the full plan.

## Adding a New Instruction

If the processor core ever adds an extension:

1. Add the instruction to `InstType` and the decode table in `src/decoder.cpp`.
2. Decode any new immediates or fields in `decode()`.
3. Implement execution in `EmulatorISS::execute()`.
4. Add a unit test in `emulator/tests/test_main.cpp`.
5. Update `notes/instruction-list.md`.

## Testing Strategy

- Decoder tests: verify known encodings produce the right type and fields.
- ALU tests: exercise every arithmetic/logical operation.
- Memory tests: aligned access, byte ordering, MMIO.
- CSR tests: read/write implemented CSRs, illegal CSR exception.
- Exception tests: misaligned fetch, `ebreak`, `ecall`, `mret`, illegal instruction.
- End-to-end: compile a workload, run it, check UART output.

Run tests with `ctest --test-dir emulator/build --output-on-failure`.
