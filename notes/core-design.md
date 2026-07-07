# Core Design Notes

## Purpose

This note records the processor-core architecture for the RV32E_Zicsr NPC core. The plan is intentionally written *before* the first line of RTL so that the data path, control signals, hazard logic, exception model, and AXI interfaces are agreed on before implementation begins.

The design proceeds in two phases:

1. **Single-cycle baseline** — validates the ISA semantics against the emulator with the smallest possible RTL.
2. **Five-stage pipeline with I-cache** — meets the full `specs/core.md` requirements and provides the PPA target.

## ISA and Privilege Reminders

- RV32E_Zicsr, M-mode only.
- No compressed instructions, no M/A/F/D extension.
- No virtual memory, no PMP/PMA, no interrupts.
- `x0`–`x15` only; any specifier in `x16`–`x31` is illegal.
- All memory accesses are naturally aligned; misaligned accesses trap.
- External bus: 32-bit AXI4.

## Configuration Parameters

| Parameter      | Default       | Verilog parameter |
|----------------|---------------|-------------------|
| reset vector   | `0x20000000`  | `RESET_VECTOR`    |
| CLINT base     | `0x02000000`  | `CLINT_BASE`      |
| CLINT size     | `0x00010000`  | `CLINT_SIZE`      |

These are passed as `parameter`s to the top module `npc_core` so synthesis and simulation can override them without editing the RTL.

## Phase 1 — Single-Cycle Baseline

The baseline exists only to get a working DUT for difftest. It intentionally omits the instruction cache and uses a combinational AXI4 read path. The testbench memory returns `rvalid`/`rdata` in the same cycle as `arvalid`/`araddr`.

### Datapath

```
PC_reg ──┬──► AXI AR ──► rdata ──► inst
         │
         └──► PC+4 adder

inst ──► Decoder ──► {
    alu_op, alu_src_a/b, imm_sel, reg_write,
    mem_read, mem_write, mem_size, mem_sext,
    branch_type, jump, jump_reg, csr_op, csr_src,
    ecall, ebreak, mret, fence_i, illegal
}

RegFile[rs1] ──┐
               ├──► ALU ──► result
RegFile[rs2] ──┘      ▲
PC              ──────┤ (alu_src_a)
imm             ──────┘ (alu_src_b)

result ──► {
    register writeback data,
    load/store effective address,
    branch/jump target
}

Load data ──► Load aligner/sign-extender ──► writeback
Store data ──► Store aligner ──► AXI W channel

CSR file ──► csr_old ──► rd writeback
            └─► csr_new ──► CSR write

PC next mux: PC+4 < branch_target < jump_target < mepc(mret) < mtvec(exception)
```

### Control Signals

All signals are produced combinationally by the decoder.

| Signal        | Width | Meaning |
|---------------|-------|---------|
| `alu_op`      | 4     | `ADD`, `SUB`, `SLT`, `SLTU`, `XOR`, `OR`, `AND`, `SLL`, `SRL`, `SRA`. |
| `alu_src_a`   | 2     | `00`=rs1, `01`=PC, `10`=zero. |
| `alu_src_b`   | 1     | `0`=rs2, `1`=imm. |
| `imm_sel`     | 3     | `I`, `S`, `B`, `U`, `J`, `Z` (CSR uimm). |
| `reg_write`   | 1     | Write `rd_value` to register file. |
| `mem_read`    | 1     | Perform a data load. |
| `mem_write`   | 1     | Perform a data store. |
| `mem_size`    | 2     | `00`=byte, `01`=half, `10`=word. |
| `mem_sext`    | 1     | Sign-extend load data; `0`=zero-extend. |
| `branch_type` | 3     | `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU`, or none. |
| `jump`        | 1     | Unconditional jump (`jal`/`jalr`). |
| `jump_reg`    | 1     | `1`=target is `rs1+imm` (`jalr`), `0`=target is `pc+imm` (`jal`). |
| `csr_op`      | 3     | `CSRRW`, `CSRRS`, `CSRRC`, `CSRRWI`, `CSRRSI`, `CSRRCI`, or none. |
| `csr_src`     | 1     | `0`=rs1, `1`=uimm[4:0]. |
| `ecall`       | 1     | Raise environment-call exception. |
| `ebreak`      | 1     | Raise breakpoint exception and halt the core. |
| `mret`        | 1     | Return from trap (`pc = mepc`). |
| `fence_i`     | 1     | Flush I-cache (NOP in the baseline). |
| `illegal`     | 1     | Illegal instruction: take trap with `mcause = 2`. |

