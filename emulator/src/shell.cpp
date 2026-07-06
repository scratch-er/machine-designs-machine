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

EmulatorISS* Shell::as_emu() {
    return dynamic_cast<EmulatorISS*>(&iss_);
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

static std::string hex(u32 v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

static std::string fmt_inst(const CommitEvent& ev) {
    std::ostringstream oss;
    oss << "PC=" << hex(ev.pc)
        << " I=" << hex(ev.inst)
        << " RD=" << ev.rd
        << " RV=" << hex(ev.rd_value)
        << " NPC=" << hex(ev.next_pc);
    if (ev.exception) {
        oss << " EXC CAUSE=" << ev.cause;
    }
    return oss.str();
}

bool Shell::check_uart_trigger(const std::string& before, const std::string& target) {
    auto* emu = as_emu();
    if (!emu) return false;
    const std::string& out = emu->uart().output();
    if (out.size() <= before.size()) return false;
    return out.find(target) != std::string::npos;
}

bool Shell::run_with_condition(const RunCondition& cond) {
    auto* emu = as_emu();
    u64 count = 0;
    CommitEvent ev;
    u32 stuck_pc = 0;
    u32 stuck_count = 0;
    while (true) {
        if (!iss_.step_inst(ev)) break;
        ++count;

        // PC-stuck detection (only counts non-exception retirements).
        if (!ev.exception && ev.pc == stuck_pc) {
            ++stuck_count;
        } else {
            stuck_pc = ev.exception ? 0 : ev.pc;
            stuck_count = ev.exception ? 0 : 1;
        }

        if (emu) {
            const Config& cfg = emu->config();
            if (cfg.max_cycles != 0 && emu->cycle() >= cfg.max_cycles) {
                out_ << "stopped: max-cycles reached (" << cfg.max_cycles << ")\n";
                break;
            }
            if (cfg.max_pc_stuck != 0 && stuck_count >= cfg.max_pc_stuck) {
                out_ << "stopped: pc stuck at " << hex(stuck_pc)
                      << " for " << stuck_count << " retirements\n";
                break;
            }
        }

        if (cond.max_inst != 0 && count >= cond.max_inst) break;

        if (cond.stop_pc && iss_.pc() == cond.target_pc) {
            out_ << "stopped at pc " << hex(cond.target_pc) << "\n";
            break;
        }
        if (cond.stop_reg && iss_.reg(cond.target_reg) == cond.target_reg_value) {
            out_ << "stopped: x" << cond.target_reg << " = "
                  << hex(cond.target_reg_value) << "\n";
            break;
        }
        if (cond.stop_uart && check_uart_trigger(cond.uart_seen_before, cond.target_uart)) {
            out_ << "stopped: uart output contains \"" << cond.target_uart << "\"\n";
            break;
        }
        if (!breakpoints_.empty() && breakpoints_.count(iss_.pc())) {
            out_ << "stopped at breakpoint " << hex(iss_.pc()) << "\n";
            break;
        }
    }
    out_ << "ran " << count << " instructions\n";
    return true;
}

void Shell::print_breakpoints() {
    if (breakpoints_.empty()) {
        out_ << "no breakpoints\n";
        return;
    }
    out_ << "breakpoints:\n";
    for (u32 addr : breakpoints_) {
        out_ << "  " << hex(addr) << "\n";
    }
}

bool Shell::parse_trace_filter(const std::vector<std::string>& tok, TraceFilter& out) {
    if (tok.size() < 3) {
        out_ << "usage: trace on all | branches | loads | stores | exceptions | reg <i> | pc <low> <high>\n";
        return false;
    }
    out.enabled = true;
    const std::string& kind = tok[2];
    if (kind == "all") {
        out.kind = TraceFilterKind::ALL;
    } else if (kind == "branches") {
        out.kind = TraceFilterKind::BRANCHES;
    } else if (kind == "loads") {
        out.kind = TraceFilterKind::LOADS;
    } else if (kind == "stores") {
        out.kind = TraceFilterKind::STORES;
    } else if (kind == "exceptions") {
        out.kind = TraceFilterKind::EXCEPTIONS;
    } else if (kind == "reg") {
        if (tok.size() < 4) {
            out_ << "usage: trace on reg <i>\n";
            return false;
        }
        out.kind = TraceFilterKind::REG;
        out.reg_idx = parse_int(tok[3]);
    } else if (kind == "pc") {
        if (tok.size() < 5) {
            out_ << "usage: trace on pc <low> <high>\n";
            return false;
        }
        out.kind = TraceFilterKind::PC_RANGE;
        out.pc_lo = parse_hex(tok[3]);
        out.pc_hi = parse_hex(tok[4]);
    } else {
        out_ << "unknown trace filter: " << kind << "\n";
        return false;
    }
    return true;
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
        RunCondition cond;
        if (tok.size() > 1) {
            if (tok[1] == "to" && tok.size() > 2) {
                cond.stop_pc = true;
                cond.target_pc = parse_hex(tok[2]);
            } else if (tok[1] == "until" && tok.size() > 2) {
                if (tok[2] == "uart" && tok.size() > 3) {
                    cond.stop_uart = true;
                    cond.target_uart = tok[3];
                    auto* emu = as_emu();
                    if (emu) cond.uart_seen_before = emu->uart().output();
                } else if (tok[2] == "reg" && tok.size() > 4) {
                    cond.stop_reg = true;
                    cond.target_reg = parse_int(tok[3]);
                    cond.target_reg_value = parse_hex(tok[4]);
                } else {
                    out_ << "usage: run until uart <string> | run until reg <i> <value>\n";
                    return true;
                }
            } else {
                cond.max_inst = parse_int(tok[1]);
            }
        }
        run_with_condition(cond);
    } else if (cmd == "break") {
        if (tok.size() < 2) { out_ << "usage: break <addr>\n"; return true; }
        breakpoints_.insert(parse_hex(tok[1]));
    } else if (cmd == "delete-break") {
        if (tok.size() < 2) { out_ << "usage: delete-break <addr>\n"; return true; }
        breakpoints_.erase(parse_hex(tok[1]));
    } else if (cmd == "clear-breaks") {
        breakpoints_.clear();
    } else if (cmd == "list-breaks") {
        print_breakpoints();
    } else if (cmd == "print") {
        if (tok.size() < 2) { out_ << "usage: print pc|reg [i]|csr [addr]|mem <addr> <size>\n"; return true; }
        if (tok[1] == "pc") {
            out_ << "pc = " << hex(iss_.pc()) << "\n";
        } else if (tok[1] == "reg") {
            if (tok.size() > 2) {
                u32 i = parse_int(tok[2]);
                out_ << "x" << i << " = " << hex(iss_.reg(i)) << "\n";
            } else {
                for (u32 i = 0; i < GPR_COUNT; ++i) {
                    out_ << "x" << std::dec << i << " = "
                          << hex(iss_.reg(i)) << "\n";
                }
            }
        } else if (tok[1] == "csr") {
            if (tok.size() > 2) {
                u32 a = parse_hex(tok[2]);
                out_ << "csr[" << hex(a) << "] = " << hex(iss_.csr(a)) << "\n";
            }
        } else if (tok[1] == "mem") {
            if (tok.size() < 4) { out_ << "usage: print mem <addr> <size>\n"; return true; }
            u32 addr = parse_hex(tok[2]);
            u32 size = parse_int(tok[3]);
            for (u32 i = 0; i < size; ++i) {
                if (i % 16 == 0) out_ << hex(addr + i) << ":";
                out_ << " " << std::setw(2) << std::setfill('0')
                      << (iss_.read_mem(addr + i, 1) & 0xFF);
                if ((i + 1) % 16 == 0) out_ << "\n";
            }
            if (size % 16 != 0) out_ << "\n";
        }
    } else if (cmd == "dump") {
        if (tok.size() < 2 || tok[1] != "state") {
            out_ << "usage: dump state\n";
            return true;
        }
        out_ << "pc = " << hex(iss_.pc()) << "\n";
        for (u32 i = 0; i < GPR_COUNT; ++i) {
            out_ << "x" << i << " = " << hex(iss_.reg(i));
            if (i % 4 == 3) out_ << "\n";
            else out_ << "  ";
        }
        if (GPR_COUNT % 4 != 0) out_ << "\n";
        out_ << "mstatus=" << hex(iss_.csr(CSR_MSTATUS))
              << " mepc=" << hex(iss_.csr(CSR_MEPC))
              << " mtvec=" << hex(iss_.csr(CSR_MTVEC))
              << " mcause=" << hex(iss_.csr(CSR_MCAUSE)) << "\n";
    } else if (cmd == "checkpoint") {
        if (tok.size() < 3) { out_ << "usage: checkpoint save|load <file>\n"; return true; }
        if (tok[1] == "save") {
            if (!iss_.save_checkpoint(tok[2])) out_ << "save failed\n";
        } else if (tok[1] == "load") {
            if (!iss_.load_checkpoint(tok[2])) out_ << "load failed\n";
        }
    } else if (cmd == "uart") {
        if (tok.size() < 3) { out_ << "usage: uart input <file|hex> | uart output <file>\n"; return true; }
        auto* emu = as_emu();
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
    } else if (cmd == "trace") {
        auto* emu = as_emu();
        if (!emu) { out_ << "trace command only supported on emulator ISS\n"; return true; }
        if (tok.size() < 2) { out_ << "usage: trace on <filter> | trace off\n"; return true; }
        if (tok[1] == "off") {
            TraceFilter f;
            f.enabled = false;
            emu->tracer().set_filter(f);
        } else if (tok[1] == "on") {
            TraceFilter f;
            if (!parse_trace_filter(tok, f)) return true;
            emu->tracer().set_filter(f);
        } else {
            out_ << "usage: trace on <filter> | trace off\n";
        }
    } else if (cmd == "last") {
        auto* emu = as_emu();
        if (!emu) { out_ << "last command only supported on emulator ISS\n"; return true; }
        size_t n = (tok.size() > 1) ? parse_int(tok[1]) : 10;
        auto evs = emu->last_events(n);
        out_ << "last " << evs.size() << " instruction(s):\n";
        for (const auto& ev : evs) {
            out_ << "  " << fmt_inst(ev) << "\n";
        }
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
