# RV32E_Zicsr Instruction List (with Encodings)

This file lists every instruction the emulator and processor core must implement, based on `specs/core.md` (RV32E_Zicsr, M-mode only) and cross-checked against the RISC-V ISA manual in `specs/riscv-isa-manual/`.

## Programmer's Model

- XLEN = 32.
- General-purpose registers: `x0`–`x15` only. `x0` is hardwired to 0. Encodings using `x16`–`x31` are **reserved** and must raise an illegal-instruction exception.
- Program counter: 32-bit `pc`. Instruction alignment IALIGN = 32, so `pc[1:0]` must be `0b00`.

## Base Instruction Formats

Bit numbering is `inst[31:0]`. Register fields (`rd`, `rs1`, `rs2`) and `funct3` are at fixed positions in every format.

| Format | Layout (31 → 0) |
|--------|-----------------|
| R-type | `funct7[31:25] \| rs2[24:20] \| rs1[19:15] \| funct3[14:12] \| rd[11:7] \| opcode[6:0]` |
| I-type | `imm[11:0][31:20] \| rs1[19:15] \| funct3[14:12] \| rd[11:7] \| opcode[6:0]` |
| S-type | `imm[11:5][31:25] \| rs2[24:20] \| rs1[19:15] \| funct3[14:12] \| imm[4:0][11:7] \| opcode[6:0]` |
| B-type | `imm[12\|10:5][31:25] \| rs2[24:20] \| rs1[19:15] \| funct3[14:12] \| imm[4:1\|11][11:7] \| opcode[6:0]` |
| U-type | `imm[31:12][31:12] \| rd[11:7] \| opcode[6:0]` |
| J-type | `imm[20\|10:1\|11\|19:12][31:12] \| rd[11:7] \| opcode[6:0]` |

### Immediate decoding

| Format | Immediate reconstruction (32-bit signed) |
|--------|------------------------------------------|
| I | `imm = sext(inst[31:20])` |
| S | `imm = sext({inst[31:25], inst[11:7]})` |
| B | `imm = sext({inst[31], inst[7], inst[30:25], inst[11:8], 1'b0})` |
| U | `imm = {inst[31:12], 12'b0}` |
| J | `imm = sext({inst[31], inst[19:12], inst[20], inst[30:21], 1'b0})` |

For RV32E, `shamt` in shift-immediate instructions is encoded in `inst[24:20]`. Bits `[31:25]` are `funct7`.

## Major Opcodes (`inst[6:0]`)

| Opcode (bin) | Opcode (hex) | Name | Notes |
|--------------|--------------|------|-------|
| `0000011` | `0x03` | LOAD | lb, lh, lw, lbu, lhu |
| `0001111` | `0x0F` | MISC-MEM | fence, fence.i |
| `0010011` | `0x13` | OP-IMM | addi, slti, sltiu, xori, ori, andi, slli, srli, srai |
| `0010111` | `0x17` | AUIPC | auipc |
| `0100011` | `0x23` | STORE | sb, sh, sw |
| `0110011` | `0x33` | OP | add, sub, sll, slt, sltu, xor, srl, sra, or, and |
| `0110111` | `0x37` | LUI | lui |
| `1100011` | `0x63` | BRANCH | beq, bne, blt, bge, bltu, bgeu |
| `1100111` | `0x67` | JALR | jalr |
| `1101111` | `0x6F` | JAL | jal |
| `1110011` | `0x73` | SYSTEM | ecall, ebreak, mret, wfi, csrrw, csrrs, csrrc, csrrwi, csrrsi, csrrci |

## 1. Integer Register-Immediate Instructions (OP-IMM, opcode = `0x13`)

| Instruction | `funct3` | `funct7` (bits [31:25]) | Operation |
|-------------|----------|------------------------|-----------|
| `addi`  | `000` | — | `rd = rs1 + sext(imm[11:0])` |
| `slti`  | `010` | — | `rd = (rs1 < imm) ? 1 : 0` (signed) |
| `sltiu` | `011` | — | `rd = (rs1 < imm) ? 1 : 0` (unsigned, `imm` sign-extended then compared as unsigned) |
| `xori`  | `100` | — | `rd = rs1 ^ sext(imm[11:0])` |
| `ori`   | `110` | — | `rd = rs1 \| sext(imm[11:0])` |
| `andi`  | `111` | — | `rd = rs1 & sext(imm[11:0])` |
| `slli`  | `001` | `0000000` | `rd = rs1 << shamt[4:0]` (shift amount lower 5 bits) |
| `srli`  | `101` | `0000000` | `rd = rs1 >> shamt[4:0]` logical |
| `srai`  | `101` | `0100000` | `rd = rs1 >> shamt[4:0]` arithmetic |

