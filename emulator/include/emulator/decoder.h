#pragma once

#include "emulator/common.h"
#include <string>

namespace emulator {

enum class InstType {
    UNKNOWN,
    // OP-IMM
    ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
    // OP
    ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND,
    // Upper immediate
    LUI, AUIPC,
    // Jumps
    JAL, JALR,
    // Branches
    BEQ, BNE, BLT, BGE, BLTU, BGEU,
    // Load/Store
    LB, LH, LW, LBU, LHU, SB, SH, SW,
    // Memory ordering
    FENCE, FENCE_I,
    // Privileged
    ECALL, EBREAK, MRET, WFI,
    // CSR
    CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI,
};

struct DecodedInst {
    InstType type = InstType::UNKNOWN;
    u32 inst = 0;
    u32 rd = 0;
    u32 rs1 = 0;
    u32 rs2 = 0;
    u32 funct3 = 0;
    u32 funct7 = 0;
    u32 csr = 0;
    u32 imm = 0;      // sign/zero-extended as appropriate
    u32 shamt = 0;
    bool is_csr_imm = false;
};

DecodedInst decode(u32 inst);
std::string inst_name(InstType type);

} // namespace emulator
