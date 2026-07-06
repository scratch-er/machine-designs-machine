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
- [ ] Verify the emulator with more complex workloads before starting the processor core.
- [x] Write user-facing emulator documentation (README, architecture, workload guide).

## Recent notes

- Emulator builds with CMake in `emulator/build`; use `cmake --build emulator/build -j`.
- `compile_commands.json` is generated in `emulator/build`.
- Homebrew LLVM at `/opt/homebrew/Cellar/llvm/22.1.7_1/bin/` is used for RISC-V assembly (`llvm-mc`, `llvm-objcopy`).
- First workload: `workloads/asm/uart_pass/pass.S` prints "PASS\n" via UART and `ebreak`s.
- Tests live in `emulator/tests/test_main.cpp` and run with `ctest --test-dir emulator/build`.
- Git commit `5d6a1cf` added the emulator; binaries and build dirs are ignored.
- Added `workload-build` skill and `data/workload-build/config.sh`; defaults point to Homebrew LLVM.
- Rebuilt emulator, built `workloads/asm/uart_pass/pass.S` with the skill, and ran it successfully (`PASS`, 12 instructions).
- Also verified a tiny C workload using `skills/workload-build/crt0.S` (stack setup, `main`, then `ebreak`).
- The pending task is to verify the emulator with more complex workloads before starting the processor core.
