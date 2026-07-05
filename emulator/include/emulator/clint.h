#pragma once

#include "emulator/common.h"
#include "emulator/config.h"

namespace emulator {

class Clint {
public:
    explicit Clint(const Config& cfg);

    void reset();
    void tick();  // increment mtime/mtimeh by one cycle

    bool load(u32 addr, u32 size, u32& out) const;
    bool store(u32 addr, u32 size, u32 data);

    u64 time() const { return time_; }

private:
    Config cfg_;
    u64 time_ = 0;
};

} // namespace emulator
