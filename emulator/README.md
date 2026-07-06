# AI-SoC Reference Emulator

A cycle-functional software model of the RV32E_Zicsr RISC-V core described in `specs/core.md`. It is intended primarily as the golden reference for differential testing of the processor core, but it can also run small RISC-V programs interactively or from scripts.

## Supported ISA

- RV32E base integer instruction set (registers `x0`–`x15` only).
- Zicsr CSR instructions.
- M-mode privileged instructions: `ecall`, `ebreak`, `mret`, `wfi`.
- Memory ordering: `fence` (nop), `fence.i` (nop).
- No compressed instructions, no interrupts, no virtual memory, no PMP/PMA.

See `notes/instruction-list.md` for the complete instruction list and encodings.

## Directory Layout

```
emulator/
├── CMakeLists.txt          # Build configuration
├── include/emulator/       # Public headers
├── src/                    # Implementation
├── tests/                  # Unit tests
└── README.md               # This file
```

Key source files:

| File | Responsibility |
|------|----------------|
| `src/decoder.cpp` | Decode 32-bit RV32E_Zicsr instructions into `DecodedInst`. |
| `src/emulator_iss.cpp` | Interpreter loop, execute one instruction per cycle. |
| `src/hart.cpp` | Architectural state: GPRs, CSRs, exception entry. |
| `src/memory.cpp` | Flat RAM model with aligned access rules. |
| `src/clint.cpp` | Built-in timer (`mtime`/`mtimeh`). |
| `src/uart.cpp` | Virtual UART byte channel. |
| `src/shell.cpp` | Interactive/scripted command loop. |
| `src/trace.cpp` | Retirement and bus tracing, plus selective trace filters. |
| `src/difftest.cpp` | Instruction-by-instruction harness for reference/DUT comparison. |

## Building

Requirements:

- CMake >= 3.16
- C++17 compiler (Apple Clang, GCC, MSVC)

```bash
cmake -S emulator -B emulator/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build emulator/build -j
ctest --test-dir emulator/build
```

The executable is `emulator/build/emulator`.

## Running

### Interactive mode

```bash
./emulator/build/emulator
> load workloads/asm/uart_pass/pass.bin 0x20000000
> run
> print reg
> exit
```

### Script mode

```bash
./emulator/build/emulator -f script.txt
```

### Single command

```bash
./emulator/build/emulator -e "load firmware.bin 0x20000000; run; exit"
```

## Command Reference

Commands can be separated by semicolons. `#` starts a comment.

| Command | Description |
|---------|-------------|
| `load <file> [addr]` | Load raw binary at `addr` (default `0x20000000`). |
| `load_elf <file>` | Load a 32-bit RISC-V ELF and set the reset vector to its entry point. |
| `reset [addr]` | Reset CPU to `addr`. |
| `step [n]` | Execute `n` instructions (default 1). |
| `run [n]` | Run until halt or `n` instructions retire. |
| `run to <addr>` | Run until PC reaches `addr`. |
| `run until uart <string>` | Run until UART output contains `string`. |
| `run until reg <i> <value>` | Run until register `xi` equals `value`. |
| `break <addr>` | Set a PC breakpoint. |
| `delete-break <addr>` | Remove a PC breakpoint. |
| `clear-breaks` | Remove all breakpoints. |
| `list-breaks` | Show breakpoints. |
| `print pc` | Show PC. |
| `print reg [i]` | Show all registers or register `i`. |
| `print csr <addr>` | Show CSR at hex address. |
| `print mem <addr> <size>` | Dump `size` bytes starting at `addr`. |
| `dump state` | Print concise architectural state. |
| `checkpoint save <file>` | Save architectural state to file. |
| `checkpoint load <file>` | Restore state from file. |
| `uart input <file\|hex>` | Set UART input source. |
| `uart output <file>` | Redirect UART output to file. |
| `log <level>` | Set log level (0=quiet, 1=inst, 2=reg/exc, 3=mem, 4=decoder). |
| `trace on [filter]` | Enable selective tracing (`all`, `branches`, `loads`, `stores`, `exceptions`, `reg i`, `pc low high`). |
| `trace off` | Disable selective tracing. |
| `last [n]` | Show the last `n` retired instructions. |
| `exit`, `quit` | Leave the shell. |

## Configuration Options

| Flag | Default | Meaning |
|------|---------|---------|
| `--reset-vector <hex>` | `0x20000000` | Reset PC. |
| `--ram-base <hex>` | `0x20000000` | Start of flat RAM. |
| `--ram-size <hex>` | `0x00100000` (1 MiB) | RAM size. |
| `--strict-mem` | off | Treat unmapped data accesses as faults. |
| `--max-cycles <N>` | `0` | Terminate `run` after `N` cycles (0 = unlimited). |
| `--max-pc-stuck <N>` | `0` | Terminate `run` if same PC retires `N` times. |
| `--log <level>` | `0` | Default log level. |

CLINT and UART bases are currently compile-time defaults (`0x02000000` and `0x10000000`); they will become command-line flags when the RTL needs to match them.

## Memory Map

| Region | Address Range | Notes |
|--------|---------------|-------|
| CLINT | `0x02000000`–`0x0200ffff` | `mtime` at `+0xBFF8`, `mtimeh` at `+0xBFFC`. Other CLINT registers ignored. |
| UART | `0x10000000` | Byte read/write. Write appends to output; read consumes input. |
| RAM | `0x20000000`–`0x200fffff` (default) | Flat, little-endian, RWX. |

## Workload ABI

Test programs are expected to follow this convention:

- Pass: write `"PASS\n"` to the UART, then execute `ebreak`.
- Fail: write `"FAIL\n"` to the UART, then execute `ebreak`.

The emulator reports `"ran N instructions"` after `run` and returns to the shell. The UART output is printed to stdout unless redirected.

## Checkpoint Format

Checkpoints are binary files with the following layout:

```
Magic   "AIEM"          (4 bytes)
Version 1               (4 bytes)
RAM size                (4 bytes)
Cycle count             (8 bytes)
PC                      (4 bytes)
GPR x[0..15]            (16 x 4 bytes)
CSRs (mvendorid/marchid/mstatus/mepc/mtvec/mcause) (6 x 4 bytes)
RAM contents            (ram_size bytes)
```

## Testing

Run the built-in tests:

```bash
ctest --test-dir emulator/build --output-on-failure
```

The test executable is `emulator/build/tests/emulator_test`.

## Common Issues

- `cmake: command not found` — install CMake or use the full path to the binary.
- `riscv64-unknown-elf-gcc` is not required for the emulator; assembly workloads can be built with Homebrew LLVM's `llvm-mc`:
  ```bash
  /opt/homebrew/Cellar/llvm/22.1.7_1/bin/llvm-mc -triple=riscv32 -mattr=+e -filetype=obj pass.S -o pass.o
  /opt/homebrew/Cellar/llvm/22.1.7_1/bin/llvm-objcopy -O binary pass.o pass.bin
  ```
- Out-of-range data accesses return `0` by default (open address space). This matches the spec; use `--strict-mem` if you prefer faults.

## Future Work

- Spike adapter for a third reference opinion.
- Verilator RTL adapter sharing the `ISS` interface (difftest harness already in place).
- Shared-clock mode for timing-sensitive difftests.