### ALU

One combinational ALU. Operations:

- `ADD`/`SUB`: 32-bit add/subtract. `SUB` selected by control, implemented as `op1 + ~op2 + 1`.
- `SLL`/`SRL`/`SRA`: 32-bit barrel shifter, amount from `op2[4:0]` or `imm[4:0]`.
- `SLT`/`SLTU`: signed/unsigned less-than, result `0` or `1`.
- `XOR`/`OR`/`AND`: bitwise.

A separate branch comparator checks `BEQ/BNE/BLT/BGE/BLTU/BGEU` using signed/unsigned comparison of `rs1` and `rs2`.

### Immediate Generator

Produces all six immediates and selects one per `imm_sel`:

| Type | Value |
|------|-------|
| I | `sext(inst[31:20])` |
| S | `sext({inst[31:25], inst[11:7]})` |
| B | `sext({inst[31], inst[7], inst[30:25], inst[11:8], 1'b0})` |
| U | `{inst[31:12], 12'b0}` |
| J | `sext({inst[31], inst[19:12], inst[20], inst[30:21], 1'b0})` |
| Z | `{27'b0, inst[19:15]}` |

### Register File

- 16 × 32-bit.
- Asynchronous read, synchronous write.
- `x0` hardwired to zero; writes to `x0` are dropped.

### CSR File

Implemented CSRs:

| CSR       | Address | Access | Behaviour |
|-----------|---------|--------|-----------|
| `mvendorid` | `0xF11` | RO | `0`. |
| `marchid`   | `0xF12` | RO | `0`. |
| `mstatus`   | `0x300` | RW | Reads `0x00001800` (MPP=M); writes ignored but accepted. |
| `mtvec`     | `0x305` | RW | Low 2 bits hardwired to `0`. |
| `mepc`      | `0x341` | RW | Low 2 bits hardwired to `0`. |
| `mcause`    | `0x342` | RW | Written by exception logic; software writable. |

CSR instruction logic:

1. Read `csr_old` from the CSR file.
2. Form source operand: `rs1` or zero-extended `uimm[4:0]`.
3. Compute `csr_new`:
   - `CSRRW*`/`CSRRWI*`: `csr_new = source`.
   - `CSRRS*`/`CSRRSI*`: `csr_new = csr_old | source`.
   - `CSRRC*`/`CSRRCI*`: `csr_new = csr_old & ~source`.
4. Suppress the CSR write for `CSRRS/C` when `rs1=0`, and for `CSRRSI/CI` when `uimm=0`.
5. Suppress the `rd` write for `CSRRW*` when `rd=0`.
6. Write `csr_new` back if the CSR address is implemented; otherwise take an illegal-instruction exception.

### Exception Model

Exception causes follow `specs/core.md`:

| Cause | Value | Source |
|-------|-------|--------|
| Inst address misaligned | 0 | Jump/branch target not 4-byte aligned. |
| Inst access fault       | 1 | AXI SLVERR/DECERR on fetch. |
| Illegal instruction     | 2 | Unknown encoding, reserved register, unimplemented CSR. |
| Breakpoint              | 3 | `ebreak`. |
| Load address misaligned | 4 | Load address not naturally aligned. |
| Load access fault       | 5 | AXI SLVERR/DECERR on load. |
| Store address misaligned| 6 | Store address not naturally aligned. |
| Store access fault      | 7 | AXI SLVERR/DECERR on store. |
| Environment call M-mode | 11| `ecall`. |

On any exception:

1. `mepc = current_pc`.
2. `mcause = cause`.
3. `pc = mtvec`.
4. The instruction does **not** perform its normal register/CSR/memory writes.

`ebreak` additionally sets an internal `halted` flag so the core stops fetching; this matches the emulator's test-finish semantics.

### Single-Cycle AXI Master

The baseline master is combinational on the request side:

- Instruction fetch: `arvalid = 1`, `araddr = pc`, `arsize = 2`, `arlen = 0`. Testbench responds with `rvalid` in the same cycle.
- Load: `arvalid = 1`, `araddr = alu_result`, `arsize = mem_size`, `arlen = 0`.
- Store: `awvalid = 1`, `wvalid = 1`, `awaddr = alu_result`, `awsize = mem_size`, `awlen = 0`, `wdata`/`wstrb` from store aligner.

