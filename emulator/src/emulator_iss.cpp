#include "emulator/emulator_iss.h"
#include "emulator/decoder.h"
#include "emulator/memory.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace emulator {

// Minimal ELF32 parsing constants.
static constexpr uint8_t ELFMAG[4] = {0x7f, 'E', 'L', 'F'};
static constexpr uint8_t ELFCLASS32 = 1;
static constexpr uint8_t ELFDATA2LSB = 1;
static constexpr uint16_t ET_EXEC = 2;
static constexpr uint16_t EM_RISCV = 243;
static constexpr uint32_t PT_LOAD = 1;

struct Elf32_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

static uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])        |
           (static_cast<uint32_t>(p[1]) << 8)  |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

EmulatorISS::EmulatorISS(Config cfg)
    : cfg_(std::move(cfg)),
      memory_(cfg_),
      clint_(cfg_),
      tracer_(cfg_.log_level) {
    hart_.reset(cfg_.reset_vector);
    clint_.reset();
    uart_.reset();
}

void EmulatorISS::reset(u32 reset_addr) {
    cycle_ = 0;
    retire_idx_ = 0;
    ring_head_ = 0;
    ring_full_ = false;
    hart_.reset(reset_addr);
    clint_.reset();
    uart_.reset();
}

bool EmulatorISS::step_cycle() {
    CommitEvent ev;
    return step_inst(ev);
}

void EmulatorISS::push_ring(const CommitEvent& ev) {
    ring_buffer_[ring_head_] = ev;
    ring_head_ = (ring_head_ + 1) % RING_SIZE;
    if (ring_head_ == 0) ring_full_ = true;
}

std::vector<CommitEvent> EmulatorISS::last_events(size_t n) const {
    if (n > RING_SIZE) n = RING_SIZE;
    size_t count = ring_full_ ? RING_SIZE : ring_head_;
    if (n > count) n = count;
    std::vector<CommitEvent> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (ring_head_ + RING_SIZE - 1 - i) % RING_SIZE;
        out.push_back(ring_buffer_[idx]);
    }
    return out;
}

u32 EmulatorISS::csr(u32 addr) const {
    u32 value = 0;
    hart_.read_csr(addr, value);
    return value;
}

u32 EmulatorISS::read_mem(u32 addr, u32 size) {
    u32 value = 0;
    // Try MMIO first
    if (cfg_.in_clint(addr)) {
        clint_.load(addr, size, value);
        return value;
    }
    if (cfg_.in_uart(addr)) {
        uart_.load(addr - cfg_.uart_base, size, value);
        return value;
    }
    memory_.load(addr, size, value);
    return value;
}

void EmulatorISS::write_mem(u32 addr, u32 size, u32 data) {
    if (cfg_.in_clint(addr)) {
        clint_.store(addr, size, data);
        return;
    }
    if (cfg_.in_uart(addr)) {
        uart_.store(addr - cfg_.uart_base, size, data);
        return;
    }
    memory_.store(addr, size, data);
}

