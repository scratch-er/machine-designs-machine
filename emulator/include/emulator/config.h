#pragma once

#include "emulator/common.h"
#include <string>

namespace emulator {

struct Config {
    u32 reset_vector = 0x20000000;
    u32 clint_base   = 0x02000000;
    u32 clint_size   = 0x00010000;
    u32 uart_base    = 0x10000000;
    u32 ram_base     = 0x20000000;
    u32 ram_size     = 0x00100000;  // 1 MiB
    bool strict_mem  = false;
    u32 commit_timeout_cycles = 10000;
    u64 max_cycles   = 0;        // 0 = unlimited
    u32 max_pc_stuck = 0;        // 0 = disabled
    int log_level    = 0;

    // Static validation / helpers
    bool in_ram(u32 addr) const;
    bool in_clint(u32 addr) const;
    bool in_uart(u32 addr) const;
    u32 clint_mtime_offset() const { return 0xBFF8; }
    u32 clint_mtimeh_offset() const { return 0xBFFC; }
};

} // namespace emulator
