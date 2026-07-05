#pragma once

#include "emulator/emulator_iss.h"
#include "emulator/iss.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

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

    std::vector<std::string> tokenize(const std::string& line);
    bool run_tokens(const std::vector<std::string>& tokens);

    // Helpers
    u32 parse_hex(const std::string& s);
    u32 parse_int(const std::string& s);
};

} // namespace emulator
