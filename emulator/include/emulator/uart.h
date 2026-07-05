#pragma once

#include "emulator/common.h"
#include <deque>
#include <string>
#include <vector>
#include <cstdio>

namespace emulator {

class Uart {
public:
    Uart() = default;

    void reset();

    // MMIO load/store. Offset 0 only.
    bool load(u32 offset, u32 size, u32& out);
    bool store(u32 offset, u32 size, u32 data);

    // Configure input from a string or raw bytes.
    void set_input(std::string data);
    void set_input(const std::vector<u8>& data);
    void set_input_file(const std::string& path);

    // Configure output.
    void set_output_file(const std::string& path);
    void flush_output();

    const std::string& output() const { return output_; }
    bool has_input() const { return !input_.empty(); }

private:
    std::deque<u8> input_;
    std::string output_;
    FILE* out_file_ = nullptr;

    void append_output(u8 ch);
};

} // namespace emulator
