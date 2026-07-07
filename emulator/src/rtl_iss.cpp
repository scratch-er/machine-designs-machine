#include "emulator/rtl_iss.h"
#include "emulator/emulator_iss.h"
#include "emulator/memory.h"
#include <verilated.h>
#include <verilated_vcd_c.h>
#include <fstream>
#include <iostream>
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

        top_->clock = 0;
        top_->reset = 1;
        reset(cfg_.reset_vector);
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
        write_seen_this_cycle_ = false;

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
        drive_axi_memory();
        top_->eval();
        return top_->debug_reg_data;
    }

    u32 csr(u32 addr) const {
        top_->debug_csr_addr = addr & 0xFFF;
        top_->eval();
        drive_axi_memory();
        top_->eval();
        return top_->debug_csr_data;
    }

    u32 read_mem(u32 addr, u32 size) {
        u32 value = 0;
        // Directly use the simulation memory model; do not route debug reads
        // back through the DUT's AXI master.
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
        std::cerr << "warning: RTL checkpoints are not implemented\n";
        return false;
    }

    bool load_checkpoint(const std::string& path) {
        (void)path;
        std::cerr << "warning: RTL checkpoints are not implemented\n";
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
    mutable Memory memory_;
    std::unique_ptr<VerilatedContext> contextp_;
    std::unique_ptr<Vnpc_core> top_;
    VerilatedVcdC* trace_ = nullptr;
    u64 cycle_ = 0;
    u32 commit_timeout_cycles_ = 10000;
    int log_level_ = 0;
    mutable bool write_seen_this_cycle_ = false;
    mutable MemResult write_result_this_cycle_ = MemResult::OK;

    static u32 axi_size_bytes(u32 axsize) {
        switch (axsize) {
            case 0: return 1;
            case 1: return 2;
            case 2: return 4;
            default: return 0;
        }
    }

    static u32 extract_narrow_write_data(u32 addr, u32 size, u32 wdata) {
        u32 shift = (addr & 0x3u) * 8u;
        if (size == 1) return (wdata >> shift) & 0xFFu;
        if (size == 2) return (wdata >> shift) & 0xFFFFu;
        return wdata;
    }

    void drive_axi_memory() const {
        top_->io_master_awready = 1;
        top_->io_master_wready = 1;
        top_->io_master_arready = 1;

        top_->io_master_rvalid = 0;
        top_->io_master_rresp = 0;
        top_->io_master_rdata = 0;
        top_->io_master_rlast = 0;
        top_->io_master_rid = top_->io_master_arid;

        if (top_->io_master_arvalid) {
            u32 addr = top_->io_master_araddr;
            u32 size = axi_size_bytes(top_->io_master_arsize);
            u32 data = 0;
            MemResult result = MemResult::ACCESS_FAULT;

            if (size != 0) {
                if (top_->io_master_arid == 0 && size == 4) {
                    result = memory_.fetch(addr, data);
                } else {
                    result = memory_.load(addr, size, data);
                }
            }

            top_->io_master_rvalid = 1;
            top_->io_master_rresp = result == MemResult::OK ? 0 : 2;
            top_->io_master_rdata = data;
            top_->io_master_rlast = 1;
            top_->io_master_rid = top_->io_master_arid;
        }

        top_->io_master_bvalid = 0;
        top_->io_master_bresp = 0;
        top_->io_master_bid = top_->io_master_awid;

        if (top_->io_master_awvalid && top_->io_master_wvalid) {
            if (!write_seen_this_cycle_) {
                u32 addr = top_->io_master_awaddr;
                u32 size = axi_size_bytes(top_->io_master_awsize);
                if (size == 0) {
                    write_result_this_cycle_ = MemResult::ACCESS_FAULT;
                } else {
                    u32 data = extract_narrow_write_data(addr, size, top_->io_master_wdata);
                    write_result_this_cycle_ = memory_.store(addr, size, data);
                }
                write_seen_this_cycle_ = true;
            }
            top_->io_master_bvalid = 1;
            top_->io_master_bresp = write_result_this_cycle_ == MemResult::OK ? 0 : 2;
            top_->io_master_bid = top_->io_master_awid;
        }
    }

    void eval_with_axi() {
        top_->eval();
        drive_axi_memory();
        top_->eval();
    }

    void toggle_clock() {
        write_seen_this_cycle_ = false;

        top_->clock = 0;
        eval_with_axi();
        if (trace_) trace_->dump(contextp_->time());
        contextp_->timeInc(1);

        top_->clock = 1;
        eval_with_axi();
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
