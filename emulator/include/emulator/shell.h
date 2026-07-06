#pragma once

#include "emulator/emulator_iss.h"
#include "emulator/iss.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <optional>

namespace emulator {

class Shell {
public:
    explicit Shell(std::istream& in, std::ostream& out, ISS& iss);

    // Run interactively until EOF or exit.
    void run();

    // Execute a single command line.
    bool execute_line(const std::string& line);

    // Execute a script file.
    bool run_file(const std::string& path);

private:
    std::istream& in_;
    std::ostream& out_;
    ISS& iss_;
    bool running_ = true;

    std::set<u32> breakpoints_;

    std::vector<std::string> tokenize(const std::string& line);
    bool run_tokens(const std::vector<std::string>& tokens);

    // Helpers
    u32 parse_hex(const std::string& s);
    u32 parse_int(const std::string& s);

    EmulatorISS* as_emu();

    struct RunCondition {
        uint64_t max_inst = 0;
        bool stop_pc = false;
        u32 target_pc = 0;
        bool stop_reg = false;
        u32 target_reg = 0;
        u32 target_reg_value = 0;
        bool stop_uart = false;
        std::string target_uart;
        std::string uart_seen_before;
    };

    bool run_with_condition(const RunCondition& cond);
    bool check_uart_trigger(const std::string& before, const std::string& target);

    void print_breakpoints();
    bool parse_trace_filter(const std::vector<std::string>& tok, TraceFilter& out);
};

} // namespace emulator
