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

### Handling external/peripheral inputs (`mtime`, UART RX, etc.)

The reference interpreter is 1-IPC, while the RTL may stall on AXI, cache misses, pipeline hazards, or future wait states. Therefore `cycle` and retired-instruction count are not equivalent. This matters for peripherals whose outputs are not purely architectural state:

- CLINT `mtime` is a global time base and must tick every hardware cycle in the RTL, not every committed instruction.
- UART RX, future disk/network devices, and any other external input may become visible to the DUT at cycle-specific times.
- The reference can only stay comparable if it observes the same peripheral input values that the DUT observed, not values generated from the reference's own single-cycle timing.

The current baseline difftest workaround keeps `mtime` instruction-count based so existing CLINT tests pass. That is intentionally a temporary verification compromise and is not the architecturally correct RTL behavior.

The correct long-term model is **DUT-observed peripheral input replay**:

1. Standalone modes keep local peripherals:
   - `EmulatorISS` uses its own `Memory`, CLINT, UART, and input queues.
   - `RtlISS` uses its AXI simulation memory/MMIO devices.
   - No cross-model coupling exists outside difftest.

2. Difftest mode designates the RTL/DUT side as the source of external observations:
   - The AXI/MMIO responder records every peripheral read result delivered to the DUT, keyed by `(retire_idx or sequence_id, device, address, size, value, fault)`.
   - For CLINT, the RTL CLINT should tick every DUT cycle; when the DUT reads `mtime/mtimeh`, the observed value is recorded.
   - For UART RX or other input devices, the byte/status value actually returned to the DUT is recorded.

3. The reference consumes recorded observations instead of generating its own peripheral values:
   - Add a difftest peripheral-input provider to `EmulatorISS`/`Memory`/MMIO plumbing.
   - When REF executes a peripheral load, it asks the provider for the next observation matching the address/size/device.
   - The provider returns the DUT-observed value and fault status; mismatched address/size/order is a difftest failure.
   - Normal RAM loads/stores remain independently executed and compared architecturally.

4. Keep architectural commit comparison commit-driven:
   - `RtlISS::step_inst()` may consume many cycles and many internal AXI transactions before a commit.
   - After the DUT commit is available, run/ref-step the REF instruction using the queued peripheral observations generated while reaching that DUT commit.
   - Compare the resulting `CommitEvent` as today.

5. Required refactor points:
   - Split memory/MMIO devices behind an interface such as `BusDevice` or `PeripheralProvider` so standalone and difftest modes can use different providers.
   - Extend `ISS` or the difftest harness with hooks to begin/end a retire window and drain DUT observations into REF.
   - Move UART output/pass-fail observation out of ad hoc shell behavior and into the same external-device layer where practical.
   - Add targeted tests where the DUT stalls before reading `mtime` and where REF would otherwise see a different value.

This refactor is non-trivial and should be done after the AXI/i-cache baseline is stable, but before relying on CLINT/UART/input-heavy workloads for pipeline correctness or performance validation. Until then, avoid treating timing-sensitive peripheral tests as proof of correct CLINT timing.

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
