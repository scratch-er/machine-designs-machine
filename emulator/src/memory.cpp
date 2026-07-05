#include "emulator/memory.h"
#include <string>

namespace emulator {

Memory::Memory(const Config& cfg) : cfg_(cfg), ram_(cfg.ram_size, 0) {}

bool Memory::in_ram(u32 addr, u32 size) const {
    if (addr < cfg_.ram_base) return false;
    u32 end = addr + size;
    if (end < addr) return false;  // overflow
    return end <= cfg_.ram_base + cfg_.ram_size;
}

MemResult Memory::load(u32 addr, u32 size, u32& out) const {
    if (size != 1 && size != 2 && size != 4) return MemResult::ACCESS_FAULT;
    if ((addr & (size - 1)) != 0) return MemResult::MISALIGNED;

    if (in_ram(addr, size)) {
        const u8* p = ram_.data() + (addr - cfg_.ram_base);
        switch (size) {
            case 1: out = read_le8(p); break;
            case 2: out = read_le16(p); break;
            case 4: out = read_le32(p); break;
        }
        return MemResult::OK;
    }

    if (cfg_.strict_mem) return MemResult::ACCESS_FAULT;
    out = 0;
    return MemResult::OK;
}

MemResult Memory::store(u32 addr, u32 size, u32 data) {
    if (size != 1 && size != 2 && size != 4) return MemResult::ACCESS_FAULT;
    if ((addr & (size - 1)) != 0) return MemResult::MISALIGNED;

    if (in_ram(addr, size)) {
        u8* p = ram_.data() + (addr - cfg_.ram_base);
        switch (size) {
            case 1: *p = static_cast<u8>(data); break;
            case 2: write_le16(p, static_cast<u16>(data)); break;
            case 4: write_le32(p, data); break;
        }
        return MemResult::OK;
    }

    if (cfg_.strict_mem) return MemResult::ACCESS_FAULT;
    return MemResult::OK;
}

MemResult Memory::fetch(u32 addr, u32& out) const {
    if ((addr & 3u) != 0) return MemResult::MISALIGNED;
    if (!in_ram(addr, 4)) return MemResult::ACCESS_FAULT;
    out = read_le32(ram_.data() + (addr - cfg_.ram_base));
    return MemResult::OK;
}

void Memory::write_ram(u32 addr, const u8* data, size_t len) {
    if (len == 0 || !in_ram(addr, static_cast<u32>(len))) return;
    std::memcpy(ram_.data() + (addr - cfg_.ram_base), data, len);
}

void Memory::read_ram(u32 addr, u8* data, size_t len) const {
    if (len == 0 || !in_ram(addr, static_cast<u32>(len))) return;
    std::memcpy(data, ram_.data() + (addr - cfg_.ram_base), len);
}

void Memory::fill_ram(u32 addr, u8 value, size_t len) {
    if (len == 0 || !in_ram(addr, static_cast<u32>(len))) return;
    std::memset(ram_.data() + (addr - cfg_.ram_base), value, len);
}

u32 Memory::read_le32(const u8* p) const {
    return static_cast<u32>(p[0])        |
           (static_cast<u32>(p[1]) << 8)  |
           (static_cast<u32>(p[2]) << 16) |
           (static_cast<u32>(p[3]) << 24);
}

u16 Memory::read_le16(const u8* p) const {
    return static_cast<u16>(p[0]) |
           (static_cast<u16>(p[1]) << 8);
}

void Memory::write_le32(u8* p, u32 v) const {
    p[0] = static_cast<u8>(v);
    p[1] = static_cast<u8>(v >> 8);
    p[2] = static_cast<u8>(v >> 16);
    p[3] = static_cast<u8>(v >> 24);
}

void Memory::write_le16(u8* p, u16 v) const {
    p[0] = static_cast<u8>(v);
    p[1] = static_cast<u8>(v >> 8);
}

} // namespace emulator
