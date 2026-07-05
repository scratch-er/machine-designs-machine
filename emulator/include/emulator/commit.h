#pragma once

#include "emulator/common.h"

namespace emulator {

struct CommitEvent {
    u64 cycle = 0;
    u32 pc = 0;
    u32 inst = 0;
    u32 rd = 0;
    u32 rd_value = 0;
    bool exception = false;
    u32 cause = 0;
    u32 next_pc = 0;

    bool operator==(const CommitEvent& other) const {
        return cycle == other.cycle && pc == other.pc && inst == other.inst &&
               rd == other.rd && rd_value == other.rd_value &&
               exception == other.exception && cause == other.cause &&
               next_pc == other.next_pc;
    }
    bool operator!=(const CommitEvent& other) const { return !(*this == other); }
};

} // namespace emulator
