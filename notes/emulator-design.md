# Emulator Design Notes

## Purpose

The emulator is the reference model for differential testing of the RISC-V processor core. It must be:

1. **Correct**: strictly follow the RV32E_Zicsr specification as defined in `specs/core.md`.
2. **Observable**: expose enough architectural state and events to compare against RTL.
3. **Scriptable**: run interactively like a shell, but also execute command scripts for automated testing.
4. **Extensible**: share a common interface with an optional Verilator backend.

The Spike adapter is explicitly deferred: first cross-check the emulator against itself, then against RTL, and only add Spike if we need an independent third opinion later.

## Directory Layout

```
emulator/
├── CMakeLists.txt          # build configuration
├── include/
│   └── emulator/
│       ├── common.h        # types, constants, helpers
│       ├── config.h        # runtime configuration (reset vector, CLINT base, ...)
│       ├── iss.h           # abstract ISS interface
│       ├── commit.h        # commit event definition
│       ├── hart.h          # architectural state (regs, pc, csrs)
│       ├── emulator_iss.h  # software interpreter
│       ├── decoder.h       # instruction decoder
│       ├── memory.h        # flat memory model
│       ├── clint.h         # built-in timer model
│       ├── uart.h          # virtual UART
│       ├── shell.h         # interactive/scripted command loop
│       ├── trace.h         # logging infrastructure
│       └── difftest.h      # differential test harness
├── src/
│   ├── config.cpp          # default config and CLI overrides
│   ├── hart.cpp            # GPR/CSR read/write helpers
│   ├── decoder.cpp         # decode table
│   ├── emulator_iss.cpp    # interpreter
│   ├── memory.cpp          # sparse memory backend
│   ├── clint.cpp           # mtime/mtimeh update
│   ├── uart.cpp            # UART I/O
│   ├── shell.cpp           # command parser and execution
│   ├── trace.cpp           # trace formatting
│   ├── difftest.cpp        # compare commit events
│   └── main.cpp            # entry point
└── tests/
    └── ...                 # unit tests for decoder, memory, etc.
```

## Configuration

A `Config` object holds all parameters that must match between the emulator and the RTL:

| Parameter              | Default       | Description                                      |
|------------------------|---------------|--------------------------------------------------|
| `reset_vector`         | `0x20000000`  | PC after reset                                   |
| `clint_base`           | `0x02000000`  | CLINT memory-map base                            |
| `clint_size`           | `0x00010000`  | CLINT memory-map size                            |
| `uart_base`            | `0x10000000`  | Virtual UART base address                        |
| `ram_base`             | `0x20000000`  | Start of default RAM region                      |
| `ram_size`             | `0x00100000`  | Default RAM size (1 MiB)                         |
| `strict_mem`           | `false`       | Treat unmapped data accesses as faults           |
| `commit_timeout_cycles`| `10000`       | Max cycles `RtlISS` waits for a commit           |
| `max_cycles`           | `0`           | Terminate `run` after N cycles (0 = unlimited)   |
| `max_pc_stuck`         | `0`           | Terminate `run` if same PC retires N times       |

The CLI parses flags such as `--reset-vector`, `--clint-base`, `--uart-base`, `--strict-mem`, `--max-cycles`, and `--max-pc-stuck`. Both the emulator and the RTL testbench read the same config file or command line so they stay synchronized.

## Abstract ISS Interface

All execution engines derive from `ISS`:

```cpp
class ISS {
public:
    virtual ~ISS() = default;

    // Reset. Default reset vector matches specs/core.md.
    virtual void reset(uint32_t reset_addr = 0x20000000) = 0;

    // Advance one clock cycle. For the software interpreter this means
    // execute exactly one instruction and increment the cycle counter by 1.
    // For the RTL adapter this ticks Verilator once.
    virtual bool step_cycle() = 0;

    // Advance until one instruction retires (or an exception is taken).
    // Returns true on success and fills `out`.
    // Returns false if the machine halted or no commit appears within a limit.
    virtual bool step_inst(CommitEvent& out) = 0;

    // Architectural state queries.
    virtual uint32_t pc() const = 0;
    virtual uint32_t reg(uint32_t idx) const = 0;
    virtual uint32_t csr(uint32_t addr) const = 0;

    // Memory access used by the shell and tests, not by the interpreter loop.
    virtual uint32_t read_mem(uint32_t addr, uint32_t size) = 0;
    virtual void write_mem(uint32_t addr, uint32_t size, uint32_t data) = 0;

    // Program loading.
    virtual bool load_bin(const std::string& path, uint32_t addr) = 0;
    virtual bool load_elf(const std::string& path) = 0;

    // Checkpointing.
    virtual bool save_checkpoint(const std::string& path) = 0;
    virtual bool load_checkpoint(const std::string& path) = 0;

    // Logging.
    virtual void set_log_level(int level) = 0;
};
```

`CommitEvent` fields:

- `cycle` — cycle count at retirement
- `pc` — address of the retired instruction
- `inst` — raw 32-bit instruction (undefined when `exception` is true)
- `rd` — destination register index (0 if none)
- `rd_value` — value written to `rd`
- `exception` — true if this commit was an exception
- `cause` — `mcause` value when `exception` is true
- `next_pc` — PC after this commit

Why `step_inst()` returns the event instead of a separate `next_commit()` pull: it makes the adapter state machine explicit and avoids the awkward "event already consumed" problem. The RTL adapter internally runs `step_cycle()` until a commit signal is observed, then populates `CommitEvent`.

## Architectural State (`Hart`)

Encapsulate the ISA-visible state in a `Hart` object:

- `uint32_t pc`
- `uint32_t x[16]` (RV32E)
- CSRs: `mstatus`, `mepc`, `mtvec`, `mcause`, plus read-only `mvendorid`/`marchid`
- `bool halted` — set when an `ebreak` test-finish marker is encountered or on an unrecoverable fault.

`Hart` provides:

- `read_reg(idx)`, `write_reg(idx, value)` (x0 is hardwired zero)
- `read_csr(addr)`, `write_csr(addr, value)` with illegal-CSR exception handling
- `take_exception(cause, tval)` — sets `mepc`, `mcause`, jumps to `mtvec`
- `serialize()` / `deserialize()` for checkpoints

This separation lets the RTL adapter build a `Hart` from Verilator signals for comparison without running the interpreter.

## Decoder

### Instruction encoding table

A single table of entries `(mask, match, InstType)` covers all 32-bit encodings. Compressed instructions are not part of RV32E_Zicsr unless explicitly enabled later, so the table only handles 32-bit instructions.

Example entry shape:

```cpp
struct DecodeEntry {
    uint32_t mask;
    uint32_t match;
    InstType type;
};
```

The decoder iterates the table and returns the first matching `InstType`, then extracts fields with helper functions (`rd`, `rs1`, `rs2`, `funct3`, `imm_i`, etc.).

### RV32E instruction coverage

The emulator must implement the following RV32E instructions:

**Integer computational**
- `lui`, `auipc`
- `addi`, `slti`, `sltiu`, `xori`, `ori`, `andi`, `slli`, `srli`, `srai`
- `add`, `sub`, `sll`, `slt`, `sltu`, `xor`, `srl`, `sra`, `or`, `and`

**Control transfer**
- `jal`, `jalr`
- `beq`, `bne`, `blt`, `bge`, `bltu`, `bgeu`

**Load/Store**
- `lb`, `lh`, `lw`, `lbu`, `lhu`
- `sb`, `sh`, `sw`

**Privileged / synchronization**
- `ecall`, `ebreak`, `mret`, `wfi`
- `fence`, `fence.i`

**CSR**
- `csrrw`, `csrrs`, `csrrc`, `csrrwi`, `csrrsi`, `csrrci`

Any instruction outside this set, including all AMO and compressed instructions, is illegal in RV32E and raises an illegal-instruction exception.

### RV32E specifics