Both fetch and data paths cannot be active simultaneously on the same AXI ID, so the baseline serializes them naturally: instruction fetch is performed, then the same cycle may issue a load/store if the instruction is a memory instruction.

`SLVERR`/`DECERR` responses raise the corresponding access-fault exception.

### CLINT

- 64-bit `mtime` counter increments every clock cycle.
- Memory-mapped at `CLINT_BASE + 0xBFF8` (`mtime`) and `+0xBFFC` (`mtimeh`).
- `mtimecmp`/`mtimecmph`/`msip` accesses are ignored (read undefined, write no effect).
- Loads see the value *before* the current-cycle increment.

## Phase 2 — Five-Stage Pipelined Core

The pipelined core implements the full specification, including the flip-flop instruction cache, proper AXI4 timing, and the same exception model.

### Pipeline Stages

1. **IF** — PC update, I-cache lookup, AXI burst fill on miss.
2. **ID** — Decode, register-file read, immediate generation, hazard detection.
3. **EX** — ALU, branch/jump target and condition, CSR source selection.
4. **MEM** — Data memory/AXI access, CSR read/write, exception commit.
5. **WB** — Register-file writeback and commit-interface assertion.

A five-stage pipeline keeps the combinational paths short and maps cleanly to the classic forwarding/hazard structure, which is easier to maintain than a merged 3-stage design.

### Pipeline Registers

| Register set | Fields carried |
|--------------|----------------|
| `IF/ID`      | `pc`, `inst`, `valid`, `exception`, `cause`. |
| `ID/EX`      | `pc`, `inst`, `rs1_data`, `rs2_data`, `imm`, `rd`, `rs1`, `rs2`, `csr_addr`, all control signals, `valid`. |
| `EX/MEM`     | `pc`, `inst`, `alu_result`, `rs2_data`, `rd`, `csr_addr`, `csr_old`, `branch_target`, `branch_taken`, `jump`, `mret`, all memory/CSR/control signals, `valid`, `exception`, `cause`. |
| `MEM/WB`     | `pc`, `inst`, `alu_result`, `mem_data`, `rd`, `csr_addr`, `csr_old`, `reg_write`, `csr_op`, `mem_read`, `mem_sext`, `mem_size`, `valid`, `exception`, `cause`, `next_pc`. |

`pc` is carried through the pipeline so that exceptions can set `mepc` to the faulting instruction's PC.

### Control Flow and Hazards

#### Branch / Jump

- Static **not-taken** prediction.
- Branch condition and target computed in EX.
- On taken branch or jump: flush IF and ID, update PC with target.
- Branch mispredict penalty = 2 cycles.

#### Load-Use Hazard

- A load in MEM and a dependent instruction in EX causes a one-cycle stall.
- Stall logic: if `EX.rs1 == MEM.rd || EX.rs2 == MEM.rd` and `MEM.mem_read`, stall IF/ID and insert a bubble into EX.
- Forwarding from WB to EX removes the stall for ALU results; load data is still one cycle late.

#### Forwarding

- **EX → EX**: ALU result from the instruction currently in MEM.
- **WB → EX**: ALU result, load data, or CSR read data from the instruction in WB.
- **MEM/WB → ID** (CSR only): if ID reads a CSR that MEM is writing, forward the write data to avoid a stall. Optional; a CSR read-after-write stall is acceptable because CSR instructions are rare.

#### Exception Preciseness

Exceptions are handled in order by stage priority:

1. **MEM** exception (oldest instruction) has highest priority.
2. **EX** exception (e.g., misaligned jump/branch target).
3. **ID** exception (illegal instruction, reserved register).
4. **IF** exception (instruction-fetch fault or misaligned PC).

When an exception is taken:

- `mepc` = PC of the offending instruction.
- `mcause` = cause.
- `pc` = `mtvec`.
- Flush IF, ID, and EX. The MEM instruction becomes a NOP and is marked as the excepting instruction on the commit interface.

### Instruction Cache

Parameters from `specs/core.md`:

- Capacity: 8 instructions = 32 bytes.
- Line size: 16 bytes (four 32-bit instructions).
- Associativity: 1 (direct mapped).
- Storage: flip-flops, not block RAM.
- All instruction-fetchable addresses are assumed cacheable.

Address breakdown for a 32-bit PC:

