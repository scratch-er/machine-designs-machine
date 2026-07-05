#pragma once

#include "emulator/common.h"
#include <array>
#include <stdexcept>

namespace emulator {

struct Hart {
    u32 pc = 0;
    std::array<u32, GPR_COUNT> x{};
    bool halted = false;

    // CSRs
    u32 mstatus = 0;
    u32 mepc = 0;
    u32 mtvec = 0;
    u32 mcause = 0;

    // Read-only CSRs
    static constexpr u32 mvendorid = 0;
    static constexpr u32 marchid = 0;

    // GPR access
    u32 read_reg(u32 idx) const;
    void write_reg(u32 idx, u32 value);

    // CSR access; returns false if CSR is unimplemented
    bool read_csr(u32 addr, u32& out) const;
    bool write_csr(u32 addr, u32 value);

    // Exception entry
    void take_exception(u32 cause, u32 tval);

    void reset(u32 reset_vector);
};

} // namespace emulator
