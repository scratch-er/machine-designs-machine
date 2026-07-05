#pragma once

#include "emulator/common.h"
#include "emulator/commit.h"
#include <string>

namespace emulator {

class ISS {
public:
    virtual ~ISS() = default;

    virtual void reset(u32 reset_addr = 0x20000000) = 0;

    // Advance one clock cycle.
    virtual bool step_cycle() = 0;

    // Advance until one instruction retires (or an exception is taken).
    virtual bool step_inst(CommitEvent& out) = 0;

    // Architectural state queries.
    virtual u32 pc() const = 0;
    virtual u32 reg(u32 idx) const = 0;
    virtual u32 csr(u32 addr) const = 0;

    // Memory access used by shell and tests.
    virtual u32 read_mem(u32 addr, u32 size) = 0;
    virtual void write_mem(u32 addr, u32 size, u32 data) = 0;

    // Program loading.
    virtual bool load_bin(const std::string& path, u32 addr) = 0;
    virtual bool load_elf(const std::string& path) = 0;

    // Checkpointing.
    virtual bool save_checkpoint(const std::string& path) = 0;
    virtual bool load_checkpoint(const std::string& path) = 0;

    // Logging.
    virtual void set_log_level(int level) = 0;
};

} // namespace emulator
