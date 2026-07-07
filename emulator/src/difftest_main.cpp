#include "emulator/emulator_iss.h"
#include "emulator/rtl_iss.h"
#include "emulator/difftest.h"
#include "emulator/commit.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace fs = std::filesystem;
using namespace emulator;

static bool is_elf(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4];
    f.read(magic, 4);
    return f.gcount() == 4 && std::memcmp(magic, "\x7f" "ELF", 4) == 0;
}

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  -p, --program <file>  Program to run (ELF or raw binary)\n"
              << "  --bin-addr <addr>     Load address for raw binary (hex)\n"
              << "  -n, --max-steps <N>   Maximum instructions to compare (default 1000000)\n"
              << "  --reset-vector <addr> Reset vector (hex)\n"
              << "  --ram-base <addr>     RAM base (hex)\n"
              << "  --ram-size <size>     RAM size (hex)\n"
              << "  -h, --help            Show this help\n";
}

int main(int argc, char** argv) {
    Config cfg;
    std::string program;
    u32 bin_addr = cfg.reset_vector;
    uint64_t max_steps = 1000000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--program") && i + 1 < argc) {
            program = argv[++i];
        } else if (arg == "--bin-addr" && i + 1 < argc) {
            bin_addr = static_cast<u32>(std::stoul(argv[++i], nullptr, 16));
        } else if ((arg == "-n" || arg == "--max-steps") && i + 1 < argc) {
            max_steps = std::stoull(argv[++i]);
        } else if (arg == "--reset-vector" && i + 1 < argc) {
            cfg.reset_vector = static_cast<u32>(std::stoul(argv[++i], nullptr, 16));
            bin_addr = cfg.reset_vector;
        } else if (arg == "--ram-base" && i + 1 < argc) {
            cfg.ram_base = static_cast<u32>(std::stoul(argv[++i], nullptr, 16));
        } else if (arg == "--ram-size" && i + 1 < argc) {
            cfg.ram_size = static_cast<u32>(std::stoul(argv[++i], nullptr, 16));
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (program.empty()) {
        std::cerr << "No program specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    if (!fs::exists(program)) {
        std::cerr << "Program not found: " << program << "\n";
        return 1;
    }

    auto ref = std::make_unique<EmulatorISS>(cfg);
    auto dut = std::make_unique<RtlISS>(cfg);

    bool loaded = false;
    if (is_elf(program)) {
        loaded = ref->load_elf(program) && dut->load_elf(program);
        if (!loaded) {
            std::cerr << "Failed to load ELF: " << program << "\n";
            return 1;
        }
    } else {
        loaded = ref->load_bin(program, bin_addr) && dut->load_bin(program, bin_addr);
        if (!loaded) {
            std::cerr << "Failed to load binary: " << program << "\n";
            return 1;
        }
    }

    ref->reset(cfg.reset_vector);
    dut->reset(cfg.reset_vector);

    Difftest diff(ref.get(), dut.get());
    bool ok = diff.run(max_steps);

    if (ok) {
        std::cout << "PASS: reference and RTL matched for "
                  << diff.retire_index() << " instructions.\n";
        return 0;
    } else {
        std::cerr << "FAIL: " << diff.last_mismatch() << "\n";
        return 1;
    }
}
