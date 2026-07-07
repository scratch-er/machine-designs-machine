#include "emulator/difftest.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace emulator {

static std::string hex(u32 v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

Difftest::Difftest(ISS* ref, ISS* dut)
    : ref_(ref), dut_(dut) {}

Difftest::Difftest(std::unique_ptr<ISS> ref, std::unique_ptr<ISS> dut)
    : owned_ref_(std::move(ref)),
      owned_dut_(std::move(dut)),
      ref_(owned_ref_.get()),
      dut_(owned_dut_.get()) {}

void Difftest::set_failure(const std::string& why) {
    if (mismatch_.empty()) {
        mismatch_ = why;
    }
    failed_ = true;
    stopped_ = true;
    std::cerr << "difftest failed: " << mismatch_ << "\n";
}

void Difftest::reset(u32 reset_addr) {
    ref_->reset(reset_addr);
    dut_->reset(reset_addr);
    mismatch_.clear();
    retire_idx_ = 0;
    stopped_ = false;
    failed_ = false;
}

bool Difftest::step_cycle() {
    CommitEvent ev;
    return step_inst(ev);
}

bool Difftest::step_inst(CommitEvent& out) {
    if (stopped_) return false;

    CommitEvent ref_ev;
    CommitEvent dut_ev;
    bool ref_ok = ref_->step_inst(ref_ev);
    bool dut_ok = dut_->step_inst(dut_ev);

    if (!ref_ok && !dut_ok) {
        stopped_ = true;
        return false;
    }
    if (ref_ok != dut_ok) {
        std::ostringstream oss;
        oss << "mismatch at retire_idx=" << retire_idx_ << " "
            << (ref_ok ? "dut halted unexpectedly" : "reference halted unexpectedly");
        set_failure(oss.str());
        return false;
    }

    std::string why;
    if (!events_match(ref_ev, dut_ev, why)) {
        std::ostringstream oss;
        oss << "mismatch at retire_idx=" << retire_idx_ << " " << why;
        set_failure(oss.str());
        return false;
    }

    out = ref_ev;
    ++retire_idx_;
    return true;
}

u32 Difftest::pc() const { return ref_->pc(); }
u32 Difftest::reg(u32 idx) const { return ref_->reg(idx); }
u32 Difftest::csr(u32 addr) const { return ref_->csr(addr); }

u32 Difftest::read_mem(u32 addr, u32 size) {
    return ref_->read_mem(addr, size);
}

void Difftest::write_mem(u32 addr, u32 size, u32 data) {
    ref_->write_mem(addr, size, data);
    dut_->write_mem(addr, size, data);
}

bool Difftest::load_bin(const std::string& path, u32 addr) {
    bool ref_ok = ref_->load_bin(path, addr);
    bool dut_ok = dut_->load_bin(path, addr);
    if (ref_ok != dut_ok) {
        set_failure(std::string("load_bin result mismatch for ") + path);
    }
    return ref_ok && dut_ok;
}

bool Difftest::load_elf(const std::string& path) {
    bool ref_ok = ref_->load_elf(path);
    bool dut_ok = dut_->load_elf(path);
    if (ref_ok != dut_ok) {
        set_failure(std::string("load_elf result mismatch for ") + path);
    }
    return ref_ok && dut_ok;
}

bool Difftest::save_checkpoint(const std::string& path) {
    (void)path;
    std::cerr << "warning: difftest checkpoints are not implemented\n";
    return false;
}

bool Difftest::load_checkpoint(const std::string& path) {
    (void)path;
    std::cerr << "warning: difftest checkpoints are not implemented\n";
    return false;
}

void Difftest::set_log_level(int level) {
    ref_->set_log_level(level);
    dut_->set_log_level(level);
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
    for (uint64_t i = 0; i < max_steps; ++i) {
        CommitEvent ev;
        if (!step_inst(ev)) {
            return !failed_;
        }
    }
    return true;
}

} // namespace emulator
