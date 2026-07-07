// Minimal Verilator testbench for npc_core.
// Loads a small program into the DPI-C memory and runs until the core halts
// or a timeout is reached.

#include "emulator/config.h"
#include "emulator/memory.h"
#include "npc_memory_dpi.h"
#include <verilated.h>
#include <verilated_vcd_c.h>
#include "Vnpc_core.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using emulator::Config;
using emulator::Memory;

static constexpr uint32_t RESET_VECTOR = 0x20000000;

int main(int argc, char** argv) {
    VerilatedContext context;
    context.debug(0);
    context.timeunit(-12);
    context.timeprecision(-12);

    Vnpc_core top(&context);

    // Optional VCD trace.
    VerilatedVcdC* trace = nullptr;
    const char* trace_path = std::getenv("NPC_TRACE");
    if (trace_path) {
        trace = new VerilatedVcdC;
        top.trace(trace, 99);
        trace->open(trace_path);
    }

    // Create and register the memory backend.
    Config cfg;
    Memory memory(cfg);
    npc::npc_dpi_memory_set_backend(&memory);

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
        top.clock = 0;
        top.eval();
        if (trace) trace->dump(context.time());
        context.timeInc(1);
        top.clock = 1;
        top.eval();
        if (trace) trace->dump(context.time());
        context.timeInc(1);
    }
    top.reset = 0;

    const int MAX_CYCLES = 1000;
    int commits = 0;
    for (int i = 0; i < MAX_CYCLES; ++i) {
        top.clock = 0;
        top.eval();
        if (trace) trace->dump(context.time());
        context.timeInc(1);
        top.clock = 1;
        top.eval();
        if (trace) trace->dump(context.time());
        context.timeInc(1);

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
