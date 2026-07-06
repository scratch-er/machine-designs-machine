#include "emulator/trace.h"
#include "emulator/common.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace emulator {

static std::string hex(u32 v) {
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

static u32 opcode(u32 inst) {
    return inst & 0x7F;
}

bool Tracer::is_branch_opcode(u32 opc) const {
    return opc == OPCODE_JAL || opc == OPCODE_JALR || opc == OPCODE_BRANCH;
}

bool Tracer::should_print_inst(const CommitEvent& ev) const {
    if (!filter_.enabled) return level_ >= 1;
    switch (filter_.kind) {
        case TraceFilterKind::ALL:        return true;
        case TraceFilterKind::BRANCHES:   return is_branch_opcode(opcode(ev.inst));
        case TraceFilterKind::LOADS:      return opcode(ev.inst) == OPCODE_LOAD;
        case TraceFilterKind::STORES:     return opcode(ev.inst) == OPCODE_STORE;
        case TraceFilterKind::EXCEPTIONS: return ev.exception;
        case TraceFilterKind::REG:        return ev.rd == filter_.reg_idx && ev.rd != 0;
        case TraceFilterKind::PC_RANGE:   return ev.pc >= filter_.pc_lo && ev.pc <= filter_.pc_hi;
    }
    return false;
}

void Tracer::trace_inst(u64 retire_idx, const CommitEvent& ev) {
    if (!should_print_inst(ev)) return;
    std::cout << "R=" << retire_idx
              << " C=" << ev.cycle
              << " PC=" << hex(ev.pc)
              << " I=" << hex(ev.inst)
              << " RD=" << ev.rd
              << " RV=" << hex(ev.rd_value)
              << " NPC=" << hex(ev.next_pc)
              << " EXC=" << (ev.exception ? 1 : 0)
              << " CAUSE=" << ev.cause
              << "\n";
}

void Tracer::trace_reg(u32 idx, u32 value) {
    if (filter_.enabled) {
        if (filter_.kind != TraceFilterKind::ALL &&
            (filter_.kind != TraceFilterKind::REG || idx != filter_.reg_idx)) {
            return;
        }
    }
    if (level_ < 2) return;
    std::cout << "  reg[" << idx << "] = " << hex(value) << "\n";
}

void Tracer::trace_mem_load(u32 addr, u32 size, u32 value) {
    if (filter_.enabled) {
        if (filter_.kind != TraceFilterKind::ALL && filter_.kind != TraceFilterKind::LOADS) {
            return;
        }
    }
    if (level_ < 3) return;
    std::cout << "  load" << (size * 8) << " [" << hex(addr) << "] = " << hex(value) << "\n";
}

void Tracer::trace_mem_store(u32 addr, u32 size, u32 value) {
    if (filter_.enabled) {
        if (filter_.kind != TraceFilterKind::ALL && filter_.kind != TraceFilterKind::STORES) {
            return;
        }
    }
    if (level_ < 3) return;
    std::cout << "  store" << (size * 8) << " [" << hex(addr) << "] = " << hex(value) << "\n";
}

void Tracer::trace_exception(u32 cause, u32 tval) {
    if (filter_.enabled) {
        if (filter_.kind != TraceFilterKind::ALL && filter_.kind != TraceFilterKind::EXCEPTIONS) {
            return;
        }
    }
    if (level_ < 2) return;
    std::cout << "  exception cause=" << cause << " tval=" << hex(tval) << "\n";
}

void Tracer::log(int level, const std::string& msg) {
    if (level_ < level) return;
    std::cout << "[emulator] " << msg << "\n";
}

} // namespace emulator