| Field       | Bits   | Purpose |
|-------------|--------|---------|
| byte offset | `[1:0]` | Always `00` for 32-bit instructions. |
| word offset | `[3:2]` | Selects one of four words in a line. |
| line index  | `[4]`   | Selects one of two cache lines. |
| tag         | `[31:5]`| Stored per line and compared on lookup. |

Each cache line stores:

- `valid`
- `tag[26:0]`
- `data[127:0]`

On every IF cycle:

1. Select line with `pc[4]`.
2. Check `valid` and `tag == pc[31:5]`.
3. If hit: output `data[{pc[3:2], 5'b0} +: 32]` and assert `icache_hit`.
4. If miss: assert `icache_miss`, stall IF, and request an AXI4 read burst.

Cache fill:

- `ARADDR = {pc[31:5], 5'b0}` (line-aligned).
- `ARLEN = 3` (four beats).
- `ARSIZE = 2` (four bytes per beat).
- `ARBURST = 2'b01` (INCR).
- As each `RVALID` beat arrives, write it to the appropriate word of the selected line.
- On `RLAST`, set `valid` and `tag`.
- If the branch target changes while a fill is in flight, the fill completes but the fetched line is simply not used unless the new PC falls inside it.

`fence.i` clears all `valid` bits.

### AXI4 Master

The core exposes one AXI4 master port shared by IF (instruction fills) and MEM (loads/stores). The master accepts one transaction at a time and arbitrates between IF and MEM.

Arbitration:

- MEM requests have priority over IF requests. This prevents a data access from being indefinitely delayed by instruction misses.
- Once a transaction starts, it runs to completion; no preemption.

State machine states:

- `IDLE`
- `IF_FILL_ADDR` / `IF_FILL_DATA` — read burst for I-cache.
- `MEM_R_ADDR` / `MEM_R_DATA` — single-beat read for load.
- `MEM_W_ADDR` / `MEM_W_DATA` / `MEM_W_RESP` — single-beat write for store.

Load/store request signals come from EX/MEM; the address, size, and write data are latched when the transaction starts.

### Data Memory Alignment and Byte Enables

**Load aligner** (in MEM/WB):

- Address low bits determine which byte/half of the 32-bit `rdata` to return.
- `lb`/`lbu`: select byte, then sign/zero extend.
- `lh`/`lhu`: select half, then sign/zero extend.
- `lw`: pass through.

**Store aligner** (in EX/MEM):

- `sb`: shift `rs2[7:0]` to the correct byte lane; `wstrb` has one bit set.
- `sh`: shift `rs2[15:0]` to the correct half lane; `wstrb` has two bits set.
- `sw`: `wdata = rs2`, `wstrb = 4'b1111`.

Misaligned addresses raise `load address misaligned` or `store address misaligned` before any AXI transaction is issued.

### Commit Interface

The core exposes the signals required by `notes/difftest-design.md`:

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

- `commit_valid` is asserted in WB when an instruction retires.
- On exception, `commit_exception` is asserted, `commit_cause` is the RISC-V cause code, and `commit_next_pc` is `mtvec`.
- `commit_rd_value` is the value that will be written to `commit_rd` (meaningful only when `commit_rd != 0` and not an exception).

### Performance Counters for I-Cache AMAT

Counters are readable from the testbench or via a simple debug interface:

- `icache_access` — number of I-cache lookups.
- `icache_hit` — number of hits.
- `icache_miss` — number of misses started.
- `icache_miss_cycles` — total cycles from `ARVALID` to `RLAST` for all misses.

AMAT = `1 + (icache_miss / icache_access) * (icache_miss_cycles / icache_miss)`.

## Verilator Simulation and Debug Infrastructure

### Why DPI-C for Memory

Using combinational Verilog arrays for RAM has several drawbacks in a Verilator-based flow:

- Large arrays slow down Verilator compilation and simulation.
- Memory content is hard to inspect, dump, or patch from the emulator shell.
- Loading a program requires either `$readmemh` or synthesizing initial values.

Instead, the RTL uses a **DPI-C memory interface**. The emulator creates and owns the `Memory` backend, then registers it with the DPI-C layer. Verilog calls DPI functions for instruction fetch, load, and store. This keeps the emulator's RAM model authoritative and lets the RTL reuse the same loader/checkpoint/debug infrastructure.

### DPI-C Function Contract

The memory DPI functions are declared in `core/rtl/npc_memory_dpi.v` and implemented in `core/dpi/npc_memory_dpi.cpp`. They are **pure** for reads and **void** for writes so Verilator can schedule them safely:

