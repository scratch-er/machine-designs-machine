#pragma once

#include "emulator/iss.h"
#include <string>

namespace emulator {

// Drives a reference ISS and a DUT ISS instruction-by-instruction and reports
// the first point of architectural divergence.
class Difftest {
public:
    Difftest(ISS* ref, ISS* dut) : ref_(ref), dut_(dut) {}

    // Run up to max_steps retired instructions. Returns true if both models halt
    // for the same reason or reach max_steps without mismatch. Returns false on
    // divergence, asymmetric halt, or step failure.
    bool run(uint64_t max_steps);

    const std::string& last_mismatch() const { return mismatch_; }
    uint64_t retire_index() const { return retire_idx_; }

private:
    ISS* ref_;
    ISS* dut_;
    std::string mismatch_;
    uint64_t retire_idx_ = 0;

    static bool events_match(const CommitEvent& a, const CommitEvent& b, std::string& why);
};

} // namespace emulator