Notes:
- `addi` ignores arithmetic overflow; result is low 32 bits.
- For shifts, `shamt` is encoded in `inst[24:20]`. Since RV32E is RV32, only `shamt[4:0]` is valid; `funct7` distinguishes `srli`/`srai`.
- Any OP-IMM encoding with `funct3`/`funct7` not listed above is illegal.

## 2. Integer Register-Register Instructions (OP, opcode = `0x33`)

| Instruction | `funct3` | `funct7` | Operation |
|-------------|----------|----------|-----------|
| `add`  | `000` | `0000000` | `rd = rs1 + rs2` |
| `sub`  | `000` | `0100000` | `rd = rs1 - rs2` |
| `sll`  | `001` | `0000000` | `rd = rs1 << rs2[4:0]` |
| `slt`  | `010` | `0000000` | `rd = (rs1 < rs2) ? 1 : 0` (signed) |
| `sltu` | `011` | `0000000` | `rd = (rs1 < rs2) ? 1 : 0` (unsigned) |
| `xor`  | `100` | `0000000` | `rd = rs1 ^ rs2` |
| `srl`  | `101` | `0000000` | `rd = rs1 >> rs2[4:0]` logical |
| `sra`  | `101` | `0100000` | `rd = rs1 >> rs2[4:0]` arithmetic |
| `or`   | `110` | `0000000` | `rd = rs1 \| rs2` |
| `and`  | `111` | `0000000` | `rd = rs1 & rs2` |

Note: For shifts, only `rs2[4:0]` is used; upper bits are ignored.

## 3. Upper-Immediate Instructions

| Instruction | Opcode | `funct3` | Operation |
|-------------|--------|----------|-----------|
| `lui`   | `0110111` (`0x37`) | — | `rd = {imm[31:12], 12'b0}` |
| `auipc` | `0010111` (`0x17`) | — | `rd = pc + {imm[31:12], 12'b0}` |

## 4. Unconditional Jump Instructions

| Instruction | Opcode | `funct3` | Target address | Link value |
|-------------|--------|----------|----------------|------------|
| `jal`  | `1101111` (`0x6F`) | — | `pc + sext({inst[31], inst[19:12], inst[20], inst[30:21], 1'b0})` | `rd = pc + 4` |
| `jalr` | `1100111` (`0x67`) | `000` | `(rs1 + sext(inst[31:20])) & ~1` | `rd = pc + 4` |

Notes:
- `jal` offset is a signed 21-bit value (20 encoded bits + implicit 0), range ±1 MiB, target must be 4-byte aligned.
- `jalr` target has bit 0 cleared.
- If a jump/branch target is not 4-byte aligned, raise **instruction address misaligned** exception.

## 5. Conditional Branch Instructions (BRANCH, opcode = `0x63`)

| Instruction | `funct3` | Condition | Next pc |
|-------------|----------|-----------|---------|
| `beq`  | `000` | `rs1 == rs2` | `pc + imm` |
| `bne`  | `001` | `rs1 != rs2` | `pc + imm` |
| `blt`  | `100` | `rs1 < rs2` (signed) | `pc + imm` |
| `bge`  | `101` | `rs1 >= rs2` (signed) | `pc + imm` |
| `bltu` | `110` | `rs1 < rs2` (unsigned) | `pc + imm` |
| `bgeu` | `111` | `rs1 >= rs2` (unsigned) | `pc + imm` |

If condition is false: `pc += 4`.
Branch offset `imm` is the B-immediate, 13-bit signed (12 encoded + implicit 0), range ±4 KiB.

## 6. Load Instructions (LOAD, opcode = `0x03`)

| Instruction | `funct3` | Operation |
|-------------|----------|-----------|
| `lb`  | `000` | `rd = sext(mem8[rs1 + imm])` |
| `lh`  | `001` | `rd = sext(mem16[rs1 + imm])` |
| `lw`  | `010` | `rd = mem32[rs1 + imm]` |
| `lbu` | `100` | `rd = zext(mem8[rs1 + imm])` |
| `lhu` | `101` | `rd = zext(mem16[rs1 + imm])` |

Notes:
- Effective address must be naturally aligned (`lh`/`lhu` → 2-byte aligned, `lw` → 4-byte aligned). Misaligned loads raise **load address misaligned**.
- AXI SLVERR/DECERR on load raises **load access fault**.

## 7. Store Instructions (STORE, opcode = `0x23`)

| Instruction | `funct3` | Operation |
|-------------|----------|-----------|
| `sb` | `000` | `mem8[rs1 + imm]  = rs2[7:0]` |
| `sh` | `001` | `mem16[rs1 + imm] = rs2[15:0]` |
| `sw` | `010` | `mem32[rs1 + imm] = rs2` |