```verilog
import "DPI-C" function int unsigned npc_dpi_mem_read(input int unsigned addr, input int unsigned size);
import "DPI-C" function void npc_dpi_mem_write(input int unsigned addr, input int unsigned size, input int unsigned data);
```

Supported `size` values are `1`, `2`, and `4`. The functions return/take little-endian data.

To avoid side effects in combinational logic:

- **Reads** (`npc_dpi_mem_read`) are pure: they only inspect the C++ memory array and return a value. It is safe to call them from combinational `always @(*)` blocks for instruction fetch and load data.
- **Writes** (`npc_dpi_mem_write`) are called only from clocked `always @(posedge clock)` blocks. The core presents a stable write request (address, size, data, byte-enable) on the clock edge; the DPI function commits it.

For the single-cycle baseline, instruction fetch is combinational: `npc_dpi_mem_read(pc, 4)` drives `inst`. This is acceptable because the function has no side effects. The store is committed on the positive clock edge after the address and data are stable.

### Debug Ports (`ifdef VERILATOR`)

The top module exposes the standard commit interface required by difftest. Extra debug-only signals are wrapped in `` `ifdef VERILATOR `` so synthesis tools see a clean port list:

```verilog
`ifdef VERILATOR
output        debug_commit_valid,
output [31:0] debug_commit_pc,
output [31:0] debug_commit_inst,
output [4:0]  debug_commit_rd,
output [31:0] debug_commit_rd_value,
output        debug_commit_exception,
output [31:0] debug_commit_cause,
output [31:0] debug_commit_next_pc,
output        debug_reg_wen,
output [3:0]  debug_reg_waddr,
output [31:0] debug_reg_wdata,
output [31:0] debug_pc,
output [31:0] debug_inst,
output        debug_stall,
output        debug_flush,
`endif
```

These signals are used by the emulator adapter for tracing and logging. They do not affect the synthesizable design.

### Emulator Verilog Adapter (`RtlISS`)

`emulator/src/rtl_iss.cpp` implements the same `ISS` interface as `EmulatorISS` so the existing shell, difftest harness, and workload loader work without change:

- `reset(addr)` — applies reset for several cycles, sets the reset vector, and resets the DPI memory model.
- `step_cycle()` — advances Verilator by one clock edge and ticks the AXI/memory model.
- `step_inst(ev)` — calls `step_cycle()` until `commit_valid` is asserted or a timeout expires.
- `pc()`, `reg(i)`, `csr(addr)` — read debug ports and the Verilated register file/CSR file through DPI accessors.
- `read_mem(addr, size)` / `write_mem(addr, size, data)` — delegate to the DPI memory model.
- `load_bin(path, addr)` / `load_elf(path)` — use the emulator's existing loaders; the data is written into the DPI memory model.
- `save_checkpoint()` / `load_checkpoint()` — **deferred**. The RTL architectural state can be read back through debug ports, but full checkpoint restore is complex. For now, debugging relies on trace logging and reproducible script runs.

The adapter supports VCD tracing when configured. The trace file is written by Verilator's `--trace` mechanism and is controlled from the C++ testbench, not from the shell.

### Reusing Emulator Commands

Because `RtlISS` implements `ISS`, the existing shell commands (`step`, `run`, `print`, `break`, `load_elf`, `dump state`, etc.) work unchanged with the RTL. Commands that are emulator-specific (`uart`, `trace`, `last`, `checkpoint`) are detected by `Shell::as_emu()` and reported as unsupported on the RTL adapter.

### Difftest Integration

The difftest harness (`emulator/src/difftest.cpp`) compares `CommitEvent`s from `EmulatorISS` and `RtlISS`. The RTL adapter obtains each event from the commit interface exposed by `npc_core`. Because `step_inst()` waits for `commit_valid`, cycle-level timing differences between the interpreter and the RTL are hidden from the harness.

### Build Flow

A `core/Makefile` (or CMake target) drives Verilator:

```
verilator --cc --exe --build --trace \
  -Icore/rtl \
  core/rtl/npc_core.v \
  core/dpi/npc_memory_dpi.cpp \
  emulator/src/rtl_iss.cpp \
  ...
