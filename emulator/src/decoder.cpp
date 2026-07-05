#include "emulator/decoder.h"
#include "emulator/common.h"
#include <sstream>

namespace emulator {

namespace {

struct Entry {
    u32 mask;
    u32 match;
    InstType type;
};

// Helper macros to build decode-table entries with explicit mask/match.
// The mask covers all bits that are fixed for the instruction encoding.
#define M(m, x, t) { (m), (x), (t) }

constexpr u32 OPCODE     = 0x0000007F;
constexpr u32 OP_F3      = 0x00007000;
constexpr u32 OP_F7      = 0xFE000000;
constexpr u32 OP_F12     = 0xFFF00000;

constexpr Entry decode_table[] = {
    // OP-IMM: opcode + funct3 fixed; shift-immediates also need funct7.
    M(OPCODE | OP_F3, 0x00000013, InstType::ADDI),
    M(OPCODE | OP_F3, 0x00002013, InstType::SLTI),
    M(OPCODE | OP_F3, 0x00003013, InstType::SLTIU),
    M(OPCODE | OP_F3, 0x00004013, InstType::XORI),
    M(OPCODE | OP_F3, 0x00006013, InstType::ORI),
    M(OPCODE | OP_F3, 0x00007013, InstType::ANDI),
    M(OPCODE | OP_F3 | OP_F7, 0x00001013, InstType::SLLI),
    M(OPCODE | OP_F3 | OP_F7, 0x00005013, InstType::SRLI),
    M(OPCODE | OP_F3 | OP_F7, 0x40005013, InstType::SRAI),

    // OP: opcode + funct3 + funct7 fixed.
    M(OPCODE | OP_F3 | OP_F7, 0x00000033, InstType::ADD),
    M(OPCODE | OP_F3 | OP_F7, 0x40000033, InstType::SUB),
    M(OPCODE | OP_F3 | OP_F7, 0x00001033, InstType::SLL),
    M(OPCODE | OP_F3 | OP_F7, 0x00002033, InstType::SLT),
    M(OPCODE | OP_F3 | OP_F7, 0x00003033, InstType::SLTU),
    M(OPCODE | OP_F3 | OP_F7, 0x00004033, InstType::XOR),
    M(OPCODE | OP_F3 | OP_F7, 0x00005033, InstType::SRL),
    M(OPCODE | OP_F3 | OP_F7, 0x40005033, InstType::SRA),
    M(OPCODE | OP_F3 | OP_F7, 0x00006033, InstType::OR),
    M(OPCODE | OP_F3 | OP_F7, 0x00007033, InstType::AND),

    // Upper immediate: only opcode fixed.
    M(OPCODE, 0x00000037, InstType::LUI),
    M(OPCODE, 0x00000017, InstType::AUIPC),

    // Jumps: JAL needs only opcode; JALR opcode+funct3.
    M(OPCODE, 0x0000006F, InstType::JAL),
    M(OPCODE | OP_F3, 0x00000067, InstType::JALR),

    // Branches: opcode + funct3.
    M(OPCODE | OP_F3, 0x00000063, InstType::BEQ),
    M(OPCODE | OP_F3, 0x00001063, InstType::BNE),
    M(OPCODE | OP_F3, 0x00004063, InstType::BLT),
    M(OPCODE | OP_F3, 0x00005063, InstType::BGE),
    M(OPCODE | OP_F3, 0x00006063, InstType::BLTU),
    M(OPCODE | OP_F3, 0x00007063, InstType::BGEU),

    // Load: opcode + funct3.
    M(OPCODE | OP_F3, 0x00000003, InstType::LB),
    M(OPCODE | OP_F3, 0x00001003, InstType::LH),
    M(OPCODE | OP_F3, 0x00002003, InstType::LW),
    M(OPCODE | OP_F3, 0x00004003, InstType::LBU),
    M(OPCODE | OP_F3, 0x00005003, InstType::LHU),

    // Store: opcode + funct3.
    M(OPCODE | OP_F3, 0x00000023, InstType::SB),
    M(OPCODE | OP_F3, 0x00001023, InstType::SH),
    M(OPCODE | OP_F3, 0x00002023, InstType::SW),

    // Memory ordering: fence accepts any pred/succ; fence.i is exact.
    M(OPCODE | OP_F3, 0x0000000F, InstType::FENCE),
    M(OPCODE | OP_F3 | OP_F12, 0x0000100F, InstType::FENCE_I),

    // SYSTEM / privileged: exact 32-bit encodings.
    M(0xFFFFFFFF, 0x00000073, InstType::ECALL),
    M(0xFFFFFFFF, 0x00100073, InstType::EBREAK),
    M(0xFFFFFFFF, 0x30200073, InstType::MRET),
    M(0xFFFFFFFF, 0x10500073, InstType::WFI),

    // CSR: opcode + funct3 fixed.
    M(OPCODE | OP_F3, 0x00001073, InstType::CSRRW),
    M(OPCODE | OP_F3, 0x00002073, InstType::CSRRS),
    M(OPCODE | OP_F3, 0x00003073, InstType::CSRRC),
    M(OPCODE | OP_F3, 0x00005073, InstType::CSRRWI),
    M(OPCODE | OP_F3, 0x00006073, InstType::CSRRSI),
    M(OPCODE | OP_F3, 0x00007073, InstType::CSRRCI),
};

#undef M

} // anonymous namespace

DecodedInst decode(u32 inst) {
    DecodedInst d;
    d.inst = inst;
    d.type = InstType::UNKNOWN;

    for (const auto& e : decode_table) {
        if ((inst & e.mask) == e.match) {
            d.type = e.type;
            break;
        }
    }

    d.rd     = extract(inst, 7, 11);
    d.rs1    = extract(inst, 15, 19);
    d.rs2    = extract(inst, 20, 24);
    d.funct3 = extract(inst, 12, 14);
    d.funct7 = extract(inst, 25, 31);
    d.csr    = extract(inst, 20, 31);
    d.shamt  = extract(inst, 20, 24);

    // Decode immediates
    switch (d.type) {
        case InstType::ADDI:
        case InstType::SLTI:
        case InstType::SLTIU:
        case InstType::XORI:
        case InstType::ORI:
        case InstType::ANDI:
        case InstType::JALR:
        case InstType::LB:
        case InstType::LH:
        case InstType::LW:
        case InstType::LBU:
        case InstType::LHU:
        case InstType::FENCE:
        case InstType::FENCE_I:
            d.imm = sign_extend(extract(inst, 20, 31), 12);
            break;
        case InstType::SLLI:
        case InstType::SRLI:
        case InstType::SRAI:
            d.imm = d.shamt;  // zero-extended
            break;
        case InstType::SB:
        case InstType::SH:
        case InstType::SW:
            d.imm = sign_extend((extract(inst, 25, 31) << 5) | extract(inst, 7, 11), 12);
            break;
        case InstType::BEQ:
        case InstType::BNE:
        case InstType::BLT:
        case InstType::BGE:
        case InstType::BLTU:
        case InstType::BGEU: {
            u32 bimm = (extract(inst, 31, 31) << 12) |
                       (extract(inst, 7, 7)   << 11) |
                       (extract(inst, 25, 30) << 5)  |
                       (extract(inst, 8, 11)  << 1);
            d.imm = sign_extend(bimm, 13);
            break;
        }
        case InstType::LUI:
        case InstType::AUIPC:
            d.imm = inst & 0xFFFFF000;
            break;
        case InstType::JAL: {
            u32 jimm = (extract(inst, 31, 31) << 20) |
                       (extract(inst, 12, 19) << 12) |
                       (extract(inst, 20, 20) << 11) |
                       (extract(inst, 21, 30) << 1);
            d.imm = sign_extend(jimm, 21);
            break;
        }
        case InstType::CSRRW:
        case InstType::CSRRS:
        case InstType::CSRRC:
            d.is_csr_imm = false;
            break;
        case InstType::CSRRWI:
        case InstType::CSRRSI:
        case InstType::CSRRCI:
            d.is_csr_imm = true;
            d.imm = d.rs1;  // uimm[4:0]
            break;
        default:
            break;
    }

    return d;
}

std::string inst_name(InstType type) {
    switch (type) {
        case InstType::ADDI: return "addi";
        case InstType::SLTI: return "slti";
        case InstType::SLTIU: return "sltiu";
        case InstType::XORI: return "xori";
        case InstType::ORI: return "ori";
        case InstType::ANDI: return "andi";
        case InstType::SLLI: return "slli";
        case InstType::SRLI: return "srli";
        case InstType::SRAI: return "srai";
        case InstType::ADD: return "add";
        case InstType::SUB: return "sub";
        case InstType::SLL: return "sll";
        case InstType::SLT: return "slt";
        case InstType::SLTU: return "sltu";
        case InstType::XOR: return "xor";
        case InstType::SRL: return "srl";
        case InstType::SRA: return "sra";
        case InstType::OR: return "or";
        case InstType::AND: return "and";
        case InstType::LUI: return "lui";
        case InstType::AUIPC: return "auipc";
        case InstType::JAL: return "jal";
        case InstType::JALR: return "jalr";
        case InstType::BEQ: return "beq";
        case InstType::BNE: return "bne";
        case InstType::BLT: return "blt";
        case InstType::BGE: return "bge";
        case InstType::BLTU: return "bltu";
        case InstType::BGEU: return "bgeu";
        case InstType::LB: return "lb";
        case InstType::LH: return "lh";
        case InstType::LW: return "lw";
        case InstType::LBU: return "lbu";
        case InstType::LHU: return "lhu";
        case InstType::SB: return "sb";
        case InstType::SH: return "sh";
        case InstType::SW: return "sw";
        case InstType::FENCE: return "fence";
        case InstType::FENCE_I: return "fence.i";
        case InstType::ECALL: return "ecall";
        case InstType::EBREAK: return "ebreak";
        case InstType::MRET: return "mret";
        case InstType::WFI: return "wfi";
        case InstType::CSRRW: return "csrrw";
        case InstType::CSRRS: return "csrrs";
        case InstType::CSRRC: return "csrrc";
        case InstType::CSRRWI: return "csrrwi";
        case InstType::CSRRSI: return "csrrsi";
        case InstType::CSRRCI: return "csrrci";
        default: return "unknown";
    }
}

} // namespace emulator
