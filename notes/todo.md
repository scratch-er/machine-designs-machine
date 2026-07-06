# TODO

## Current state

- Project uses a single `uv` workspace at the repo root (`machine-designs-machine`).
- `tools/*` are workspace members; dependencies resolve into one root `.venv`.
- `semantic-search` is the only tool so far.
- Full spec index exists at `data/semantic-search/index.db`.
- Emulator design written in `notes/emulator-design.md`.
- Difftest design written in `notes/difftest-design.md`.
- Core design written in `notes/core-design.md`.
- Overall plan updated in `notes/plan.md`.

## Reminders for future work

- Run `uv sync --all-packages` from the repo root when dependencies change.
- Run `uv run tools/semantic-search/search.py --index data/semantic-search/index.db "query"` to search specs.
- Run `uv run tools/semantic-search/index.py --source-root specs --index data/semantic-search/index.db` to update the index after spec changes.
- Add future Python tools as workspace members under `tools/`.
- The `semantic-search` skill is available for spec questions.
- The `workload-build` skill is available for compiling/assembling RISC-V workloads.
  - Config and toolchain paths live in `data/workload-build/config.sh`.
  - Linker script and `crt0.S` live in `skills/workload-build/`.
  - Build helper: `skills/workload-build/build.sh <source.S|source.c> [output-stem]`.
  - Builds produce `<stem>.bin` and `<stem>.lst`.

## Pending tasks

- [x] Compile the complete RV32E_Zicsr instruction list into `notes/instruction-list.md`.
- [x] Set up the emulator build system in `emulator/`.
- [x] Implement the decoder and basic RV32E interpreter.
- [x] Add memory, CLINT, UART, and shell.
- [x] Create a small assembly workload and run it on the emulator.
- [x] Verify the emulator with more complex workloads before starting the processor core.
- [x] Create ISA test suite under `workloads/isa-test/` and run it successfully.
- [x] Write user-facing emulator documentation (README, architecture, workload guide).
- [x] Add debugging features: breakpoints, run-until, trace filters, last-N ring buffer, watchdog limits.
- [x] Implement difftest harness and self-test.
- [x] Expand unit tests and ISA workloads for exception paths and CSR variants.

## Next steps

- Begin processor core design (M4): choose microarchitecture, implement single-cycle or pipelined RV32E_Zicsr in Verilog.
- Use the difftest harness to compare RTL against the emulator (M5).
- Defer: GoogleTest migration, watchpoints, in-memory snapshots, Spike adapter, ELF loader.

## Recent notes

- Added debugging features to the emulator: PC breakpoints, `run to`/`run until`, selective trace filters, `last` ring buffer, `dump state`, and `--max-cycles`/`--max-pc-stuck` watchdog flags.
- Implemented the difftest harness in `emulator/src/difftest.cpp` with a self-test in `emulator/tests/test_main.cpp`.
- Expanded C++ unit tests to cover decoder formats, RV32E register checks, all ALU/shift/compare instructions, branch/jump misaligned targets, load/store sizes and faults, all CSR instruction variants, and exception priorities.
- Added ISA workloads: `ecall.S`, `mret.S`, `branch_misaligned.S`, `csr_all.S`, `exception_priority.S`. The suite now has 21 workloads and all pass.
- Updated `notes/emulator-design.md` with the new shell commands, trace filters, and difftest harness.
