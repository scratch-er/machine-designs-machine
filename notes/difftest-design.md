# Difftest Design Notes

## Goal

Compare the architectural behavior of the reference emulator against the RTL core, instruction by instruction, so that the first point of divergence is reported immediately.

## Components

- **Reference** (`EmulatorISS`): software interpreter described in `notes/emulator-design.md`.
- **DUT** (`RtlISS`): Verilator wrapper around the processor core.
- **AXI memory model**: a cycle-accurate AXI responder that reuses the emulator's `Memory` backend.
- **Difftest harness**: drives the reference and DUT, compares `CommitEvent`s, and stops on mismatch.
- **Clock source**: shared object that advances `mtime` and provides a common cycle count when timing-aware tests are needed.

## RTL Adapter (`RtlISS`)

`RtlISS` implements the same `ISS` interface as `EmulatorISS`:

- `reset(addr)`: apply reset, set reset vector parameters, release reset.
- `step_cycle()`: evaluate Verilator for one clock cycle and tick the AXI memory model.
- `step_inst(CommitEvent& out)`: call `step_cycle()` repeatedly until a commit signal is observed or a timeout is reached.
- State queries (`pc()`, `reg(i)`, `csr(addr)`) read the Verilated model's wires.

The RTL core must expose a commit interface. Minimum signals:

```verilog
output        commit_valid,
output [31:0] commit_pc,
output [31:0] commit_inst,
output [4:0]  commit_rd,
output [31:0] commit_rd_value,
output        commit_exception,
output [31:0] commit_cause,
output [31:0] commit_next_pc
```

If the core is pipelined, `commit_valid` is asserted in the cycle an instruction retires. On an exception, `commit_exception` is asserted and `commit_inst` may be invalid; the harness compares `pc`, `cause`, and `next_pc`.

## AXI Memory Model

The core exposes a 32-bit AXI4 master. The testbench implements an AXI slave connected to the emulator's `Memory` backend:

- Accepts read/write address channels with a small latency.
- Responds with SLVERR/DECERR for accesses that the memory model flags as invalid.
- Supports read bursts for instruction cache fills.
- Sits in the same Verilator evaluation loop as the core.

The AXI slave is cycle-accurate but functionally identical to the emulator memory. This catches AXI protocol bugs in the core while keeping the reference model simple.

## Synchronization

### Commit-driven synchronization

The main difftest loop is driven by instruction commits, not lockstep cycles:

```cpp
while (true) {
    CommitEvent ref_event, dut_event;
    bool r1 = ref.step_inst(ref_event);
    bool r2 = dut.step_inst(dut_event);
    if (!r1 || !r2) { /* one side halted; compare halt reason */ break; }
    if (ref_event != dut_event) { report_mismatch(); break; }
}
```

This works because both models execute the same dynamic instruction stream in the same order. The DUT may take many cycles per instruction; `RtlISS::step_inst()` hides that from the harness.

### Handling `mtime`

Because the reference interpreter is 1-IPC and the RTL may be pipelined, the raw cycle count will differ for the same instruction sequence. Programs that read `mtime` therefore see different values.

Options:

1. **Avoid timing-sensitive programs in correctness difftest.** Most ISA tests do not read `mtime`; this is the simplest approach and is the default.

2. **Shared clock for timing-aware tests.** Both the emulator's CLINT and the RTL's CLINT are driven from the same `Clock` object:
   - `Clock::tick()` is called once per Verilator cycle.
   - `EmulatorISS` does not tick the clock on its own; instead, the difftest harness ticks the clock once for each RTL cycle.
   - When the reference executes an instruction that reads `mtime`, it reads the current `Clock` value.
   - This requires the reference to execute instructions "in lockstep" with RTL commits rather than freely running ahead.

For M5 we use option 1. Option 2 is documented for later benchmarking.

## Commit Event Comparison

Fields compared at each commit:

- `pc` must match.
- `inst` must match (skipped on exception).
- `rd` and `rd_value` must match when `rd != 0`.
- `exception` and `cause` must match.
- `next_pc` must match.

`cycle` is **not** compared directly because of the 1-IPC vs pipelined timing difference. For timing-aware tests, the harness may compare `mtime` separately using the shared clock.

## Trace Cross-Reference

Both models emit the same trace line format:

```
R=<retire_idx> C=<cycle> PC=<pc> I=<inst> RD=<rd> RV=<value> NPC=<next_pc> EXC=<exc> CAUSE=<cause>
```

`R=<retire_idx>` is a monotonically increasing retirement index shared between reference and DUT; it is the primary key for trace comparison. `C=<cycle>` is model-specific and not compared directly.

## Halt and Timeout

- **Halt**: the emulator sets `halted` when an `ebreak` is encountered (used as the test-finish marker) or on a fatal fault. The RTL adapter should detect the same `ebreak` commit.
- **Timeout**: `RtlISS::step_inst()` limits the number of cycles it waits for a commit. If no commit appears within, say, 10,000 cycles, it returns false and the harness reports a timeout hang.

## Workload ABI for Difftest

Tests use a simple convention:

- Pass: write `"PASS\n"` to the UART, then execute `ebreak`.
- Fail: write `"FAIL\n"` to the UART, then execute `ebreak`.
- The harness can also detect pass/fail by watching UART output, which is useful for long-running programs.

## Testing Stages

1. **Self-test**: two `EmulatorISS` instances, confirm they match.
2. **Decoder-only test**: if a Verilog decoder module is built early, compare decoded instruction fields against the emulator decoder.
3. **Single-cycle core**: compare against the reference instruction by instruction.
4. **Pipelined core**: same as above; the commit-driven approach should still work.
5. **I-cache verification**: include tests that exercise cache hits, misses, and `fence.i`.

## Deferred Items

- Spike adapter: not part of M5; can be added later as an alternative reference.
- Multi-core / AXI coherency: out of scope (single core only).
- Performance counters for AMAT: collected from RTL signals during benchmark runs, not during correctness difftest.
