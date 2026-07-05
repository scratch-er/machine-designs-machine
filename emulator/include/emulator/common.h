#pragma once

#include <cstdint>
#include <cstddef>

namespace emulator {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s32 = int32_t;
using s64 = int64_t;

constexpr u32 XLEN = 32;
constexpr u32 GPR_COUNT = 16;  // RV32E

// Major opcodes (inst[6:0])
constexpr u32 OPCODE_LOAD     = 0x03;
constexpr u32 OPCODE_MISC_MEM = 0x0F;
constexpr u32 OPCODE_OP_IMM   = 0x13;
constexpr u32 OPCODE_AUIPC    = 0x17;
constexpr u32 OPCODE_STORE    = 0x23;
constexpr u32 OPCODE_OP       = 0x33;
constexpr u32 OPCODE_LUI      = 0x37;
constexpr u32 OPCODE_BRANCH   = 0x63;
constexpr u32 OPCODE_JALR     = 0x67;
constexpr u32 OPCODE_JAL      = 0x6F;
constexpr u32 OPCODE_SYSTEM   = 0x73;

// Exception cause codes (mcause[30:0])
constexpr u32 CAUSE_INST_MISALIGNED   = 0;
constexpr u32 CAUSE_INST_ACCESS_FAULT = 1;
constexpr u32 CAUSE_ILLEGAL_INST      = 2;
constexpr u32 CAUSE_BREAKPOINT        = 3;
constexpr u32 CAUSE_LOAD_MISALIGNED   = 4;
constexpr u32 CAUSE_LOAD_ACCESS_FAULT = 5;
constexpr u32 CAUSE_STORE_MISALIGNED  = 6;
constexpr u32 CAUSE_STORE_ACCESS_FAULT= 7;
constexpr u32 CAUSE_ECALL_M           = 11;

// CSR addresses
constexpr u32 CSR_MVENDORID = 0xF11;
constexpr u32 CSR_MARCHID   = 0xF12;
constexpr u32 CSR_MSTATUS   = 0x300;
constexpr u32 CSR_MTVEC     = 0x305;
constexpr u32 CSR_MEPC      = 0x341;
constexpr u32 CSR_MCAUSE    = 0x342;

inline u32 extract(u32 inst, u32 lo, u32 hi) {
    return (inst >> lo) & ((1u << (hi - lo + 1)) - 1);
}

inline u32 sign_extend(u32 value, u32 bits) {
    u32 mask = 1u << (bits - 1);
    return (value ^ mask) - mask;
}

} // namespace emulator
