#pragma once

#include "emulator/iss.h"
#include "emulator/hart.h"
#include "emulator/memory.h"
#include "emulator/clint.h"
#include "emulator/uart.h"
#include "emulator/config.h"
#include "emulator/trace.h"
#include "emulator/decoder.h"
#include <memory>
#include <array>
#include <vector>

namespace emulator {

class EmulatorISS : public ISS {
public:
    explicit EmulatorISS(Config cfg = {});

    void reset(u32 reset_addr = 0x20000000) override;
    bool step_cycle() override;
    bool step_inst(CommitEvent& out) override;

    u32 pc() const override { return hart_.pc; }
    u32 reg(u32 idx) const override { return hart_.read_reg(idx); }
    u32 csr(u32 addr) const override;

    u32 read_mem(u32 addr, u32 size) override;
    void write_mem(u32 addr, u32 size, u32 data) override;

    bool load_bin(const std::string& path, u32 addr) override;
    bool load_elf(const std::string& path) override;

    bool save_checkpoint(const std::string& path) override;
    bool load_checkpoint(const std::string& path) override;

    void set_log_level(int level) override { log_level_ = level; tracer_.set_level(level); }

    const Hart& hart() const { return hart_; }
    Hart& hart() { return hart_; }
    const Memory& memory() const { return memory_; }
    Memory& memory() { return memory_; }
    Clint& clint() { return clint_; }
    Uart& uart() { return uart_; }
    Tracer& tracer() { return tracer_; }
    const Config& config() const { return cfg_; }

    u64 cycle() const { return cycle_; }

    // Last N retired instructions (most recent first).
    std::vector<CommitEvent> last_events(size_t n = 64) const;

private:
    Config cfg_;
    Hart hart_;
    Memory memory_;
    Clint clint_;
    Uart uart_;
    Tracer tracer_;
    u64 cycle_ = 0;
    u64 retire_idx_ = 0;
    int log_level_ = 0;

    static constexpr size_t RING_SIZE = 64;
    std::array<CommitEvent, RING_SIZE> ring_buffer_;
    size_t ring_head_ = 0;
    bool ring_full_ = false;

    void push_ring(const CommitEvent& ev);

    bool execute(const DecodedInst& d, CommitEvent& out);
    CommitEvent commit(u32 pc, u32 inst, u32 rd, u32 rd_value, u32 next_pc,
                       bool exception = false, u32 cause = 0);
};

} // namespace emulator
