#include "emulator/rtl_iss.h"
#include "emulator/emulator_iss.h"
#include "emulator/memory.h"
#include "npc_memory_dpi.h"
#include <verilated.h>
#include <verilated_vcd_c.h>
#include <fstream>
#include <iterator>
#include <vector>
#include "Vnpc_core.h"

namespace emulator {

class RtlISS::Impl {
public:
    explicit Impl(Config cfg)
        : cfg_(std::move(cfg)),
          memory_(cfg_) {
        contextp_ = std::make_unique<VerilatedContext>();
        contextp_->debug(0);
        contextp_->randReset(2);
        contextp_->timeunit(-12);  // 1 ps
        contextp_->timeprecision(-12);

        top_ = std::make_unique<Vnpc_core>(contextp_.get());

        npc::npc_dpi_memory_set_backend(&memory_);

        top_->clock = 0;
        top_->reset = 1;
    }

    ~Impl() {
        if (trace_) {
            top_->trace(trace_, 99);
            trace_->close();
        }
        top_.reset();
        contextp_.reset();
    }

    void reset(u32 reset_addr) {
        cycle_ = 0;
        commit_timeout_cycles_ = cfg_.commit_timeout_cycles;

        // Hold reset for a few cycles.
        top_->reset = 1;
        for (int i = 0; i < 10; ++i) {
            toggle_clock();
        }
        top_->reset = 0;

        // The reset vector is hardwired as a parameter; we cannot change it at
        // runtime without re-instantiating. For now, the RTL uses RESET_VECTOR
        // and this method ignores reset_addr unless it matches.
        (void)reset_addr;
    }

    bool step_cycle() {
        toggle_clock();
        return true;
    }

    bool step_inst(CommitEvent& out) {
        u64 start_cycle = cycle_;
        while (true) {
            if (!step_cycle()) return false;
            if (top_->commit_valid) {
                out.cycle = cycle_;
                out.pc = top_->commit_pc;
                out.inst = top_->commit_inst;
                out.rd = top_->commit_rd;
                out.rd_value = top_->commit_rd_value;
                out.exception = top_->commit_exception;
                out.cause = top_->commit_cause;
                out.next_pc = top_->commit_next_pc;
                return true;
            }
            if (cycle_ - start_cycle > commit_timeout_cycles_) {
                return false;
            }
        }
    }

    u32 pc() const {
        return top_->debug_pc;
    }

    u32 reg(u32 idx) const {
        top_->debug_reg_addr = idx & 0xF;
        top_->eval();
        return top_->debug_reg_data;
    }

    u32 csr(u32 addr) const {
        top_->debug_csr_addr = addr & 0xFFF;
        top_->eval();
        return top_->debug_csr_data;
    }

    u32 read_mem(u32 addr, u32 size) {
        u32 value = 0;
        // Directly use the emulator memory model; do not route through RTL.
        memory_.load(addr, size, value);
        return value;
    }

    void write_mem(u32 addr, u32 size, u32 data) {
        memory_.store(addr, size, data);
    }

    bool load_bin(const std::string& path, u32 addr) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        std::vector<u8> data((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        memory_.write_ram(addr, data.data(), data.size());
        return true;
    }

    bool load_elf(const std::string& path) {
        // Parse the ELF with EmulatorISS and copy its RAM image into our
        // Memory backend. This avoids duplicating the ELF loader.
        EmulatorISS loader(cfg_);
        if (!loader.load_elf(path)) return false;

        const auto& src_ram = loader.memory().ram();
        memory_.write_ram(cfg_.ram_base, src_ram.data(), src_ram.size());
        return true;
    }

    bool save_checkpoint(const std::string& path) {
        (void)path;
        // Full RTL checkpoint restore is deferred. Reading architectural state
        // back through debug ports is possible but not yet implemented.
        return false;
    }

    bool load_checkpoint(const std::string& path) {
        (void)path;
        return false;
    }

    void set_log_level(int level) {
        log_level_ = level;
    }

    u64 cycle() const { return cycle_; }

    void enable_trace(const std::string& path) {
        if (trace_) return;
        trace_ = new VerilatedVcdC;
        top_->trace(trace_, 99);
        trace_->open(path.c_str());
    }

    void disable_trace() {
        if (!trace_) return;
        trace_->close();
        delete trace_;
        trace_ = nullptr;
    }

private:
    Config cfg_;
    Memory memory_;
    std::unique_ptr<VerilatedContext> contextp_;
    std::unique_ptr<Vnpc_core> top_;
    VerilatedVcdC* trace_ = nullptr;
    u64 cycle_ = 0;
    u32 commit_timeout_cycles_ = 10000;
    int log_level_ = 0;

    void toggle_clock() {
        top_->clock = 0;
        top_->eval();
        if (trace_) trace_->dump(contextp_->time());
        contextp_->timeInc(1);

        top_->clock = 1;
        top_->eval();
        if (trace_) trace_->dump(contextp_->time());
        contextp_->timeInc(1);

        ++cycle_;
    }
};

RtlISS::RtlISS(Config cfg)
    : impl_(std::make_unique<Impl>(std::move(cfg))) {}

RtlISS::~RtlISS() = default;

void RtlISS::reset(u32 reset_addr) { impl_->reset(reset_addr); }
bool RtlISS::step_cycle() { return impl_->step_cycle(); }
bool RtlISS::step_inst(CommitEvent& out) { return impl_->step_inst(out); }
u32 RtlISS::pc() const { return impl_->pc(); }
u32 RtlISS::reg(u32 idx) const { return impl_->reg(idx); }
u32 RtlISS::csr(u32 addr) const { return impl_->csr(addr); }
u32 RtlISS::read_mem(u32 addr, u32 size) { return impl_->read_mem(addr, size); }
void RtlISS::write_mem(u32 addr, u32 size, u32 data) { impl_->write_mem(addr, size, data); }
bool RtlISS::load_bin(const std::string& path, u32 addr) { return impl_->load_bin(path, addr); }
bool RtlISS::load_elf(const std::string& path) { return impl_->load_elf(path); }
bool RtlISS::save_checkpoint(const std::string& path) { return impl_->save_checkpoint(path); }
bool RtlISS::load_checkpoint(const std::string& path) { return impl_->load_checkpoint(path); }
void RtlISS::set_log_level(int level) { impl_->set_log_level(level); }
u64 RtlISS::cycle() const { return impl_->cycle(); }
void RtlISS::enable_trace(const std::string& path) { impl_->enable_trace(path); }
void RtlISS::disable_trace() { impl_->disable_trace(); }

} // namespace emulator
