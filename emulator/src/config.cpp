#include "emulator/config.h"

namespace emulator {

bool Config::in_ram(u32 addr) const {
    return addr >= ram_base && addr < ram_base + ram_size;
}

bool Config::in_clint(u32 addr) const {
    return addr >= clint_base && addr < clint_base + clint_size;
}

bool Config::in_uart(u32 addr) const {
    return addr == uart_base;
}

} // namespace emulator
