#include "emulator/emulator_iss.h"
#include "emulator/shell.h"
#include <iostream>
#include <fstream>
#include <string>
#include <memory>

#ifdef WITH_RTL
#include "emulator/difftest.h"
#include "emulator/rtl_iss.h"
#endif

using namespace emulator;

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
#ifdef WITH_RTL
              << "  --rtl                 Use the Verilated RTL core as backend\n"
              << "  --difftest            Compare emulator reference against RTL backend\n"
#endif
              << "  -f, --file <script>   Execute commands from file\n"
              << "  -e, --exec <cmd>     Execute command string\n"
              << "  --reset-vector <addr> Set reset vector (hex)\n"
              << "  --ram-base <addr>     Set RAM base (hex)\n"
              << "  --ram-size <size>     Set RAM size (hex)\n"
              << "  --strict-mem          Treat unmapped data accesses as faults\n"
              << "  --max-cycles <N>      Terminate after N cycles (0 = unlimited)\n"
              << "  --max-pc-stuck <N>    Terminate if same PC retires N times\n"
              << "  --log <level>         Set log level\n"
              << "  -h, --help            Show this help\n";
}

int main(int argc, char** argv) {
    Config cfg;
    std::string script_file;
    std::string exec_cmd;
    bool use_rtl = false;
    bool use_difftest = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
#ifdef WITH_RTL
        } else if (arg == "--rtl") {
            use_rtl = true;
        } else if (arg == "--difftest") {
            use_difftest = true;
#endif
        } else if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            script_file = argv[++i];
        } else if ((arg == "-e" || arg == "--exec") && i + 1 < argc) {
            exec_cmd = argv[++i];
        } else if (arg == "--reset-vector" && i + 1 < argc) {
            cfg.reset_vector = static_cast<u32>(std::stoul(argv[++i], nullptr, 16));
        } else if (arg == "--ram-base" && i + 1 < argc) {
            cfg.ram_base = static_cast<u32>(std::stoul(argv[++i], nullptr, 16));
        } else if (arg == "--ram-size" && i + 1 < argc) {
            cfg.ram_size = static_cast<u32>(std::stoul(argv[++i], nullptr, 16));
        } else if (arg == "--log" && i + 1 < argc) {
            cfg.log_level = static_cast<int>(std::stoul(argv[++i]));
        } else if (arg == "--strict-mem") {
            cfg.strict_mem = true;
        } else if (arg == "--max-cycles" && i + 1 < argc) {
            cfg.max_cycles = static_cast<u64>(std::stoull(argv[++i]));
        } else if (arg == "--max-pc-stuck" && i + 1 < argc) {
            cfg.max_pc_stuck = static_cast<u32>(std::stoul(argv[++i]));
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    std::unique_ptr<ISS> iss;
#ifdef WITH_RTL
    if (use_rtl && use_difftest) {
        std::cerr << "--rtl and --difftest are mutually exclusive.\n";
        print_usage(argv[0]);
        return 1;
    }
    if (use_difftest) {
        iss = std::make_unique<Difftest>(
            std::make_unique<EmulatorISS>(cfg),
            std::make_unique<RtlISS>(cfg));
    } else if (use_rtl) {
        iss = std::make_unique<RtlISS>(cfg);
    } else {
        iss = std::make_unique<EmulatorISS>(cfg);
    }
#else
    if (use_rtl || use_difftest) {
        std::cerr << "RTL and difftest backends are not available in this build.\n";
        return 1;
    }
    iss = std::make_unique<EmulatorISS>(cfg);
#endif

    Shell shell(std::cin, std::cout, *iss);

    if (!script_file.empty()) {
        if (!shell.run_file(script_file)) {
            std::cerr << "Failed to open script: " << script_file << "\n";
            return 1;
        }
    } else if (!exec_cmd.empty()) {
        shell.execute_line(exec_cmd);
    } else {
        shell.run();
    }

    return 0;
}