```

The DPI C++ files are compiled into the Verilated executable. The emulator's `Memory`, `Config`, and ELF loader are linked in as a static library.

## Module Hierarchy

```
core/
├── rtl/
│   ├── npc_defines.vh          # parameters, opcodes, causes
│   ├── npc_core.v              # top module + AXI slave tie-offs + debug ports
│   ├── npc_single_cycle.v      # single-cycle datapath (Phase 1)
│   ├── npc_regfile.v           # 16-entry register file
│   ├── npc_csr_file.v          # CSR read/write
│   ├── npc_clint.v             # mtime/mtimeh
│   ├── npc_memory_dpi.v        # DPI-C memory wrapper
│   ├── npc_if_stage.v          # PC, I-cache, fetch FSM (Phase 2)
│   ├── npc_id_stage.v          # decoder, regfile read, hazard unit
│   ├── npc_ex_stage.v          # ALU, branch/jump resolver
│   ├── npc_mem_stage.v         # data memory, CSR file, CLINT, exceptions
│   ├── npc_wb_stage.v          # writeback and commit interface
│   ├── npc_icache.v            # flip-flop instruction cache
│   └── npc_axi_master.v        # shared AXI4 master arbiter
├── dpi/
│   └── npc_memory_dpi.cpp      # C++ implementation of DPI memory functions
└── tb/
    └── ...                     # Verilator testbench (M5)
```

For Phase 1 the single-cycle core is implemented as `npc_core.v` + `npc_single_cycle.v` + `npc_regfile.v` + `npc_csr_file.v` + `npc_clint.v` + `npc_memory_dpi.v`. The DPI-C layer uses the emulator's `Memory` backend. Stage modules and the AXI master are introduced in Phase 2.

The emulator adapter lives in `emulator/`:

```
emulator/
├── include/emulator/rtl_iss.h  # Verilator wrapper header
├── src/rtl_iss.cpp             # Verilator wrapper implementation
└── ...                         # existing emulator files
```

## PPA Considerations

| Concern     | Decision | Rationale |
|-------------|----------|-----------|
| **Performance** | 5-stage pipeline, static not-taken branch prediction, simple forwarding. | Short critical path; CPI close to 1 for ALU-heavy code, ~1.2–1.5 for control/memory-heavy code. |
| **Area** | Tiny I-cache (32 B), 16-entry register file, no multiplier/divider, no branch predictor table. | The design is dominated by control and the AXI master; the datapath is small. |
| **Power** | Flip-flop I-cache avoids block-RAM leakage in this process node; clock gating deferred. | The tiny cache reduces memory traffic and thus dynamic power. |
| **Timing** | Registers on all AXI handshake outputs; no combinational loops; registered AXI responses. | Easier timing closure on FPGA/ASIC flows. |

The single-cycle baseline is expected to have poor Fmax; it exists only for correctness bring-up.

## Development Order

1. Create `core/rtl/`, `core/dpi/`, and `npc_defines.vh`.
2. Implement the DPI-C memory wrapper and C++ backend.
3. Implement the emulator `RtlISS` adapter that drives Verilator.
4. Implement the single-cycle `npc_core` using DPI-C memory, with debug/commit ports.
5. Build with Verilator and sanity-test the simulation flow.
6. Run difftest against `EmulatorISS` on the existing workload suite.
7. Refactor into the five-stage pipeline:
   - `npc_if_stage` + `npc_icache`.
   - `npc_id_stage` + `npc_regfile`.
   - `npc_ex_stage`.
   - `npc_mem_stage` + `npc_csr_file` + `npc_clint`.
   - `npc_wb_stage`.
   - `npc_axi_master` arbiter.
8. Add hazard/forwarding logic and re-run difftest.
9. Add I-cache AMAT counters and benchmark.

## Open Questions / Decisions

- **Language**: Verilog with SystemVerilog DPI-C imports, for direct control and easy Verilator integration. Revisit Chisel only if the pipeline becomes unmanageable.
- **Pipeline depth**: 5-stage chosen over 3-stage for timing; the extra forwarding is standard and well documented.
- **Branch prediction**: static not-taken initially. A simple static backward-taken predictor can be added later for loop benchmarks if CPI measurement justifies it.
- **mtime semantics**: the RTL increments `mtime` every clock cycle. Difftest will avoid programs that load `mtime` for correctness verification, per `notes/difftest-design.md`.
- **Checkpointing**: full RTL checkpoint save/restore is deferred. The adapter can read architectural state through debug ports, but restoring a checkpoint is not yet supported. Rely on reproducible scripts and trace buffers for debugging.
- **Memory model**: DPI-C memory is authoritative. Combinational reads are pure; writes happen on clock edges only. This avoids Verilator's "no side effects in combinational logic" issue.
