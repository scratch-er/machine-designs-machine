#include "emulator/shell.h"
#include "emulator/iss.h"
#include "emulator/common.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>

namespace emulator {

Shell::Shell(std::istream& in, std::ostream& out, ISS& iss)
    : in_(in), out_(out), iss_(iss) {}

u32 Shell::parse_hex(const std::string& s) {
    return static_cast<u32>(std::stoul(s, nullptr, 16));
}

u32 Shell::parse_int(const std::string& s) {
    if (s.size() > 2 && (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')))
        return parse_hex(s);
    return static_cast<u32>(std::stoul(s, nullptr, 10));
}

std::vector<std::string> Shell::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(tok);
    }
    return tokens;
}

bool Shell::run_tokens(const std::vector<std::string>& tok) {
    if (tok.empty()) return true;
    const std::string& cmd = tok[0];

    if (cmd == "load" || cmd == "load_bin") {
        if (tok.size() < 2) { out_ << "usage: load <file> [addr]\n"; return true; }
        u32 addr = (tok.size() > 2) ? parse_hex(tok[2]) : 0x20000000;
        if (!iss_.load_bin(tok[1], addr)) {
            out_ << "failed to load " << tok[1] << "\n";
        }
    } else if (cmd == "load_elf") {
        if (tok.size() < 2) { out_ << "usage: load_elf <file>\n"; return true; }
        if (!iss_.load_elf(tok[1])) {
            out_ << "failed to load ELF " << tok[1] << "\n";
        }
    } else if (cmd == "reset") {
        u32 addr = (tok.size() > 1) ? parse_hex(tok[1]) : 0x20000000;
        iss_.reset(addr);
    } else if (cmd == "step") {
        u32 n = (tok.size() > 1) ? parse_int(tok[1]) : 1;
        CommitEvent ev;
        for (u32 i = 0; i < n; ++i) {
            if (!iss_.step_inst(ev)) break;
        }
    } else if (cmd == "run") {
        u32 n = (tok.size() > 1) ? parse_int(tok[1]) : 0;
        u32 count = 0;
        CommitEvent ev;
        while (true) {
            if (!iss_.step_inst(ev)) break;
            ++count;
            if (n != 0 && count >= n) break;
        }
        out_ << "ran " << count << " instructions\n";
    } else if (cmd == "print") {
        if (tok.size() < 2) { out_ << "usage: print pc|reg [i]|csr [addr]|mem <addr> <size>\n"; return true; }
        if (tok[1] == "pc") {
            out_ << "pc = 0x" << std::hex << iss_.pc() << std::dec << "\n";
        } else if (tok[1] == "reg") {
            if (tok.size() > 2) {
                u32 i = parse_int(tok[2]);
                out_ << "x" << i << " = 0x" << std::hex << iss_.reg(i) << std::dec << "\n";
            } else {
                for (u32 i = 0; i < GPR_COUNT; ++i) {
                    out_ << "x" << std::dec << i << " = 0x"
                          << std::hex << std::setw(8) << std::setfill('0') << iss_.reg(i)
                          << std::dec << "\n";
                }
            }
        } else if (tok[1] == "csr") {
            if (tok.size() > 2) {
                u32 a = parse_hex(tok[2]);
                out_ << "csr[0x" << std::hex << a << "] = 0x"
                      << iss_.csr(a) << std::dec << "\n";
            }
        } else if (tok[1] == "mem") {
            if (tok.size() < 4) { out_ << "usage: print mem <addr> <size>\n"; return true; }
            u32 addr = parse_hex(tok[2]);
            u32 size = parse_int(tok[3]);
            for (u32 i = 0; i < size; ++i) {
                if (i % 16 == 0) out_ << "0x" << std::hex << (addr + i) << ":";
                out_ << " " << std::setw(2) << std::setfill('0')
                      << (iss_.read_mem(addr + i, 1) & 0xFF);
                if ((i + 1) % 16 == 0) out_ << "\n";
            }
            if (size % 16 != 0) out_ << "\n";
        }
    } else if (cmd == "checkpoint") {
        if (tok.size() < 3) { out_ << "usage: checkpoint save|load <file>\n"; return true; }
        if (tok[1] == "save") {
            if (!iss_.save_checkpoint(tok[2])) out_ << "save failed\n";
        } else if (tok[1] == "load") {
            if (!iss_.load_checkpoint(tok[2])) out_ << "load failed\n";
        }
    } else if (cmd == "uart") {
        if (tok.size() < 3) { out_ << "usage: uart input <file|hex> | uart output <file>\n"; return true; }
        // Cast to EmulatorISS to access UART. This is acceptable because the shell
        // is tied to the emulator executable.
        auto* emu = dynamic_cast<EmulatorISS*>(&iss_);
        if (!emu) { out_ << "uart command only supported on emulator ISS\n"; return true; }
        if (tok[1] == "input") {
            if (tok[2].size() >= 2 && tok[2][0] == '0' && (tok[2][1] == 'x' || tok[2][1] == 'X')) {
                std::string hexstr = tok[2].substr(2);
                std::vector<u8> bytes;
                for (size_t i = 0; i + 1 < hexstr.size(); i += 2) {
                    std::string byte_str = hexstr.substr(i, 2);
                    bytes.push_back(static_cast<u8>(std::stoul(byte_str, nullptr, 16)));
                }
                emu->uart().set_input(bytes);
            } else {
                emu->uart().set_input_file(tok[2]);
            }
        } else if (tok[1] == "output") {
            emu->uart().set_output_file(tok[2]);
        }
    } else if (cmd == "log") {
        if (tok.size() < 2) { out_ << "usage: log <level>\n"; return true; }
        iss_.set_log_level(static_cast<int>(parse_int(tok[1])));
    } else if (cmd == "exit" || cmd == "quit") {
        running_ = false;
    } else {
        out_ << "unknown command: " << cmd << "\n";
    }
    return true;
}

bool Shell::execute_line(const std::string& line) {
    // Strip comments.
    size_t c = line.find('#');
    std::string stripped = (c == std::string::npos) ? line : line.substr(0, c);
    // Split on semicolons and execute each part.
    std::vector<std::string> parts;
    std::istringstream iss(stripped);
    std::string part;
    while (std::getline(iss, part, ';')) {
        parts.push_back(part);
    }
    for (const auto& p : parts) {
        auto tok = tokenize(p);
        if (!run_tokens(tok)) return false;
    }
    return running_;
}

void Shell::run() {
    std::string line;
    while (running_ && std::getline(in_, line)) {
        execute_line(line);
    }
}

bool Shell::run_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (running_ && std::getline(f, line)) {
        execute_line(line);
    }
    return true;
}

} // namespace emulator
