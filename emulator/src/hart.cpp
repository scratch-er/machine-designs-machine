#include "emulator/hart.h"

namespace emulator {

u32 Hart::read_reg(u32 idx) const {
    if (idx == 0) return 0;
    if (idx >= GPR_COUNT) return 0;  // Should have been trapped earlier
    return x[idx];
}

void Hart::write_reg(u32 idx, u32 value) {
    if (idx == 0 || idx >= GPR_COUNT) return;
    x[idx] = value;
}

bool Hart::read_csr(u32 addr, u32& out) const {
    switch (addr) {
        case CSR_MVENDORID: out = mvendorid; return true;
        case CSR_MARCHID:   out = marchid;   return true;
        case CSR_MSTATUS:   out = mstatus;   return true;
        case CSR_MTVEC:     out = mtvec;     return true;
        case CSR_MEPC:      out = mepc;      return true;
        case CSR_MCAUSE:    out = mcause;    return true;
        default: return false;
    }
}

bool Hart::write_csr(u32 addr, u32 value) {
    switch (addr) {
        case CSR_MSTATUS:
            // MPP hardcoded to M-mode (bits [12:11] = 11), others 0.
            mstatus = 0x00001800;
            return true;
        case CSR_MTVEC:
            mtvec = value & ~3u;  // aligned to 4 bytes
            return true;
        case CSR_MEPC:
            mepc = value & ~3u;   // IALIGN = 32
            return true;
        case CSR_MCAUSE:
            mcause = value;
            return true;
        case CSR_MVENDORID:
        case CSR_MARCHID:
            // Read-only; writes are ignored silently per RISC-V CSR rules.
            return true;
        default:
            return false;
    }
}

void Hart::take_exception(u32 cause, u32 tval) {
    (void)tval;  // tval not implemented in this core
    mepc = pc;
    mcause = cause;  // interrupt bit = 0 for exceptions
    pc = mtvec;
}

void Hart::reset(u32 reset_vector) {
    pc = reset_vector;
    x.fill(0);
    halted = false;
    mstatus = 0x00001800;  // MPP = M
    mepc = 0;
    mtvec = 0;
    mcause = 0;
}

} // namespace emulator