bool EmulatorISS::load_bin(const std::string& path, u32 addr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<u8> data((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    memory_.write_ram(addr, data.data(), data.size());
    return true;
}

bool EmulatorISS::load_elf(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::vector<u8> file((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    if (file.size() < sizeof(Elf32_Ehdr)) return false;

    const u8* p = file.data();
    if (std::memcmp(p, ELFMAG, 4) != 0) return false;
    if (p[4] != ELFCLASS32 || p[5] != ELFDATA2LSB) return false;

    Elf32_Ehdr ehdr{};
    ehdr.e_type      = read_u16(p + 16);
    ehdr.e_machine   = read_u16(p + 18);
    ehdr.e_version   = read_u32(p + 20);
    ehdr.e_entry     = read_u32(p + 24);
    ehdr.e_phoff     = read_u32(p + 28);
    ehdr.e_shoff     = read_u32(p + 32);
    ehdr.e_flags     = read_u32(p + 36);
    ehdr.e_ehsize    = read_u16(p + 40);
    ehdr.e_phentsize = read_u16(p + 42);
    ehdr.e_phnum     = read_u16(p + 44);

    if (ehdr.e_type != ET_EXEC) return false;
    if (ehdr.e_machine != EM_RISCV) return false;
    if (ehdr.e_phoff == 0 || ehdr.e_phentsize < sizeof(Elf32_Phdr)) return false;
    if (ehdr.e_phoff + static_cast<u32>(ehdr.e_phnum) * ehdr.e_phentsize > file.size()) return false;

    for (uint16_t i = 0; i < ehdr.e_phnum; ++i) {
        const u8* ph = file.data() + ehdr.e_phoff + i * ehdr.e_phentsize;
        Elf32_Phdr phdr{};
        phdr.p_type   = read_u32(ph + 0);
        phdr.p_offset = read_u32(ph + 4);
        phdr.p_vaddr  = read_u32(ph + 8);
        phdr.p_paddr  = read_u32(ph + 12);
        phdr.p_filesz = read_u32(ph + 16);
        phdr.p_memsz  = read_u32(ph + 20);

        if (phdr.p_type != PT_LOAD) continue;
        if (phdr.p_offset + phdr.p_filesz > file.size()) return false;

        u32 addr = phdr.p_paddr ? phdr.p_paddr : phdr.p_vaddr;
        if (phdr.p_filesz > 0) {
            memory_.write_ram(addr, file.data() + phdr.p_offset, phdr.p_filesz);
        }
        if (phdr.p_memsz > phdr.p_filesz) {
            u32 bss_size = phdr.p_memsz - phdr.p_filesz;
            memory_.fill_ram(addr + phdr.p_filesz, 0, bss_size);
        }
    }

    hart_.reset(ehdr.e_entry);
    return true;
}

bool EmulatorISS::save_checkpoint(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const char magic[4] = {'A', 'I', 'E', 'M'};
    u32 version = 1;
    f.write(magic, 4);
    f.write(reinterpret_cast<const char*>(&version), sizeof(version));
    f.write(reinterpret_cast<const char*>(&cfg_.ram_size), sizeof(cfg_.ram_size));
    f.write(reinterpret_cast<const char*>(&cycle_), sizeof(cycle_));
    f.write(reinterpret_cast<const char*>(&hart_.pc), sizeof(hart_.pc));
    f.write(reinterpret_cast<const char*>(hart_.x.data()), GPR_COUNT * sizeof(u32));
    u32 csrs[6] = {hart_.mvendorid, hart_.marchid, hart_.mstatus,
                   hart_.mepc, hart_.mtvec, hart_.mcause};
    f.write(reinterpret_cast<const char*>(csrs), sizeof(csrs));
    f.write(reinterpret_cast<const char*>(memory_.ram().data()), cfg_.ram_size);
    return f.good();
}

bool EmulatorISS::load_checkpoint(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4];
    f.read(magic, 4);
    if (std::memcmp(magic, "AIEM", 4) != 0) return false;
    u32 version = 0;
    f.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) return false;
    u32 ram_size = 0;
    f.read(reinterpret_cast<char*>(&ram_size), sizeof(ram_size));
    if (ram_size != cfg_.ram_size) return false;
    f.read(reinterpret_cast<char*>(&cycle_), sizeof(cycle_));
    f.read(reinterpret_cast<char*>(&hart_.pc), sizeof(hart_.pc));
    f.read(reinterpret_cast<char*>(hart_.x.data()), GPR_COUNT * sizeof(u32));
    u32 csrs[6];
    f.read(reinterpret_cast<char*>(csrs), sizeof(csrs));
    hart_.mstatus = csrs[2];
    hart_.mepc    = csrs[3];
    hart_.mtvec   = csrs[4];
    hart_.mcause  = csrs[5];
    std::vector<u8> ram(ram_size);
    f.read(reinterpret_cast<char*>(ram.data()), ram_size);
    memory_.write_ram(cfg_.ram_base, ram.data(), ram_size);
    return f.good();
}

CommitEvent EmulatorISS::commit(u32 pc, u32 inst, u32 rd, u32 rd_value, u32 next_pc,
                                bool exception, u32 cause) {
    CommitEvent ev;
    ev.cycle = cycle_;
    ev.pc = pc;
    ev.inst = inst;
    ev.rd = rd;
    ev.rd_value = rd_value;
    ev.exception = exception;
    ev.cause = cause;
    ev.next_pc = next_pc;
    tracer_.trace_inst(retire_idx_, ev);
    push_ring(ev);
    ++retire_idx_;
    return ev;
}

bool EmulatorISS::execute(const DecodedInst& d, CommitEvent& out) {
    const u32 pc = hart_.pc;
    u32 next_pc = pc + 4;
    u32 rd = 0;
    u32 rd_value = 0;
    bool exception = false;
    u32 cause = 0;

    // Helper to take an exception and stop executing this instruction normally.
    auto take_exc = [&](u32 c, u32 tval = 0) {
        hart_.take_exception(c, tval);
        exception = true;
        cause = c;
        next_pc = hart_.pc;
    };

    // RV32E register validation based on instruction format.
    auto has_rd = [](InstType t) {
        return t != InstType::SB && t != InstType::SH && t != InstType::SW &&
               t != InstType::BEQ && t != InstType::BNE &&
               t != InstType::BLT && t != InstType::BGE &&
               t != InstType::BLTU && t != InstType::BGEU;
    };
    auto has_rs1_reg = [](InstType t) {
        return t == InstType::ADDI || t == InstType::SLTI || t == InstType::SLTIU ||
               t == InstType::XORI || t == InstType::ORI || t == InstType::ANDI ||
               t == InstType::SLLI || t == InstType::SRLI || t == InstType::SRAI ||
               t == InstType::JALR ||
               t == InstType::LB || t == InstType::LH || t == InstType::LW ||
               t == InstType::LBU || t == InstType::LHU ||
               t == InstType::SB || t == InstType::SH || t == InstType::SW ||
               t == InstType::BEQ || t == InstType::BNE ||
               t == InstType::BLT || t == InstType::BGE ||
               t == InstType::BLTU || t == InstType::BGEU ||
               t == InstType::CSRRW || t == InstType::CSRRS || t == InstType::CSRRC;
    };
    auto has_rs2_reg = [](InstType t) {
        return t == InstType::ADD || t == InstType::SUB || t == InstType::SLL ||
               t == InstType::SLT || t == InstType::SLTU || t == InstType::XOR ||
               t == InstType::SRL || t == InstType::SRA || t == InstType::OR ||
               t == InstType::AND ||
               t == InstType::SB || t == InstType::SH || t == InstType::SW ||
               t == InstType::BEQ || t == InstType::BNE ||
               t == InstType::BLT || t == InstType::BGE ||
               t == InstType::BLTU || t == InstType::BGEU;
    };

    if (d.type != InstType::UNKNOWN) {
        if (has_rd(d.type) && d.rd >= GPR_COUNT) {
            take_exc(CAUSE_ILLEGAL_INST);
            out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
            return true;
        }
        if (has_rs1_reg(d.type) && d.rs1 >= GPR_COUNT) {
            take_exc(CAUSE_ILLEGAL_INST);
            out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
            return true;
        }
        if (has_rs2_reg(d.type) && d.rs2 >= GPR_COUNT) {
            take_exc(CAUSE_ILLEGAL_INST);
            out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
            return true;
        }
    }

    auto alu_op1 = hart_.read_reg(d.rs1);
    auto alu_op2 = hart_.read_reg(d.rs2);
    u32 alu_imm = d.imm;

    switch (d.type) {
        // OP-IMM
        case InstType::ADDI:  rd = d.rd; rd_value = alu_op1 + alu_imm; break;
        case InstType::SLTI:  rd = d.rd; rd_value = (static_cast<s32>(alu_op1) < static_cast<s32>(alu_imm)) ? 1 : 0; break;
        case InstType::SLTIU: rd = d.rd; rd_value = (alu_op1 < alu_imm) ? 1 : 0; break;
        case InstType::XORI:  rd = d.rd; rd_value = alu_op1 ^ alu_imm; break;
        case InstType::ORI:   rd = d.rd; rd_value = alu_op1 | alu_imm; break;
        case InstType::ANDI:  rd = d.rd; rd_value = alu_op1 & alu_imm; break;
        case InstType::SLLI:  rd = d.rd; rd_value = alu_op1 << (alu_imm & 0x1F); break;
        case InstType::SRLI:  rd = d.rd; rd_value = alu_op1 >> (alu_imm & 0x1F); break;
        case InstType::SRAI:  rd = d.rd; rd_value = static_cast<u32>(static_cast<s32>(alu_op1) >> (alu_imm & 0x1F)); break;

        // OP
        case InstType::ADD:  rd = d.rd; rd_value = alu_op1 + alu_op2; break;
        case InstType::SUB:  rd = d.rd; rd_value = alu_op1 - alu_op2; break;
        case InstType::SLL:  rd = d.rd; rd_value = alu_op1 << (alu_op2 & 0x1F); break;
        case InstType::SLT:  rd = d.rd; rd_value = (static_cast<s32>(alu_op1) < static_cast<s32>(alu_op2)) ? 1 : 0; break;
        case InstType::SLTU: rd = d.rd; rd_value = (alu_op1 < alu_op2) ? 1 : 0; break;
        case InstType::XOR:  rd = d.rd; rd_value = alu_op1 ^ alu_op2; break;
        case InstType::SRL:  rd = d.rd; rd_value = alu_op1 >> (alu_op2 & 0x1F); break;
        case InstType::SRA:  rd = d.rd; rd_value = static_cast<u32>(static_cast<s32>(alu_op1) >> (alu_op2 & 0x1F)); break;
        case InstType::OR:   rd = d.rd; rd_value = alu_op1 | alu_op2; break;
        case InstType::AND:  rd = d.rd; rd_value = alu_op1 & alu_op2; break;

        // Upper immediate
        case InstType::LUI:   rd = d.rd; rd_value = alu_imm; break;
        case InstType::AUIPC: rd = d.rd; rd_value = pc + alu_imm; break;

        // Jumps
        case InstType::JAL:
            rd = d.rd;
            rd_value = pc + 4;
            next_pc = pc + alu_imm;
            if ((next_pc & 3u) != 0) {
                take_exc(CAUSE_INST_MISALIGNED, next_pc);
                out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
                return true;
            }
            break;
        case InstType::JALR:
            rd = d.rd;
            rd_value = pc + 4;
            next_pc = (alu_op1 + alu_imm) & ~1u;
            if ((next_pc & 3u) != 0) {
                take_exc(CAUSE_INST_MISALIGNED, next_pc);
                out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
                return true;
            }
            break;

        // Branches
        case InstType::BEQ:  next_pc = (alu_op1 == alu_op2) ? pc + alu_imm : next_pc; break;
        case InstType::BNE:  next_pc = (alu_op1 != alu_op2) ? pc + alu_imm : next_pc; break;
        case InstType::BLT:  next_pc = (static_cast<s32>(alu_op1) < static_cast<s32>(alu_op2)) ? pc + alu_imm : next_pc; break;
        case InstType::BGE:  next_pc = (static_cast<s32>(alu_op1) >= static_cast<s32>(alu_op2)) ? pc + alu_imm : next_pc; break;
        case InstType::BLTU: next_pc = (alu_op1 < alu_op2) ? pc + alu_imm : next_pc; break;
        case InstType::BGEU: next_pc = (alu_op1 >= alu_op2) ? pc + alu_imm : next_pc; break;

        // Load
        case InstType::LB:
        case InstType::LH:
        case InstType::LW:
        case InstType::LBU:
        case InstType::LHU: {
            u32 addr = alu_op1 + alu_imm;
            u32 size = 0;
            bool sext = false;
            switch (d.type) {
                case InstType::LB:  size = 1; sext = true; break;
                case InstType::LH:  size = 2; sext = true; break;
                case InstType::LW:  size = 4; sext = false; break;
                case InstType::LBU: size = 1; sext = false; break;
                case InstType::LHU: size = 2; sext = false; break;
                default: break;
            }
            u32 value = 0;
            MemResult mr = MemResult::ACCESS_FAULT;
            if (cfg_.in_clint(addr)) {
                // CLINT only supports 4-byte accesses per spec note.
                if (size != 4) {
                    mr = MemResult::MISALIGNED;
                } else {
                    mr = clint_.load(addr, size, value) ? MemResult::OK : MemResult::ACCESS_FAULT;
                }
            } else if (cfg_.in_uart(addr)) {
                mr = uart_.load(addr - cfg_.uart_base, size, value) ? MemResult::OK : MemResult::ACCESS_FAULT;
            } else {
                mr = memory_.load(addr, size, value);
            }
            if (mr == MemResult::MISALIGNED) {
                take_exc(CAUSE_LOAD_MISALIGNED, addr);
                out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
                return true;
            } else if (mr == MemResult::ACCESS_FAULT) {
                take_exc(CAUSE_LOAD_ACCESS_FAULT, addr);
                out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
                return true;
            }
            if (sext) {
                if (size == 1) value = sign_extend(value & 0xFF, 8);
                else if (size == 2) value = sign_extend(value & 0xFFFF, 16);
            }
            rd = d.rd;
            rd_value = value;
            tracer_.trace_mem_load(addr, size, value);
            break;
        }

        // Store
        case InstType::SB:
        case InstType::SH:
        case InstType::SW: {
            u32 addr = alu_op1 + alu_imm;
            u32 size = 0;
            switch (d.type) {
                case InstType::SB: size = 1; break;
                case InstType::SH: size = 2; break;
                case InstType::SW: size = 4; break;
                default: break;
            }
            u32 data = alu_op2;
            MemResult mr = MemResult::ACCESS_FAULT;
            if (cfg_.in_clint(addr)) {
                if (size != 4) {
                    mr = MemResult::MISALIGNED;
                } else {
                    mr = clint_.store(addr, size, data) ? MemResult::OK : MemResult::ACCESS_FAULT;
                }
            } else if (cfg_.in_uart(addr)) {
                mr = uart_.store(addr - cfg_.uart_base, size, data) ? MemResult::OK : MemResult::ACCESS_FAULT;
            } else {
                mr = memory_.store(addr, size, data);
            }
            if (mr == MemResult::MISALIGNED) {
                take_exc(CAUSE_STORE_MISALIGNED, addr);
                out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
                return true;
            } else if (mr == MemResult::ACCESS_FAULT) {
                take_exc(CAUSE_STORE_ACCESS_FAULT, addr);
                out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
                return true;
            }
            tracer_.trace_mem_store(addr, size, data);
            break;
        }

        // Memory ordering
        case InstType::FENCE:
        case InstType::FENCE_I:
            // nop
            break;

        // Privileged
        case InstType::ECALL:
            take_exc(CAUSE_ECALL_M);
            out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
            return true;
        case InstType::EBREAK:
            hart_.halted = true;
            take_exc(CAUSE_BREAKPOINT);
            out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
            return true;
        case InstType::MRET:
            next_pc = hart_.mepc;
            break;
        case InstType::WFI:
            // nop
            break;

        // CSR
        case InstType::CSRRW:
        case InstType::CSRRS:
        case InstType::CSRRC:
        case InstType::CSRRWI:
        case InstType::CSRRSI:
        case InstType::CSRRCI: {
            u32 csr_addr = d.csr;
            u32 csr_old = 0;
            if (!hart_.read_csr(csr_addr, csr_old)) {
                take_exc(CAUSE_ILLEGAL_INST);
                out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
                return true;
            }
            u32 source = d.is_csr_imm ? d.imm : hart_.read_reg(d.rs1);
            u32 csr_new = csr_old;
            bool do_write = true;
            switch (d.type) {
                case InstType::CSRRW:
                case InstType::CSRRWI:
                    csr_new = source;
                    break;
                case InstType::CSRRS:
                    if (d.rs1 == 0 && !d.is_csr_imm) do_write = false;
                    csr_new = csr_old | source;
                    break;
                case InstType::CSRRSI:
                    if (d.imm == 0) do_write = false;
                    csr_new = csr_old | source;
                    break;
                case InstType::CSRRC:
                    if (d.rs1 == 0 && !d.is_csr_imm) do_write = false;
                    csr_new = csr_old & ~source;
                    break;
                case InstType::CSRRCI:
                    if (d.imm == 0) do_write = false;
                    csr_new = csr_old & ~source;
                    break;
                default: break;
            }
            if (do_write && !hart_.write_csr(csr_addr, csr_new)) {
                take_exc(CAUSE_ILLEGAL_INST);
                out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
                return true;
            }
            if (d.rd != 0) {
                rd = d.rd;
                rd_value = csr_old;
            }
            break;
        }

        case InstType::UNKNOWN:
        default:
            take_exc(CAUSE_ILLEGAL_INST);
            out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
            return true;
    }

    // Validate branch target alignment after condition resolution.
    if ((d.type == InstType::BEQ || d.type == InstType::BNE ||
         d.type == InstType::BLT || d.type == InstType::BGE ||
         d.type == InstType::BLTU || d.type == InstType::BGEU) &&
        next_pc != pc + 4 && (next_pc & 3u) != 0) {
        take_exc(CAUSE_INST_MISALIGNED, next_pc);
        out = commit(pc, d.inst, 0, 0, next_pc, exception, cause);
        return true;
    }

    // Commit register write.
    if (rd != 0 && rd < GPR_COUNT) {
        hart_.write_reg(rd, rd_value);
        tracer_.trace_reg(rd, rd_value);
    }

    hart_.pc = next_pc;
    out = commit(pc, d.inst, rd, rd_value, next_pc, exception, cause);
    return true;
}

bool EmulatorISS::step_inst(CommitEvent& out) {
    if (hart_.halted) return false;

    // Fetch
    u32 inst = 0;
    MemResult fr = memory_.fetch(hart_.pc, inst);
    if (fr == MemResult::MISALIGNED) {
        hart_.take_exception(CAUSE_INST_MISALIGNED, hart_.pc);
        out = commit(hart_.pc, inst, 0, 0, hart_.pc, true, CAUSE_INST_MISALIGNED);
        clint_.tick();
        ++cycle_;
        return true;
    } else if (fr == MemResult::ACCESS_FAULT) {
        hart_.take_exception(CAUSE_INST_ACCESS_FAULT, hart_.pc);
        out = commit(hart_.pc, inst, 0, 0, hart_.pc, true, CAUSE_INST_ACCESS_FAULT);
        clint_.tick();
        ++cycle_;
        return true;
    }

    DecodedInst d = decode(inst);
    bool ok = execute(d, out);
    clint_.tick();
    ++cycle_;
    return ok;
}

} // namespace emulator
