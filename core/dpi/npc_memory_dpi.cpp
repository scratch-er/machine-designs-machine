#include "npc_memory_dpi.h"
#include "emulator/memory.h"
#include <cstring>

namespace npc {

namespace {

emulator::Memory* g_memory = nullptr;

constexpr uint32_t SIZE_BYTE = 0;
constexpr uint32_t SIZE_HALF = 1;
constexpr uint32_t SIZE_WORD = 2;

// Packed response layout:
//   bits [31:0]  data
//   bit  [32]    access fault
//   bit  [33]    misaligned fault
uint64_t pack_resp(uint32_t data, bool access_fault, bool misaligned) {
    uint64_t r = static_cast<uint64_t>(data);
    if (access_fault) r |= (1ULL << 32);
    if (misaligned)   r |= (1ULL << 33);
    return r;
}

} // anonymous namespace

void npc_dpi_memory_set_backend(emulator::Memory* memory) {
    g_memory = memory;
}

} // namespace npc

extern "C" {

uint64_t npc_dpi_mem_fetch(uint32_t addr) {
    if (!npc::g_memory) return npc::pack_resp(0, true, false);
    if ((addr & 3u) != 0) return npc::pack_resp(0, false, true);
    uint32_t inst = 0;
    emulator::MemResult r = npc::g_memory->fetch(addr, inst);
    bool af = (r == emulator::MemResult::ACCESS_FAULT);
    bool ma = (r == emulator::MemResult::MISALIGNED);
    return npc::pack_resp(inst, af, ma);
}

uint64_t npc_dpi_mem_load(uint32_t addr, uint32_t size) {
    if (!npc::g_memory) return npc::pack_resp(0, true, false);
    uint32_t real_size = 0;
    switch (size) {
        case npc::SIZE_BYTE: real_size = 1; break;
        case npc::SIZE_HALF: real_size = 2; break;
        case npc::SIZE_WORD: real_size = 4; break;
        default: return npc::pack_resp(0, true, false);
    }
    // Alignment pre-check.
    if ((addr & (real_size - 1)) != 0) return npc::pack_resp(0, false, true);
    uint32_t data = 0;
    emulator::MemResult r = npc::g_memory->load(addr, real_size, data);
    bool af = (r == emulator::MemResult::ACCESS_FAULT);
    bool ma = (r == emulator::MemResult::MISALIGNED);
    return npc::pack_resp(data, af, ma);
}

void npc_dpi_mem_store(uint32_t addr, uint32_t size, uint32_t data) {
    if (!npc::g_memory) return;
    uint32_t real_size = 0;
    switch (size) {
        case npc::SIZE_BYTE: real_size = 1; break;
        case npc::SIZE_HALF: real_size = 2; break;
        case npc::SIZE_WORD: real_size = 4; break;
        default: return;
    }
    npc::g_memory->store(addr, real_size, data);
}

} // extern "C"
