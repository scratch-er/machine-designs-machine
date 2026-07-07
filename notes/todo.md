# TODO

## Current state

- Project uses a single `uv` workspace at the repo root (`machine-designs-machine`).
- `tools/*` are workspace members; dependencies resolve into one root `.venv`.
- `semantic-search` is the only tool so far.
- Full spec index exists at `data/semantic-search/index.db`.
- Emulator design written in `notes/emulator-design.md`.
- Difftest design written in `notes/difftest-design.md`.
- Core design written in `notes/core-design.md`.
- Detailed datapath and control-signal plan added to `notes/core-design.md` (single-cycle baseline + 5-stage pipeline).
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
- [x] Port AbstractMachine to `riscv32e-npc` and run AM kernels on the emulator.
- [x] Replace klib string/stdlib/stdio implementations with sonnet-libc.

## Next steps

- If UART must be modeled in RTL simulation, implement it in the DPI-C external bus/crossbar model, not in the core; generally ignore RTL UART writes during difftest and trust the emulator output if architectural execution matches.  
- Refactor single-cycle core into the five-stage pipeline with I-cache, forwarding, and hazard logic.  
- Re-run difftest and collect I-cache AMAT counters.  

## Deferred

- GoogleTest migration, watchpoints, in-memory snapshots, Spike adapter, real AM interrupts, multi-core.
- Advanced branch prediction, clock gating, cache size exploration.
- Full RTL checkpoint save/restore.

## Deferred

- GoogleTest migration, watchpoints, in-memory snapshots, Spike adapter, real AM interrupts, multi-core.
- Advanced branch prediction, clock gating, cache size exploration.

## Recent notes

- Completed AbstractMachine port for `riscv32e-npc`:
  - Fixed `arch/riscv.h` Context layout to match `trap.S`.
  - Implemented TRM (`putch` via UART, `halt` via `ebreak`), IOE (UART TX, timer uptime from CLINT `mtime`), CTE (`yield`/`ecall`, `kcontext`), MPE (single-core stubs), VME stubs.
  - Switched AM build system to Clang/LLVM with configurable toolchain via `data/workload-build/config.sh`.
  - Aligned AM memory map with the emulator at `0x20000000`.
- Implemented emulator ELF loader and verified it loads RISC-V ELF32 executables.
- `workloads/am-kernels/kernels/hello` prints correctly with `make run`.
- `workloads/am-kernels/kernels/yield-os` successfully context-switches between two threads.
- Emulator unit tests and ISA test suite (21 tests) still pass.
- Replaced klib stdio/stdlib/string with implementations adapted from sonnet-libc (https://gitlink.org.cn/foobat/sonnet-libc):
  - `klib/src/string.c`, `klib/src/stdlib.c`, `klib/src/stdio.c` now use sonnet-libc core code.
  - Added klib-specific adapters: `calloc`/`realloc`, `strchr`/`strrchr`, `vprintf`, `vsscanf`/`sscanf`/`__isoc99_sscanf`, fixed `vsnprintf` null-termination.
  - Added `malloc` 8-byte alignment for RISC-V strict alignment.
  - Added shim headers `klib/include/{stdio.h,stdlib.h,string.h}` so AM kernels can include standard headers.
  - `klib/include/klib.h` now declares `putchar`/`puts`.
  - `workloads/am-kernels/kernels/demo` builds and reaches its menu.
- Updated `notes/core-design.md` with the complete datapath, control-signal, pipeline, and AXI plan before writing RTL.
- Chose a 5-stage pipeline with static not-taken prediction, flip-flop I-cache, and shared AXI4 master for the full implementation; the first RTL milestone will be a single-cycle baseline for fast difftest bring-up.
- Unified RTL build with emulator CMake (`BUILD_RTL` option). `cmake -DBUILD_RTL=ON` produces `emulator`, `emulator-rtl`, and `npc-difftest`.
- Implemented single-cycle baseline `npc_core` in Verilog with DPI-C memory backend, commit interface, and debug ports.
- Added `RtlISS` adapter implementing the emulator `ISS` interface so the shell and difftest harness drive the RTL directly.
- Fixed single-cycle RTL difftest bugs in CSR immediates, load data extraction, `fence.i` decode, and CLINT write/tick ordering.
- `npc-difftest` passes all core ISA workloads except `uart`, which is intentionally excluded until RTL difftest has an external UART/MMIO model: `alu`, `branch`, `branch_misaligned`, `clint`, `compare`, `csr`, `csr_all`, `ecall`, `exception`, `exception_priority`, `illegal_inst`, `jal_jalr`, `load_store`, `lui_auipc`, `mem_fault`, `misc_priv`, `mret`, `shift`, `alu_comb`, `branch_comb`.
- UART is external to the RTL core. If needed in RTL simulation, emulate it in the DPI-C bus/crossbar model; in normal difftest, ignore RTL UART output and rely on the emulator output when commit events match.
- More complex RTL difftest workloads now pass: AM CoreMark completes all 1,000 iterations and matches for 756,865,056 retired instructions (`CoreMark PASS 386 Marks`, `7568 ms` from CLINT time reads); RT-Thread AM runs through its scripted shell workload and halts with matching commit events after 567,442 instructions.
- Completed RT-Thread AM BSP port under `workloads/rt-thread-am/bsp/abstract-machine`; see `notes/rtthread-am-port.md` for details.
  - Restored the default `.config` (DFS enabled) after discovering it had been stripped; switched from `elm-fat` to `ramfs`/`romfs`/`devfs` and added `RT_USING_MEMHEAP`.
  - Added POSIX headers `include/fcntl.h`, `include/string.h`, `include/unistd.h`, and wired the BSP `include/` path ahead of AM/RT-Thread headers.
  - Fixed `Makefile` so `rtconfig.h` is regenerated when `.config` changes, and removed the broken Python injection of `extra.h`.
  - Implemented `halt` MSH command and extended the UART-fed command sequence with `pwd`, `ls`, `memtrace`, `memcheck`; all execute and `halt` stops the emulator cleanly.
  - Also fixed an alignment bug in `components/libc/compilers/common/ctime.c`: `asctime_r` used `*(int*)` reads on the `days`/`months` string literals, which were not guaranteed 4-byte aligned in this BSP's rodata layout. Changed the declarations to use separately aligned arrays.
