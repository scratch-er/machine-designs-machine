#pragma once

#include "emulator/common.h"
#include "emulator/config.h"
#include <vector>
#include <stdexcept>

namespace emulator {

// Result codes for memory accesses.
enum class MemResult {
    OK,
    MISALIGNED,
    ACCESS_FAULT,
};

class Memory {
public:
    explicit Memory(const Config& cfg);

    // Load/store helpers return result code. On OK, *out is set.
    MemResult load(u32 addr, u32 size, u32& out) const;
    MemResult store(u32 addr, u32 size, u32 data);

    // Fetch an instruction word. Returns OK/MISALIGNED/ACCESS_FAULT.
    MemResult fetch(u32 addr, u32& out) const;

    // Raw RAM access for loaders/debuggers (no MMIO).
    void write_ram(u32 addr, const u8* data, size_t len);
    void read_ram(u32 addr, u8* data, size_t len) const;
    void fill_ram(u32 addr, u8 value, size_t len);

    const Config& config() const { return cfg_; }
    const std::vector<u8>& ram() const { return ram_; }

private:
    Config cfg_;
    std::vector<u8> ram_;

    bool in_ram(u32 addr, u32 size) const;
    u32 read_le32(const u8* p) const;
    u16 read_le16(const u8* p) const;
    u8  read_le8 (const u8* p) const { return *p; }
    void write_le32(u8* p, u32 v) const;
    void write_le16(u8* p, u16 v) const;
};

} // namespace emulator
