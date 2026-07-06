#include "emulator/difftest.h"
#include <sstream>
#include <iomanip>

namespace emulator {

static std::string hex(u32 v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

bool Difftest::events_match(const CommitEvent& a, const CommitEvent& b, std::string& why) {
    if (a.pc != b.pc) {
        why = "pc mismatch ref=" + hex(a.pc) + " dut=" + hex(b.pc);
        return false;
    }
    if (a.exception != b.exception) {
        why = "exception flag mismatch";
        return false;
    }
    if (a.exception) {
        if (a.cause != b.cause) {
            why = "exception cause mismatch ref=" + std::to_string(a.cause) +
                  " dut=" + std::to_string(b.cause);
            return false;
        }
    } else {
        if (a.inst != b.inst) {
            why = "inst mismatch ref=" + hex(a.inst) + " dut=" + hex(b.inst);
            return false;
        }
        if (a.rd != b.rd) {
            why = "rd mismatch ref=" + std::to_string(a.rd) + " dut=" + std::to_string(b.rd);
            return false;
        }
        if (a.rd != 0 && a.rd_value != b.rd_value) {
            why = "rd_value mismatch rd=" + std::to_string(a.rd) +
                  " ref=" + hex(a.rd_value) + " dut=" + hex(b.rd_value);
            return false;
        }
    }
    if (a.next_pc != b.next_pc) {
        why = "next_pc mismatch ref=" + hex(a.next_pc) + " dut=" + hex(b.next_pc);
        return false;
    }
    return true;
}

bool Difftest::run(uint64_t max_steps) {
    mismatch_.clear();
    retire_idx_ = 0;
    for (uint64_t i = 0; i < max_steps; ++i) {
        CommitEvent ref_ev, dut_ev;
        bool r = ref_->step_inst(ref_ev);
        bool d = dut_->step_inst(dut_ev);
        if (!r && !d) {
            // Both halted. Treat as matching end state.
            return true;
        }
        if (r != d) {
            mismatch_ = (r ? "dut halted unexpectedly" : "reference halted unexpectedly");
            return false;
        }
        std::string why;
        if (!events_match(ref_ev, dut_ev, why)) {
            std::ostringstream oss;
            oss << "mismatch at retire_idx=" << i << " " << why;
            mismatch_ = oss.str();
            return false;
        }
        retire_idx_ = i + 1;
    }
    return true;
}

} // namespace emulator
