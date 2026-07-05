#include "emulator/uart.h"
#include <fstream>
#include <stdexcept>

namespace emulator {

void Uart::reset() {
    input_.clear();
    output_.clear();
    if (out_file_) {
        std::fflush(out_file_);
    }
}

bool Uart::load(u32 offset, u32 size, u32& out) {
    (void)offset;
    if (size != 1 && size != 2 && size != 4) return false;
    if (input_.empty()) {
        out = 0xFF;
        return true;
    }
    u8 ch = input_.front();
    input_.pop_front();
    out = ch;
    return true;
}

bool Uart::store(u32 offset, u32 size, u32 data) {
    (void)offset;
    if (size != 1 && size != 2 && size != 4) return false;
    append_output(static_cast<u8>(data & 0xFF));
    return true;
}

void Uart::set_input(std::string data) {
    input_.assign(data.begin(), data.end());
}

void Uart::set_input(const std::vector<u8>& data) {
    input_.assign(data.begin(), data.end());
}

void Uart::set_input_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return;
    std::string s((std::istreambuf_iterator<char>(f)),
                   std::istreambuf_iterator<char>());
    set_input(s);
}

void Uart::set_output_file(const std::string& path) {
    if (out_file_) {
        std::fclose(out_file_);
    }
    out_file_ = std::fopen(path.c_str(), "wb");
}

void Uart::flush_output() {
    if (out_file_) std::fflush(out_file_);
}

void Uart::append_output(u8 ch) {
    output_.push_back(static_cast<char>(ch));
    if (out_file_) {
        std::fputc(ch, out_file_);
    } else {
        std::fputc(ch, stdout);
    }
}

} // namespace emulator
