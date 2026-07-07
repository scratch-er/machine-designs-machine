// Minimal Verilator testbench for npc_core.
// Loads a small program into the AXI memory model and runs until the core halts
// or a timeout is reached.

#include "emulator/config.h"
#include "emulator/memory.h"
#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vnpc_core.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using emulator::Config;
using emulator::MemResult;
using emulator::Memory;

static constexpr uint32_t RESET_VECTOR = 0x20000000;

static uint32_t axi_size_bytes(uint32_t axsize) {
    switch (axsize) {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        default: return 0;
    }
}

static uint32_t extract_narrow_write_data(uint32_t addr, uint32_t size, uint32_t wdata) {
    uint32_t shift = (addr & 0x3u) * 8u;
    if (size == 1) return (wdata >> shift) & 0xFFu;
    if (size == 2) return (wdata >> shift) & 0xFFFFu;
    return wdata;
}

static bool write_seen_this_cycle = false;
static MemResult write_result_this_cycle = MemResult::OK;

static void drive_axi_memory(Vnpc_core& top, Memory& memory) {
    top.io_master_awready = 1;
    top.io_master_wready = 1;
    top.io_master_arready = 1;

    top.io_master_rvalid = 0;
    top.io_master_rresp = 0;
    top.io_master_rdata = 0;
    top.io_master_rlast = 0;
    top.io_master_rid = top.io_master_arid;

    if (top.io_master_arvalid) {
        uint32_t addr = top.io_master_araddr;
        uint32_t size = axi_size_bytes(top.io_master_arsize);
        uint32_t data = 0;
        MemResult result = MemResult::ACCESS_FAULT;

        if (size != 0) {
            if (top.io_master_arid == 0 && size == 4) {
                result = memory.fetch(addr, data);
            } else {
                result = memory.load(addr, size, data);
            }
        }

        top.io_master_rvalid = 1;
        top.io_master_rresp = result == MemResult::OK ? 0 : 2;
        top.io_master_rdata = data;
        top.io_master_rlast = 1;
        top.io_master_rid = top.io_master_arid;
    }

    top.io_master_bvalid = 0;
    top.io_master_bresp = 0;
    top.io_master_bid = top.io_master_awid;

    if (top.io_master_awvalid && top.io_master_wvalid) {
        if (!write_seen_this_cycle) {
            uint32_t addr = top.io_master_awaddr;
            uint32_t size = axi_size_bytes(top.io_master_awsize);
            if (size == 0) {
                write_result_this_cycle = MemResult::ACCESS_FAULT;
            } else {
                uint32_t data = extract_narrow_write_data(addr, size, top.io_master_wdata);
                write_result_this_cycle = memory.store(addr, size, data);
            }
            write_seen_this_cycle = true;
        }
        top.io_master_bvalid = 1;
        top.io_master_bresp = write_result_this_cycle == MemResult::OK ? 0 : 2;
        top.io_master_bid = top.io_master_awid;
    }
}

static void eval_with_axi(Vnpc_core& top, Memory& memory) {
    top.eval();
    drive_axi_memory(top, memory);
    top.eval();
}

static void tick(VerilatedContext& context, Vnpc_core& top, Memory& memory, VerilatedVcdC* trace) {
    write_seen_this_cycle = false;

    top.clock = 0;
    eval_with_axi(top, memory);
    if (trace) trace->dump(context.time());
    context.timeInc(1);

    top.clock = 1;
    eval_with_axi(top, memory);
    if (trace) trace->dump(context.time());
    context.timeInc(1);
}

int main(int argc, char** argv) {
    VerilatedContext context;
    context.debug(0);
    context.timeunit(-12);
    context.timeprecision(-12);
    context.commandArgs(argc, argv);

    Vnpc_core top(&context);

    // Optional VCD trace.
    VerilatedVcdC* trace = nullptr;
    const char* trace_path = std::getenv("NPC_TRACE");
    if (trace_path) {
        trace = new VerilatedVcdC;
        top.trace(trace, 99);
        trace->open(trace_path);
    }

    Config cfg;
    Memory memory(cfg);

    // Write a tiny program:
    //   0x20000000: addi x1, x0, 5
    //   0x20000004: addi x2, x1, 3
    //   0x20000008: ebreak
    uint8_t prog[12] = {
        0x93, 0x00, 0x50, 0x00,  // addi x1, x0, 5
        0x13, 0x01, 0x30, 0x00,  // addi x2, x1, 3
        0x73, 0x00, 0x10, 0x00,  // ebreak
    };
    memory.write_ram(RESET_VECTOR, prog, sizeof(prog));

    top.clock = 0;
    top.reset = 1;

    const int RESET_CYCLES = 10;
    for (int i = 0; i < RESET_CYCLES; ++i) {
        tick(context, top, memory, trace);
    }
    top.reset = 0;

    const int MAX_CYCLES = 1000;
    int commits = 0;
    for (int i = 0; i < MAX_CYCLES; ++i) {
        tick(context, top, memory, trace);

        if (top.commit_valid) {
            ++commits;
            std::printf("commit pc=%08x inst=%08x rd=%2d rv=%08x exc=%d cause=%2d npc=%08x\n",
                        top.commit_pc, top.commit_inst, top.commit_rd,
                        top.commit_rd_value, top.commit_exception,
                        top.commit_cause, top.commit_next_pc);
            if (top.commit_exception) {
                std::printf("exception taken, stopping\n");
                break;
            }
        }
    }

    if (trace) {
        trace->close();
        delete trace;
    }

    std::printf("total commits: %d\n", commits);
    return 0;
}
