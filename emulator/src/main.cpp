#include "emulator/emulator_iss.h"
#include "emulator/shell.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace emulator;

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  -f, --file <script>    Execute commands from file\n"
              << "  -e, --exec <cmd>      Execute command string\n"
              << "  --reset-vector <addr> Set reset vector (hex)\n"
              << "  --ram-base <addr>     Set RAM base (hex)\n"
              << "  --ram-size <size>     Set RAM size (hex)\n"
              << "  --strict-mem          Treat unmapped data accesses as faults\n"
              << "  --log <level>         Set log level\n"
              << "  -h, --help            Show this help\n";
}

int main(int argc, char** argv) {
    Config cfg;
    std::string script_file;
    std::string exec_cmd;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
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
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    EmulatorISS iss(cfg);
    Shell shell(std::cin, std::cout, iss);

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
