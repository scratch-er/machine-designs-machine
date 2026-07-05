#include "emulator/clint.h"

namespace emulator {

Clint::Clint(const Config& cfg) : cfg_(cfg) {}

void Clint::reset() {
    time_ = 0;
}

void Clint::tick() {
    ++time_;
}

bool Clint::load(u32 addr, u32 size, u32& out) const {
    if (!cfg_.in_clint(addr)) return false;
    u32 offset = addr - cfg_.clint_base;
    if (size != 4) {
        // undefined; return 0
        out = 0;
        return true;
    }
    if (offset == cfg_.clint_mtime_offset()) {
        out = static_cast<u32>(time_ & 0xFFFFFFFF);
        return true;
    }
    if (offset == cfg_.clint_mtimeh_offset()) {
        out = static_cast<u32>(time_ >> 32);
        return true;
    }
    out = 0;
    return true;
}

bool Clint::store(u32 addr, u32 size, u32 data) {
    if (!cfg_.in_clint(addr)) return false;
    u32 offset = addr - cfg_.clint_base;
    if (size != 4) return true;  // ignored
    if (offset == cfg_.clint_mtime_offset()) {
        time_ = (time_ & 0xFFFFFFFF00000000ULL) | data;
        return true;
    }
    if (offset == cfg_.clint_mtimeh_offset()) {
        time_ = (time_ & 0xFFFFFFFFULL) | (static_cast<u64>(data) << 32);
        return true;
    }
    return true;  // other registers ignored
}

} // namespace emulator