Notes:
- Effective address must be naturally aligned. Misaligned stores raise **store/AMO address misaligned**.
- AXI SLVERR/DECERR on store raises **store/AMO access fault**.

## 8. Memory Ordering Instructions (MISC-MEM, opcode = `0x0F`)

| Instruction | `funct3` | `inst[31:20]` / other fields | Notes |
|-------------|----------|------------------------------|-------|
| `fence`   | `000` | `rd=0`, `rs1=0`, `fm=0000`, `pred/succ` arbitrary | Implemented as `nop` per core spec. |
| `fence.i` | `001` | `rd=0`, `rs1=0`, `imm[11:0]=0` | Flushes the instruction cache in the core; in emulator, acts as instruction-fetch synchronization barrier (functional model may implement as a simple `nop` plus any decode-cache flush). |

Note: `fence` encodings other than `fence.i` under MISC-MEM are not needed for RV32E_Zicsr.

## 9. Environment / Privileged Instructions (SYSTEM, opcode = `0x73`, `funct3 = 000`)

These instructions share `opcode = 1110011`, `funct3 = 000`, `rd = 0`, `rs1 = 0`, and use `funct12` (bits [31:20]) to distinguish the operation.

| Instruction | `funct12` (bin) | `funct12` (hex) | Operation |
|-------------|-----------------|-----------------|-----------|
| `ecall`  | `000000000000` | `0x000` | Raise **environment call from M-mode** exception (cause = 11). |
| `ebreak` | `000000000001` | `0x001` | Raise **breakpoint** exception (cause = 3). |
| `mret`   | `001100000010` | `0x302` | Return from M-mode trap: `pc = mepc`. |
| `wfi`    | `000100000101` | `0x105` | Wait for interrupt; implement as `nop` per core spec. |

Any SYSTEM instruction with `funct3 = 000` and `funct12` not in the table above is illegal.

## 10. Zicsr Instructions (SYSTEM, opcode = `0x73`, `funct3 = 001–111`)

All CSR instructions atomically read-modify-write a single CSR. The 12-bit CSR address is in `inst[31:20]`. Immediate forms use a 5-bit zero-extended immediate encoded in the `rs1` field.

| Instruction | `funct3` | Source operand | Read CSR? | Write CSR? |
|-------------|----------|----------------|-----------|------------|
| `csrrw`  | `001` | `rs1` | Yes, unless `rd = x0` | Always |
| `csrrs`  | `010` | `rs1` | Always | No if `rs1 = x0` |
| `csrrc`  | `011` | `rs1` | Always | No if `rs1 = x0` |
| `csrrwi` | `101` | `uimm[4:0]` | Yes, unless `rd = x0` | Always |
| `csrrsi` | `110` | `uimm[4:0]` | Always | No if `uimm = 0` |
| `csrrci` | `111` | `uimm[4:0]` | Always | No if `uimm = 0` |

Operation:
- `csrrw`:  `tmp = csr; csr = rs1; rd = tmp` (read suppressed if `rd = x0`).
- `csrrs`:  `rd = csr; csr = csr \| rs1`.
- `csrrc`:  `rd = csr; csr = csr & ~rs1`.
- `csrrwi`: `tmp = csr; csr = zext(uimm); rd = tmp` (read suppressed if `rd = x0`).
- `csrrsi`: `rd = csr; csr = csr \| zext(uimm)`.
- `csrrci`: `rd = csr; csr = csr & ~zext(uimm)`.

### Implemented CSRs

| CSR | Address (hex) | Privilege | Behavior |
|-----|---------------|-----------|----------|
| `mvendorid` | `0xF11` | MRO | Hardcoded to 0. |
| `marchid`   | `0xF12` | MRO | Hardcoded to 0. |
| `mstatus`   | `0x300` | MRW | MPP hardcoded to M-mode (bits [12:11] = `11`), all other bits hardcoded to 0. |
| `mtvec`     | `0x305` | MRW | Holds 32-bit trap vector base address. Low 2 bits WARL; implementation aligned to 4 bytes. |
| `mepc`      | `0x341` | MRW | Holds 32-bit exception program counter. Low 2 bits hardcoded to 0 (IALIGN=32). |
| `mcause`    | `0x342` | MRW | Exception cause; bit 31 = 0 for synchronous exceptions, bits [30:0] = cause code. |

Any access to a CSR outside this list raises an **illegal instruction** exception.

## 11. Exception and Interrupt Cause Codes

`mcause` layout: bit 31 = Interrupt (0 for exceptions, 1 for interrupts); bits [30:0] = Cause code.

