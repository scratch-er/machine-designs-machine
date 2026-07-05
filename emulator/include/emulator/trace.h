#pragma once

#include "emulator/commit.h"
#include <cstdint>
#include <string>

namespace emulator {

class Tracer {
public:
    Tracer() = default;
    explicit Tracer(int level) : level_(level) {}

    void set_level(int level) { level_ = level; }
    int level() const { return level_; }

    void trace_inst(u64 retire_idx, const CommitEvent& ev);
    void trace_reg(u32 idx, u32 value);
    void trace_mem_load(u32 addr, u32 size, u32 value);
    void trace_mem_store(u32 addr, u32 size, u32 value);
    void trace_exception(u32 cause, u32 tval);
    void log(int level, const std::string& msg);

private:
    int level_ = 0;
};

} // namespace emulator
