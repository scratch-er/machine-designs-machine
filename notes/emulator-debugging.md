# Emulator Debugging Strategy

## Why the current workflow is not enough

The ISA tests and the `run_tests.sh` harness work well for small workloads that
terminate quickly.  But real-world programs (compiled C, Dhrystone, CoreMark,
RT-thread, etc.) will run thousands or millions of instructions.  The current
approach has several weaknesses:

- `run n` runs until completion or until the budget is hit.  If the program
  dead-locks, the emulator just spins until the budget is exhausted, producing
  a huge trace.
- `--log 1` traces every retired instruction.  The trace files grow linearly
  and slow the emulator down, and the interesting failure point is buried in
  the middle.
- There is no way to stop at a particular PC, register value, UART output, or
  instruction count and inspect state.
- There is no way to enable/disable tracing dynamically: either the whole run
  is logged or nothing is.
- The trace format is fixed.  We cannot ask for only branches, only stores,
  only changes to a particular register, etc.

To debug real programs efficiently we need **targeted observation** instead of
full logging.

## Design principles

1. **Fail fast**: if a workload behaves unexpectedly, find the divergence point
   with minimal overhead.
2. **Minimal noise**: only log what is relevant.
3. **Reproducibility**: every debugging session should be scriptable so we can
   re-run exactly the same steps after a code change.
4. **Two-phase debugging**:
   - *Effective mode*: run with lightweight checks to locate the failure region.
   - *Verbose mode*: zoom in on that region with full logging, single-stepping,
     and state inspection.

## Required emulator features

### 1. Breakpoints on PC

Add a `break <addr>` / `delete-break <addr>` / `clear-breaks` shell command.
The emulator checks the list after every instruction retirement and pauses
before executing the next instruction.  When paused it prints the current PC
and accepts the next command.

Use cases:
- Stop right after `_start`, after `main`, or at a known fault handler.
- Stop at the instruction just before the one where divergence is expected.

### 2. Conditional / data breakpoints (optional but very useful)

- `watch reg <i> [value]`: stop when register `xi` changes or equals `value`.
- `watch mem <addr> <size> [value]`: stop on memory write.
- `watch uart <string>`: stop when the given string is written to the UART.

These are heavier than PC breakpoints but much lighter than full tracing.

### 3. Run-until / run-to semantics

Extend `run` so it can stop at a condition:

```
run 10000                # run up to 10000 instructions
run to 0x20000400        # run until PC == 0x20000400
run until uart "FAIL\n"  # run until this string is emitted
run until reg 10 0xdeadbeef
```

This avoids the "run forever" problem and makes scripts deterministic.

### 4. Single-step and small-step commands

- `step [n]`: already exists, execute n instructions.
- `step over`: execute the current instruction but do not follow calls/jumps
  (for subroutine-level stepping).
- `step until <pc>`: step until PC reaches the target.

### 5. Dynamic log-level control and selective tracing

Add shell commands:

- `log <level>`: already exists.
- `trace on [filter]`, `trace off`: enable/disable tracing without changing
  the global log level.
- `trace filter <kind>` where kind is one of:
  - `all`
  - `branches`
  - `loads`
  - `stores`
  - `exceptions`
  - `reg <i>`
  - `pc <low> <high>`

This lets us enable tracing only around the region of interest.

### 6. Snapshot / rewind

Add a lightweight snapshot command:

```
snapshot save <name>   # save current state in memory
snapshot load <name>   # restore state
snapshot list          # show in-memory snapshots
snapshot delete <name>
```

This lets us "run to near the bug, save snapshot, then step/trace without
re-executing the whole program".

### 7. Last-N instruction ring buffer

Always keep the last N retired instructions in a circular buffer (e.g. 64 or
256 entries) even when tracing is off.  On a fault, `backtrace` or `last`
prints the buffer.  This gives context without the cost of writing to disk.

### 8. Program-counter histogram / hot-loop detection

Maintain a small map of retired PCs.  A command `hot` shows the most frequently
retired PCs.  This detects spin-loops immediately.

### 9. Watchdog / hang detection