- Only `x0`–`x15` are valid.
- Writes to `x0` are ignored; reads return 0.
- Any instruction whose `rd`, `rs1`, or `rs2` field names `x16`–`x31` raises illegal-instruction exception. This includes encodings that would otherwise be legal in RV32I.

## CSR Handling

Implemented CSRs:

| CSR       | Address | Behavior                                              |
|-----------|---------|-------------------------------------------------------|
| mvendorid | 0xF11   | hardcoded 0                                           |
| marchid   | 0xF12   | hardcoded 0                                           |
| mstatus   | 0x300   | MPP hardcoded to M-mode, all other bits 0             |
| mepc      | 0x341   | holds 32-bit addresses, low two bits masked on write  |
| mtvec     | 0x305   | holds 32-bit addresses, low two bits masked on write  |
| mcause    | 0x342   | exception cause; written by exception path            |

Any other CSR access raises an illegal-instruction exception.

CSR instructions (`csrrw`, `csrrs`, `csrrc`, and their immediate variants) must handle the read-before-write even when `rd` is `x0`, because the read can still raise an illegal-instruction exception for unimplemented CSRs.

## Exception Priorities and Causes

Per RISC-V spec, the priority order for the implemented exceptions is:

| Priority | Exception                        | `mcause` |
|----------|----------------------------------|----------|
| 1        | Instruction address misaligned   | 0        |
| 2        | Instruction access fault         | 1        |
| 3        | Illegal instruction              | 2        |
| 4        | Breakpoint                       | 3        |
| 5        | Load address misaligned          | 4        |
| 6        | Load access fault                | 5        |
| 7        | Store/AMO address misaligned     | 6        |
| 8        | Store/AMO access fault           | 7        |
| 9        | Environment call from M-mode     | 11       |

The "AMO" naming is historical; RV32E does not implement atomic instructions, so only store faults use cause 6/7.

The emulator checks fetch alignment and access first, then decodes, then executes. Load/store faults are detected inside the memory access routine.

## Privileged Instructions

- `ecall`: raises an Environment call from M-mode exception (`mcause` = 11). This is available for workloads that use it as a system call; it does **not** terminate the emulator.
- `ebreak`: raises a Breakpoint exception (`mcause` = 3). The test harness treats an `ebreak` commit as the test-finish marker and inspects the UART output or `a0` to determine pass/fail. The emulator itself records the exception normally and sets `Hart::halted`.
- `mret`: returns to `mepc`.
- `wfi`: NOP.
- `fence`: NOP.
- `fence.i`: NOP in the software emulator (the RTL will flush its i-cache).

## Memory Model

### Flat RAM + MMIO

The emulator uses a simple flat-RAM model:

- A single contiguous byte array for the configured RAM region (`ram_base` to `ram_base + ram_size`).
- RAM size defaults to 1 MiB and is capped at 128 MiB, which is the maximum target memory.
- MMIO regions (CLINT and UART) are handled by range checks before the RAM access path.

This matches `specs/emulator.md`: “For the emulator, you can use a single continuous RWX address space as the memory.”

### Why not a sparse page model?

A sparse page model (`unordered_map` or tree of pages) is unnecessary for this project because:

1. The target has at most 128 MiB of memory — small enough to allocate directly.
2. There is no MMU and no need for page-granular protection or lazy allocation.
3. A flat array gives O(1) access with one bounds check and better cache locality than a hash lookup per access.
4. Checkpointing is just `fwrite`/`fread` of the RAM vector.

Sparse storage would only make sense if the emulated address space were much larger than physical host memory or if page-granular fault injection were required. Neither applies here.

### Access rules

- Widths: 1, 2, 4 bytes.
- Address must be aligned to width; otherwise raise the corresponding misaligned fault.
- Little-endian byte order.
- MMIO regions override the RAM path.

### Memory-mapped regions

