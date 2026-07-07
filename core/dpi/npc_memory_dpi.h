// C++ side of the NPC DPI-C memory interface.
//
// The RTL calls npc_dpi_mem_{fetch,load,store}. These functions delegate to an
// emulator::Memory object that is registered by the Verilator testbench/adapter.
//
// Reads are pure (no side effects) so they can be called from Verilog
// combinational logic. Writes are only called from clocked logic.

#ifndef NPC_MEMORY_DPI_H
#define NPC_MEMORY_DPI_H

#include <cstdint>

namespace emulator {
class Memory;
} // namespace emulator

namespace npc {

// Register the emulator Memory object that backs the DPI-C memory.
// This must be called before the RTL performs any memory access.
void npc_dpi_memory_set_backend(emulator::Memory* memory);

} // namespace npc

extern "C" {

// Fetch a 32-bit instruction word. Returns packed {fault, data} where fault is
// bit 32. A fault occurs on misaligned or out-of-range accesses.
uint64_t npc_dpi_mem_fetch(uint32_t addr);

// Load 1, 2, or 4 bytes (size = 0/1/2). Returns packed {fault, data}.
uint64_t npc_dpi_mem_load(uint32_t addr, uint32_t size);

// Store 1, 2, or 4 bytes (size = 0/1/2). The DPI layer does not return a fault
// code directly; it is the caller's responsibility to ensure the access is
// valid. Out-of-range writes are silently ignored.
void npc_dpi_mem_store(uint32_t addr, uint32_t size, uint32_t data);

} // extern "C"

#endif // NPC_MEMORY_DPI_H
