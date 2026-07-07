#pragma once

#include "emulator/iss.h"
#include <memory>
#include <string>

namespace emulator {

// Drives a reference ISS and a DUT ISS instruction-by-instruction and reports
// the first point of architectural divergence. Difftest also implements ISS so
// the shell can control it with the same command sequence used for single models.
class Difftest : public ISS {
public:
    Difftest(ISS* ref, ISS* dut);
    Difftest(std::unique_ptr<ISS> ref, std::unique_ptr<ISS> dut);

    void reset(u32 reset_addr = 0x20000000) override;
    bool step_cycle() override;
    bool step_inst(CommitEvent& out) override;

    u32 pc() const override;
    u32 reg(u32 idx) const override;
    u32 csr(u32 addr) const override;

    u32 read_mem(u32 addr, u32 size) override;
    void write_mem(u32 addr, u32 size, u32 data) override;

    bool load_bin(const std::string& path, u32 addr) override;
    bool load_elf(const std::string& path) override;

    bool save_checkpoint(const std::string& path) override;
    bool load_checkpoint(const std::string& path) override;

    void set_log_level(int level) override;

    // Run up to max_steps retired instructions. Returns true if both models halt
    // for the same reason or reach max_steps without mismatch. Returns false on
    // divergence, asymmetric halt, or step failure.
    bool run(uint64_t max_steps);

    const std::string& last_mismatch() const { return mismatch_; }
    uint64_t retire_index() const { return retire_idx_; }
    bool failed() const { return failed_; }

private:
    std::unique_ptr<ISS> owned_ref_;
    std::unique_ptr<ISS> owned_dut_;
    ISS* ref_ = nullptr;
    ISS* dut_ = nullptr;
    std::string mismatch_;
    uint64_t retire_idx_ = 0;
    bool stopped_ = false;
    bool failed_ = false;

    void set_failure(const std::string& why);
    static bool events_match(const CommitEvent& a, const CommitEvent& b, std::string& why);
};

} // namespace emulator