This core does **not** support interrupts (async), so bit 31 is always 0 in normal operation.

| Cause Code | Name | Source |
|------------|------|--------|
| 0 | Instruction address misaligned | Jump/branch target not 4-byte aligned. |
| 1 | Instruction access fault | AXI SLVERR/DECERR on instruction fetch. |
| 2 | Illegal instruction | Unknown opcode, reserved register, unimplemented CSR, etc. |
| 3 | Breakpoint | `ebreak` instruction. |
| 4 | Load address misaligned | Load address not naturally aligned. |
| 5 | Load access fault | AXI SLVERR/DECERR on load. |
| 6 | Store/AMO address misaligned | Store address not naturally aligned. |
| 7 | Store/AMO access fault | AXI SLVERR/DECERR on store. |
| 11 | Environment call from M-mode | `ecall` instruction. |

## 12. Memory Map and Peripherals to Model in the Emulator

The emulator uses a single flat RWX address space, plus the following special regions:

| Region | Address range | Notes |
|--------|---------------|-------|
| Built-in CLINT | `0x02000000`–`0x0200ffff` (configurable, default) | See below. |
| Virtual UART | `0x10000000` | Write → output character; read/input configurable via emulator command. |
| Main memory | Configurable, must include reset address `0x20000000` (default) | Flat, RWX, little-endian. |

### CLINT register map (within the mapped region)

The core spec says the built-in CLINT only implements `mtime` and `mtimeh`, and does not provide interrupts. Accesses to `mtimecmp`, `mtimecmph`, and `msip` are ignored.

| Offset (hex) | Register | Size | Access | Behavior |
|--------------|----------|------|--------|----------|
| `0xBFF8` | `mtime`   | 32b | R/W | Lower 32 bits of cycle counter; increments by 1 each cycle. |
| `0xBFFC` | `mtimeh`  | 32b | R/W | Upper 32 bits of cycle counter. |
| `0x0000` | `msip`    | 32b | R/W | Ignored (no effect, read undefined). |
| `0x4000` | `mtimecmp` low | 32b | R/W | Ignored. |
| `0x4004` | `mtimecmp` high | 32b | R/W | Ignored. |

Note: The `clint.rst` reference document gives a larger bound (`0x200BFFF`) and `tick_count`, but the project spec (`specs/core.md`) overrides this: timer increments every cycle, only `mtime`/`mtimeh` are functional, and other CLINT registers are no-ops.

## 13. Summary of Reserved / Illegal Encodings

The following must raise an **illegal instruction** exception:

- Any opcode not listed in the Major Opcodes table.
- Any `funct3`/`funct7` combination not listed for the corresponding opcode.
- Any instruction using register specifier `rd`, `rs1`, or `rs2` in the range `16`–`31` (RV32E restriction).
- Any CSR access where the CSR address is not one of the six implemented CSRs.
- `SYSTEM` instructions with `funct3 = 000` and `funct12` not equal to `ecall`, `ebreak`, `mret`, or `wfi`.
- Compressed instructions (RISC-V C extension is not part of RV32E_Zicsr).

## 14. Notes for the Core Design

- RV32E uses only `x0`–`x15`; any encoding with `x16`–`x31` is reserved and must trap.
- All memory accesses are naturally aligned; misaligned accesses raise the corresponding exception.
- The external bus is 32-bit AXI4; SLVERR/DECERR responses raise access-fault exceptions on fetch/load/store.
- `fence` is a `nop`; `fence.i` flushes the instruction cache; `wfi` is a `nop`.
- Virtual memory, PMP, PMA, and interrupts are not supported.
- The reset address is configurable, default `0x20000000`.
- The instruction cache is 8 instructions (32 bytes), 16-byte line, direct-mapped, flip-flop based; all instruction-fetcheable addresses are cacheable.

## References

- `specs/core.md` — project core specification.
- `specs/emulator.md` — project emulator specification.
- `specs/riscv-isa-manual/src/unpriv/rv32e.adoc` — RV32E register model.
- `specs/riscv-isa-manual/src/unpriv/rv32.adoc` — RV32I instruction definitions.
- `specs/riscv-isa-manual/src/unpriv/rv-32-64g.adoc` — opcode map and instruction encodings.
- `specs/riscv-isa-manual/src/unpriv/zicsr.adoc` — CSR instruction semantics.
- `specs/riscv-isa-manual/src/unpriv/zifencei.adoc` — `fence.i` semantics.
- `specs/riscv-isa-manual/src/priv/machine.adoc` — privileged instructions, exception causes, CSR details.
- `specs/clint.rst` — CLINT reference (overridden by `specs/core.md` for this project).
