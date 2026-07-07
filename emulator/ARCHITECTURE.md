# Emulator Architecture

This document describes the internal architecture of the reference emulator for maintainers and anyone extending it. For user-facing usage instructions, see `emulator/README.md`.

## Design Goals

1. **Correctness first** — match `specs/core.md` and the RISC-V ISA manual exactly.
2. **Observability** — expose every architectural event needed for difftest.
3. **Scriptability** — run interactively or from files/commands.
4. **Extensibility** — share the `ISS` interface across the interpreter, Verilator RTL, and difftest adapter.

## Component Overview

```
+------------------------------------------------+
|                     Shell                      |
+------------------------------------------------+
|                      ISS                       |  (abstract interface)
+-------------+----------------+-----------------+
| EmulatorISS |     RtlISS     |    Difftest     |
|             |                | ref ISS + DUT ISS|
+-------+-----+-----+----------+-----------------+
| Hart  | Memory/MMIO | Verilated npc_core       |
+-------+-------------+--------------------------+
|              Trace / Config                    |
+------------------------------------------------+
```

## `ISS` Interface

`include/emulator/iss.h` defines the contract shared by all execution engines. `EmulatorISS`, `RtlISS`, and `Difftest` all implement it, so the shell can drive the interpreter, RTL simulation, or commit-by-commit comparison with the same command sequence.

Key methods:

- `reset(addr)` — reset architectural state.
- `step_cycle()` — advance one clock cycle.
- `step_inst(event)` — advance until one instruction retires or an exception is taken, filling a `CommitEvent`.
- State queries: `pc()`, `reg(idx)`, `csr(addr)`, `read_mem(...)`, `write_mem(...)`.
- Loading and checkpointing: `load_bin`, `load_elf`, `save_checkpoint`, `load_checkpoint`.
- Difftest support: `Difftest` in `src/difftest.cpp` drives two `ISS` implementations and reports the first mismatch.

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

Recent additions to the command set:

- `break` / `delete-break` / `clear-breaks` / `list-breaks` for PC breakpoints.
- `run to <addr>`, `run until uart <string>`, and `run until reg <i> <value>` for run-until semantics.
- `trace on <filter>` / `trace off` for selective tracing.
- `last [n]` to inspect the last retired instructions without full logging.
- `dump state` for a concise architectural summary.

The `run` command honors global limits set by `--max-cycles` and `--max-pc-stuck`, which are useful when running potentially hung programs.

## Tracing

`Tracer` prints a line per retired instruction when log level is at least 1:

```
R=<retire_idx> C=<cycle> PC=<pc> I=<inst> RD=<rd> RV=<value> NPC=<next_pc> EXC=<exc> CAUSE=<cause>
```

`TraceFilter` allows runtime selection of which instructions are printed (`all`, `branches`, `loads`, `stores`, `exceptions`, `reg i`, `pc low high`). When a filter is active, matching instructions are traced and sub-traces are restricted to the same category. This is useful for zooming in on a bug region without generating a full trace.

This format is designed to be diffed against an RTL trace log.

## Difftest ISS Adapter

The emulator and RTL both produce `CommitEvent`s. `Difftest` in `src/difftest.cpp` wraps two `ISS` instances, usually an `EmulatorISS` reference and an `RtlISS` DUT, and implements `ISS` itself.

`Difftest::step_inst()` advances both sides by one retired instruction, compares the commit events, and returns the reference event to the shell. On mismatch, asymmetric halt, or DUT timeout, it records `last_mismatch()`, prints a concise diagnostic to stderr, marks itself failed, and stops. State queries (`pc`, `reg`, `csr`, `read_mem`) return the reference side after successful comparisons.

Program loading and direct memory writes are applied to both sides. Checkpointing is intentionally not implemented for difftest because a correct snapshot must include both model states; checkpoint commands print a warning and fail in this mode.

The unified RTL-capable CLI is `emulator-rtl`:

- default mode: interpreter reference,
- `--rtl`: RTL backend,
- `--difftest`: reference-vs-RTL difftest backend.

See `notes/difftest-design.md` for the original design notes.

## Adding a New Instruction

If the processor core ever adds an extension:

1. Add the instruction to `InstType` and the decode table in `src/decoder.cpp`.
2. Decode any new immediates or fields in `decode()`.
3. Implement execution in `EmulatorISS::execute()`.
4. Add a unit test in `emulator/tests/test_main.cpp`.
5. Update `notes/instruction-list.md`.

## Testing Strategy

- Decoder tests: verify known encodings produce the right type and fields, including illegal/unknown opcodes and RV32E register checks.
- ALU/shift/compare tests: exercise every arithmetic/logical operation with edge operands.
- Memory tests: aligned access, unaligned faults, byte ordering, strict-mode faults, MMIO.
- CSR tests: read/write implemented CSRs, all six CSR instruction variants, illegal CSR exception.
- Exception tests: misaligned fetch, misaligned branch/jump targets, `ebreak`, `ecall`, `mret`, exception priority.
- Difftest self-test: confirm two `EmulatorISS` instances match and that injected mismatches are detected.
- End-to-end: compile a workload, run it, check UART output.

Run tests with `ctest --test-dir emulator/build --output-on-failure`. The ISA workload suite under `workloads/isa-test/` is run with `./workloads/isa-test/run_tests.sh`.
