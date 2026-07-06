#pragma once

#include "emulator/commit.h"
#include <cstdint>
#include <string>

namespace emulator {

enum class TraceFilterKind {
    ALL,
    BRANCHES,
    LOADS,
    STORES,
    EXCEPTIONS,
    REG,
    PC_RANGE,
};

struct TraceFilter {
    bool enabled = false;
    TraceFilterKind kind = TraceFilterKind::ALL;
    u32 reg_idx = 0;
    u32 pc_lo = 0;
    u32 pc_hi = 0;
};

class Tracer {
public:
    Tracer() = default;
    explicit Tracer(int level) : level_(level) {}

    void set_level(int level) { level_ = level; }
    int level() const { return level_; }

    void set_filter(const TraceFilter& f) { filter_ = f; }
    const TraceFilter& filter() const { return filter_; }

    void trace_inst(u64 retire_idx, const CommitEvent& ev);
    void trace_reg(u32 idx, u32 value);
    void trace_mem_load(u32 addr, u32 size, u32 value);
    void trace_mem_store(u32 addr, u32 size, u32 value);
    void trace_exception(u32 cause, u32 tval);
    void log(int level, const std::string& msg);

private:
    int level_ = 0;
    TraceFilter filter_;

    bool should_print_inst(const CommitEvent& ev) const;
    bool is_branch_opcode(u32 opcode) const;
};

} // namespace emulator