| Region        | Base                  | Size      | Notes                                      |
|---------------|-----------------------|-----------|--------------------------------------------|
| CLINT         | `clint_base`          | 0x10000   | `mtime` (+0xBFF8), `mtimeh` (+0xBFFC)      |
| UART          | `uart_base`           | 4 bytes   | TX/RX at offset 0, little-endian           |
| RAM           | `ram_base` (default)  | ≤128 MiB  | default reset vector 0x20000000            |

CLINT offsets are fixed relative to `clint_base`. With the default base:
- `mtime`  at `0x0200BFF8`
- `mtimeh` at `0x0200BFFC`

### Access-fault policy

Because the spec says PMP/PMA are unsupported and the address space is open, the emulator default is:

- Instruction fetch outside RAM: instruction access fault.
- Data read outside RAM and outside MMIO regions: returns 0 (open address space).
- Data write outside RAM and outside MMIO regions: ignored (no effect).

A `--strict-mem` option may be added later to turn out-of-range data accesses into load/store access faults for debugging, but the default matches the "open" intent.

### CLINT and cycle counting

The CLINT `mtime`/`mtimeh` registers increment by 1 each cycle. In the software interpreter one `step_cycle()` executes one instruction and increments the cycle counter by 1. This gives the reference a simple 1-IPC timing model.

**Important timing note:** when the RTL core is pipelined, its cycle count for the same dynamic instruction sequence will differ from the reference. Programs that read `mtime` will therefore observe different values. For correctness difftest, avoid workloads that depend on exact `mtime` values. When we need timing-aware tests, the difftest harness can share a single `Clock` object between reference and RTL so both read the same `mtime`. See `notes/difftest-design.md` for the shared-clock design.

### Virtual UART

Minimal byte channel:

- Write to offset 0: byte appended to an output buffer (flushed to stdout or file).
- Read from offset 0: returns the next byte from the configured input sequence, or `0xFF` when empty.
- No status/control registers; blocking reads/writes are sufficient for test programs.
- Input can be set from a file or a hex string via the shell.

## Shell

### Commands

| Command                         | Description                                  |
|---------------------------------|----------------------------------------------|
| `load <file> [addr]`            | load raw binary at address                   |
| `load_elf <file>`               | load ELF and use its segments/entry          |
| `reset [addr]`                  | reset CPU; default reset vector              |
| `step [n]`                      | execute n instructions (default 1)           |
| `run [n]`                       | run until n instructions retire or stop      |
| `run to <addr>`                 | run until PC reaches addr                    |
| `run until uart <string>`       | run until UART output contains string        |
| `run until reg <i> <value>`     | run until register xi equals value           |
| `break <addr>`                  | set PC breakpoint                            |
| `delete-break <addr>`           | remove PC breakpoint                         |
| `clear-breaks`                  | remove all breakpoints                       |
| `list-breaks`                   | show breakpoints                             |
| `print pc`                      | show PC                                      |
| `print reg [i]`                 | show register(s)                             |
| `print csr [addr]`              | show CSR(s)                                  |
| `print mem <addr> <size>`       | show memory bytes                            |
| `dump state`                    | print concise architectural state            |
| `checkpoint save <file>`        | save state to file                           |
| `checkpoint load <file>`        | restore state from file                      |
| `uart input <file|hex>`         | set UART input source                        |
| `uart output <file>`            | set UART output sink                         |
| `log <level>`                   | set log level (0=quiet, 1=inst, 2=bus)       |
| `trace on [filter]`             | enable selective tracing                     |
| `trace off`                     | disable selective tracing                    |
| `last [n]`                      | show last n retired instructions             |
| `exit`                          | quit                                         |

Notes:
- `run` honors global limits set by `--max-cycles` and `--max-pc-stuck`.
- Breakpoints and `run to` stop before the instruction at the target PC executes.
- `trace on` filters are applied on top of the current log level; sub-traces are only printed when the filter allows them.

### Command parsing

- Semicolon-separated commands.
- `#` starts a comment.
- Simple whitespace tokenization; quoted strings are not required for file paths that do not contain spaces.

### Usage modes

Interactive:

```
$ ./emulator
> load firmware.bin 0x20000000
> run
> print reg 10
> exit
```

Scripted:

```
$ ./emulator -f script.txt
```

Single command:

```
$ ./emulator -e "load firmware.bin 0x20000000; run; exit"
```

## Tracing

Log levels:

- `0`: errors and final summary only.
- `1`: instruction retirement stream.
- `2`: register writes, branches, exceptions.
- `3`: memory and bus transactions.
- `4`: decoder details and internal state.

Selective tracing can be enabled at runtime with `trace on <kind>` and disabled with `trace off`. Supported filters are:

- `all` — trace everything (respects the current log level).
- `branches` — control-transfer instructions only.
- `loads` — load instructions only.
- `stores` — store instructions only.
- `exceptions` — exception commits only.
- `reg <i>` — instructions that write register `xi`.
- `pc <low> <high>` — instructions whose PC falls in the range.

When a filter is active, only matching instructions are printed; sub-traces are also restricted to the filter category.

Default trace line format:

```
R=<retire_idx> C=<cycle> PC=<pc> I=<inst> RD=<rd> RV=<value> NPC=<next_pc> EXC=<exc> CAUSE=<cause>
```

`R=<retire_idx>` is a monotonically increasing retirement index. It is the same for both models when running the same workload, so it is the primary key for cross-model trace comparison. `C=<cycle>` is model-specific (instruction count for the emulator, Verilator cycle for RTL) and is kept for debugging but is not compared directly.

Trace output is line-oriented so it can be diffed with RTL trace logs.

## Differential Testing

The difftest harness is described in `notes/difftest-design.md`. The emulator provides the reference side through `EmulatorISS::step_inst()`. The harness itself lives in `emulator/src/difftest.cpp` and is self-tested with two identical `EmulatorISS` instances in `emulator/tests/test_main.cpp`.

Because the software interpreter is 1-IPC, `CommitEvent::cycle` equals the number of retired instructions. The harness compares `pc`, `inst` (when not an exception), `rd`/`rd_value` (when `rd != 0`), `exception`/`cause`, and `next_pc`; it does **not** compare `cycle` directly.

## Checkpoint Format

Binary format for compactness:

- Magic bytes (`AIEM`) and version (4 bytes each).
- RAM size `ram_size` (default 1 MiB, max 128 MiB). The byte vector is sized accordingly.
- Cycle count (8 bytes).
- PC (4 bytes).
- GPR array (16 x 4 bytes).
- CSRs (6 x 4 bytes: mvendorid, marchid, mstatus, mepc, mtvec, mcause).
- RAM contents (`ram_size` bytes).
- UART input/output state.

A separate `--dump-checkpoint` command can render the binary as human-readable text for debugging.

## Testing Strategy

1. Decoder unit tests: feed known encodings and verify decoded type + fields.
2. ALU tests: verify each arithmetic/logical instruction result and flag semantics.
3. Memory tests: aligned access, unaligned faults, byte ordering, MMIO reads.
4. CSR tests: read/write implemented CSRs, illegal CSR access exception.
5. Exception tests: misaligned fetch, `ebreak` (test-finish marker / breakpoint), `ecall`, `mret`, illegal instruction.
6. End-to-end: compile a small C/assembly program, run to UART pass marker.
7. Difftest self-test: run two `EmulatorISS` instances and confirm they match.

## Build and Tooling

- C++17, CMake >= 3.16.
- GoogleTest fetched by CMake or vendored under `emulator/third_party/`.
- Compiler warnings: `-Wall -Wextra -Werror` in release/test builds.
- A top-level `Makefile` or `justfile` provides convenience targets:
  - `make build` — configure and build the emulator.
  - `make test` — run unit tests.
  - `make run-<test>` — run a workload binary.

## Open Questions / Deferred Decisions

- ELF loader: start with raw binary + explicit address; add ELF loading once C workloads arrive.
- Spike adapter: deferred until RTL difftest is stable.
- AXI testbench memory: designed in `notes/difftest-design.md`.
