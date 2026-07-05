#include "emulator/trace.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace emulator {

static std::string hex(u32 v) {
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

void Tracer::trace_inst(u64 retire_idx, const CommitEvent& ev) {
    if (level_ < 1) return;
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
    if (level_ < 2) return;
    std::cout << "  reg[" << idx << "] = " << hex(value) << "\n";
}

void Tracer::trace_mem_load(u32 addr, u32 size, u32 value) {
    if (level_ < 3) return;
    std::cout << "  load" << (size * 8) << " [" << hex(addr) << "] = " << hex(value) << "\n";
}

void Tracer::trace_mem_store(u32 addr, u32 size, u32 value) {
    if (level_ < 3) return;
    std::cout << "  store" << (size * 8) << " [" << hex(addr) << "] = " << hex(value) << "\n";
}

void Tracer::trace_exception(u32 cause, u32 tval) {
    if (level_ < 2) return;
    std::cout << "  exception cause=" << cause << " tval=" << hex(tval) << "\n";
}

void Tracer::log(int level, const std::string& msg) {
    if (level_ < level) return;
    std::cout << "[emulator] " << msg << "\n";
}

} // namespace emulator