Command-line option `--max-cycles <N>` and `--max-pc-stuck <N>`:

- `--max-cycles`: terminate after N cycles (already partially supported via
  `run N`).
- `--max-pc-stuck`: terminate if the same PC is retired more than N times
  without an exception.  This catches tight fault loops (e.g. instruction
  access fault at PC=0).

### 10. Diff-friendly trace modes

The current trace format is good for difftest.  Add:

- `--trace-pc-only`: one hex PC per line, very compact.
- `--trace-gpr-writes`: only log register writes.
- `--trace-mem`: only log memory addresses and data.

These are useful when comparing two runs.

## Proposed shell command additions

| Command | Description |
|---------|-------------|
| `break <addr>` | set PC breakpoint |
| `delete-break <addr>` | remove PC breakpoint |
| `clear-breaks` | remove all breakpoints |
| `watch reg <i> [value]` | stop on register change/value |
| `watch mem <addr> <size> [value]` | stop on memory write |
| `watch uart <string>` | stop when UART output contains string |
| `run to <addr>` | run until PC matches |
| `run until uart <string>` | run until UART output matches |
| `run until reg <i> <value>` | run until register matches |
| `step over` | execute current instruction, skip into calls |
| `trace on [filter]` | enable selective tracing |
| `trace off` | disable selective tracing |
| `last [n]` | show last n retired instructions |
| `hot [n]` | show top n PCs |
| `snapshot save/load/list/delete <name>` | in-memory checkpoints |
| `dump state` | print concise architectural state |
| `--max-cycles <N>` | CLI option to terminate long runs |
| `--max-pc-stuck <N>` | CLI option to detect PC stuck loops |

## Suggested implementation order

1. **PC breakpoints** + `run to <addr>`: highest impact, small change.
2. **`last` ring buffer**: gives almost-free context on any stop.
3. **`run until uart <string>`**: real programs usually signal failure/success
   over UART; stopping on the first bad output is essential.
4. **`trace on/off` with filters**: makes verbose mode usable.
5. **Snapshots**: speeds up iterative debugging.
6. **Watchdog CLI flags**: prevents accidental infinite runs.
7. **Watchpoints and hot-loop detection**: nice to have once the above works.

## Scriptable debugging workflow

With the features above, a typical debugging session becomes:

```bash
# Effective mode: run until the program emits "FAIL" or finishes.
./emulator -e "load program.bin 0x20000000; run until uart 'FAIL\n'; dump state; snapshot save fail_point; trace on pc 0x20002000 0x20002100; step 100; exit"

# Verbose mode: re-run from the snapshot with full tracing around the failure.
./emulator -e "load program.bin 0x20000000; snapshot load fail_point; log 3; step 50; exit"
```

This is far more effective than "run with full logging and grep the trace".

## Files to modify

- `emulator/include/emulator/shell.h` and `emulator/src/shell.cpp`: add
  breakpoint/watchpoint tables, `run until` logic, `trace` filtering, `last`,
  `hot`, and snapshot commands.
- `emulator/include/emulator/emulator_iss.h` and `emulator/src/emulator_iss.cpp`:
  expose hooks for breakpoints, watchpoints, ring buffer, and snapshots.
- `emulator/src/main.cpp`: add `--max-cycles`, `--max-pc-stuck`, and
  snapshot-restore CLI options.
- `emulator/include/emulator/config.h`: add new config fields for limits.
- `notes/emulator-design.md`: document the new commands and workflow.

## Notes from ISA-test debugging

- Always disassemble generated/hacky `.word` encodings with `llvm-objdump -d -M
  no-aliases`; it is easy to write what looks like an illegal instruction but
  is actually a legal load/store.
- Per-test instruction budgets prevent hangs but can hide late bugs if the
  budget is too low.  Make budgets configurable or derived from workload size
  with a large safety margin.
- Capturing UART output to a file is the most reliable pass/fail signal.
- Re-running failed tests with `--log 1` is useful only if the failure is near
  the end; for failures in the middle of long runs we need breakpoints or
  selective tracing.
