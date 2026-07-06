---
name: workload-build
description: Compile or assemble a simple RISC-V workload for the emulator using the configured LLVM toolchain
type: prompt
whenToUse: When I need to turn a C or assembly source file into an ELF or raw binary that can be loaded by the emulator
arguments:
  - source
  - output
---

Build the workload `$source` for the emulator.

## Quick start

1. Make sure you have an LLVM RISC-V toolchain installed. On macOS the example
   config below assumes Homebrew LLVM; on other systems adjust the paths to
   match your install.

2. Create `data/workload-build/config.sh` and set the tool paths. For example:

   ```bash
   # data/workload-build/config.sh
   LLVM_PREFIX=/opt/homebrew/Cellar/llvm/22.1.7_1/bin
   LD=/opt/homebrew/bin/ld.lld
   export LLVM_PREFIX LD
   ```

3. Load the configuration before building:

   ```bash
   source data/workload-build/config.sh
   ```

   This sets toolchain variables (`CC`, `AS`, `LD`, `OBJCOPY`, `OBJDUMP`) and
   target/flag defaults. If the config file is missing, the build helper falls
   back to generic defaults and tools found on `PATH`.

4. Run the helper script:

   ```bash
   ./skills/workload-build/build.sh "$source" "$output"
   ```

   If `$output` is omitted, the output stem is `$source` with its extension
   removed. The script produces:

   - `<stem>.bin` — raw binary to load into the emulator
   - `<stem>.lst` — disassembly listing
   - intermediate `<stem>.o` (and `<stem>.elf` for C)

4. Load and run the resulting binary in the emulator (default RAM base is
   `0x20000000`):

   ```bash
   ./emulator/build/emulator -e "load <stem>.bin 0x20000000; run; exit"
   ```

## Configuring the toolchain

The skill reads `data/workload-build/config.sh` if it exists. It is a plain shell
file; every setting is optional because the build helper falls back to generic
defaults when the config is missing. If you want a config at all, you must
create `data/workload-build/config.sh` yourself — nothing under `data/` is
committed to the repository.

The most common things to override in a fresh environment are the tool paths.
For example, if your LLVM tools live in `/usr/local/llvm/bin` and `ld.lld` is
on `PATH`:

```bash
# data/workload-build/config.sh
LLVM_PREFIX=/usr/local/llvm/bin
LD=ld.lld
export LLVM_PREFIX LD
```

Variables the build helper reads from the config (or the caller's environment):

| Variable | Meaning | Default |
| --- | --- | --- |
| `CC` | C compiler (clang) | `${LLVM_PREFIX}/clang` |
| `AS` | Assembler (llvm-mc) | `${LLVM_PREFIX}/llvm-mc` |
| `LD` | Linker (ld.lld) | `/opt/homebrew/bin/ld.lld` |
| `OBJCOPY` | Object copier | `${LLVM_PREFIX}/llvm-objcopy` |
| `OBJDUMP` | Disassembler | `${LLVM_PREFIX}/llvm-objdump` |
| `WORKLOAD_ARCH` | RISC-V architecture string | `rv32e_zicsr` |
| `WORKLOAD_ABI` | RISC-V ABI | `ilp32e` |
| `WORKLOAD_RAM_BASE` | RAM base address | `0x20000000` |
| `*_FLAGS` | Default flags for each tool | set by the helper |
| `*_EXTRA_FLAGS` | Extra flags appended after defaults | empty |
| `WORKLOAD_BUILD_LINK_SCRIPT` | Linker script path | `skills/workload-build/link.ld` |

After editing `config.sh`, re-run `source data/workload-build/config.sh` before
the next build.

## Manual builds

- Assembly:

  ```bash
  $AS -triple=riscv32 -mattr=+e -filetype=obj -o out.o "$source"
  $OBJCOPY -O binary out.o out.bin
  ```

- C (linked with the skill's linker script and `crt0.S`):

  ```bash
  $AS -triple=riscv32 -mattr=+e -filetype=obj -o crt0.o skills/workload-build/crt0.S
  $CC --target=riscv32 -march=$WORKLOAD_ARCH -mabi=$WORKLOAD_ABI \
      -nostdlib -static -T "$WORKLOAD_BUILD_LINK_SCRIPT" \
      -fuse-ld="$LD" -Wl,--build-id=none \
      -o out.elf crt0.o "$source"
  $OBJCOPY -O binary out.elf out.bin
  ```

  The C file should define `int main(void)`; the provided `crt0.S` sets up the
  stack pointer and calls `main`, then executes `ebreak`.
