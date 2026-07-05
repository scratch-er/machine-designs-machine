# Core Design Notes

## Purpose

This note records the processor core architecture. The initial target is a **correct, simple, single-cycle core** that fully implements `specs/core.md`. Once it passes differential testing, we will iterate toward a pipelined design for better PPA.

## ISA and Privilege

- RV32E_Zicsr, M-mode only.
- No compressed instructions, no M extension, no A extension, no F/D extension.
- No virtual memory, no PMP/PMA, no interrupts.

## Configuration

The same `Config` object used by the emulator drives the RTL parameters:

| Parameter      | Default       | RTL parameter          |
|----------------|---------------|------------------------|
| reset_vector   | `0x20000000`  | `RESET_VECTOR`         |
| clint_base     | `0x02000000`  | `CLINT_BASE`           |
| clint_size     | `0x00010000`  | `CLINT_SIZE`           |
| uart_base      | `0x10000000`  | n/a (testbench only)   |

## Microarchitecture Strategy

### Phase 1: Single-cycle baseline

A single-cycle core is the fastest path to a working DUT:

- One instruction completes per cycle.
- No pipeline hazards, no forwarding, no stall logic.
- Fetch, decode, execute, memory, writeback all happen in one cycle.
- AXI4 master is combinational on the request side; response must arrive in the same cycle (testbench memory supports this for the baseline).

**Pros:** easy to get right, trivial difftest, clear RTL reference.
**Cons:** low clock frequency, combinational AXI is unrealistic, not optimizable for PPA.

### Phase 2: Pipelined core

After the single-cycle core passes difftest, introduce a multi-cycle or pipelined design:

- 3-stage or 5-stage pipeline.
- In-order execution, in-order retirement.
- AXI4 master with proper request/response timing.
- Instruction cache with burst fills.
- Precise exceptions; `mepc` points to the faulting instruction.
- `fence.i` flushes the instruction cache.

The pipeline will be verified by the same commit-driven difftest harness; only the RTL adapter changes.

## Top-Level Ports

Use the exact port list from `specs/core.md`:

- `clock`, `reset` (active high).
- `io_interrupt` — reserved input; do not use.
- AXI4 master (`io_master_*`).
- AXI4 slave (`io_slave_*`) — reserved; outputs hardcoded to 0, inputs ignored.

## Instruction Fetch

### Without cache (single-cycle baseline)

- PC drives `araddr` combinationaly.
- Testbench memory returns `rdata` and `rvalid` in the same cycle.
- No bursts.

### With cache (pipelined)

- Direct-mapped, 16-byte line, 2 lines total = 8 instructions when compressed is absent, but for 32-bit instructions each line holds 4 instructions.
- Capacity: 8 instructions = 32 bytes = 2 lines of 16 bytes.
- Tag + valid + data per line.
- All instruction addresses are cacheable (per spec).
- On miss, issue an AXI4 read burst of 4 beats (16 bytes / 4 bytes per beat).
- AMAT counters: hit count, miss count, total cycles. AMAT = hit_time + miss_rate * miss_penalty.

## Execute Stage

- 32-bit ALU supporting add/sub, shifts, logical, compares.
- Branch comparator.
- PC generation: PC+4, branch target, jump target, `mtvec`, `mepc`.

## Memory Stage

- AXI4 write and read transactions for load/store.
- Address alignment check; raise misaligned exception if violated.
- Byte/half/word sign/zero extension for loads.
- `fence` as NOP; `fence.i` flushes i-cache (in pipelined core).

## CSR / Exception Unit

- CSRs implemented in a dedicated block.
- Exception cause encoded per RISC-V spec (see `notes/emulator-design.md`).
- `mret` restores PC from `mepc`.
- `wfi` treated as NOP.

## CLINT

- Built into the core, memory-mapped at `clint_base`.
- `mtime` increments every cycle.
- `mtimeh` is the upper 32 bits.
- `mtimecmp`, `mtimecmph`, `msip` ignored.

## Difftest Interface

The core must expose commit signals for the RTL adapter:

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

For the single-cycle baseline, `commit_valid` is asserted every cycle that an instruction completes. For the pipelined core, it is asserted at retirement.

## Development Order

1. Single-cycle datapath without AXI (internal memory).
2. Add AXI4 master and connect to testbench memory.
3. Add CLINT, CSR, exception handling.
4. Run difftest against emulator.
5. Add instruction cache and pipeline stages.
6. Re-run difftest and optimize.

## PPA Targets

TBD after M5. Initial metrics to collect:

- Cycles per instruction (CPI) on a small benchmark.
- Critical path / Fmax after synthesis.
- Area (LUT/FF count).
- I-cache AMAT from performance counters.

## Open Questions

- Chisel vs Verilog: start with Verilog for direct control; revisit Chisel if the pipeline becomes complex.
- Pipeline depth: decide between 3-stage and 5-stage after the single-cycle baseline works.
- Whether to implement branch prediction or simple static-taken/not-taken initially.
