#include "emulator/common.h"
#include "emulator/decoder.h"
#include "emulator/emulator_iss.h"
#include "emulator/memory.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace emulator;

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL: %s at line %d\n", #cond, __LINE__); ++failures; } } while(0)

static u32 encode_i(u32 opcode, u32 funct3, u32 rd, u32 rs1, u32 imm) {
    return opcode | (rd << 7) | (funct3 << 12) | (rs1 << 15) | ((imm & 0xFFF) << 20);
}

static u32 encode_r(u32 opcode, u32 funct3, u32 funct7, u32 rd, u32 rs1, u32 rs2) {
    return opcode | (rd << 7) | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) | (funct7 << 25);
}

static u32 encode_u(u32 opcode, u32 rd, u32 imm) {
    return opcode | (rd << 7) | (imm & 0xFFFFF000);
}

static u32 encode_j(u32 rd, s32 offset) {
    u32 imm = static_cast<u32>(offset);
    u32 jimm = ((imm & 0x100000) << 11) |
               ((imm & 0x0FF000) << 0) |
               ((imm & 0x000800) << 9) |
               ((imm & 0x0007FE) << 20);
    return 0x6F | (rd << 7) | jimm;
}

static void test_decoder() {
    DecodedInst d;
    d = decode(encode_i(0x13, 0, 1, 2, 42)); // addi x1, x2, 42
    CHECK(d.type == InstType::ADDI);
    CHECK(d.rd == 1 && d.rs1 == 2 && d.imm == 42);

    d = decode(encode_r(0x33, 0, 0, 3, 4, 5)); // add x3, x4, x5
    CHECK(d.type == InstType::ADD);
    CHECK(d.rd == 3 && d.rs1 == 4 && d.rs2 == 5);

    d = decode(encode_u(0x37, 6, 0x12345000)); // lui x6, 0x12345000
    CHECK(d.type == InstType::LUI);
    CHECK(d.rd == 6 && d.imm == 0x12345000);

    d = decode(encode_j(7, 16)); // jal x7, +16
    CHECK(d.type == InstType::JAL);
    CHECK(d.rd == 7 && d.imm == 16);
}

static void test_addi() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_i(0x13, 0, 1, 0, 5)); // addi x1, x0, 5
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(1) == 5);
}

static void test_lui_auipc() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_u(0x37, 1, 0x12345000)); // lui x1, 0x12345
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(1) == 0x12345000);
    iss.write_mem(0x20000004, 4, encode_u(0x17, 2, 0x00001000)); // auipc x2, 0x1
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(2) == (0x20000004 + 0x00001000));
}

static void test_store_load() {
    EmulatorISS iss;
    // lui x3, 0x20000 -> x3 = 0x20000000 (RAM base)
    // addi x1, x0, 0xAB -> x1 = 0xAB
    // sw x1, 0(x3) -> mem[0x20000000] = 0xAB
    // lw x2, 0(x3) -> x2 = 0xAB
    iss.write_mem(0x20000000, 4, encode_u(0x37, 3, 0x20000000)); // lui x3, 0x20000
    iss.write_mem(0x20000004, 4, encode_i(0x13, 0, 1, 0, 0xAB)); // addi x1, x0, 0xAB
    u32 sw_enc = 0x23 | (2 << 12) | (3 << 15) | (1 << 20); // sw x1, 0(x3)
    iss.write_mem(0x20000008, 4, sw_enc);
    u32 lw_enc = 0x03 | (2 << 12) | (3 << 15) | (2 << 7); // lw x2, 0(x3)
    iss.write_mem(0x2000000C, 4, lw_enc);
    CommitEvent ev;
    CHECK(iss.step_inst(ev)); // lui x3
    CHECK(iss.step_inst(ev)); // addi x1
    CHECK(iss.step_inst(ev)); // sw
    CHECK(iss.step_inst(ev)); // lw
    CHECK(iss.reg(2) == 0xAB);
}

static void test_csr() {
    EmulatorISS iss;
    // csrrw x1, mstatus, x0 -> read mstatus into x1, write 0
    u32 csrw = 0x73 | (1 << 12) | (1 << 7) | (0 << 15) | (CSR_MSTATUS << 20);
    iss.write_mem(0x20000000, 4, csrw);
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(1) == 0x00001800); // MPP=M
    CHECK(iss.csr(CSR_MSTATUS) == 0x00001800);
}

static void test_clint() {
    EmulatorISS iss;
    // Read mtime at CLINT base + 0xBFF8 = 0x0200BFF8.
    // lui x2, 0x0200C gives x2 = 0x0200C000; lw x1, -8(x2) reads 0x0200BFF8.
    u32 lui_addr = encode_u(0x37, 2, 0x0200C000); // lui x2, 0x0200C
    u32 lw_mtime = 0x03 | (2 << 12) | (2 << 15) | (1 << 7) | (0xFF8 << 20); // lw x1, -8(x2)
    iss.write_mem(0x20000000, 4, lui_addr);
    iss.write_mem(0x20000004, 4, lw_mtime);
    CommitEvent ev;
    CHECK(iss.step_inst(ev)); // lui
    CHECK(iss.step_inst(ev)); // lw; reads mtime after first tick = 1
    CHECK(iss.reg(1) == 1);
}

static void test_branch() {
    EmulatorISS iss;
    // addi x1, x0, 5; addi x2, x0, 5; beq x1, x2, +8; addi x3, x0, 1; addi x3, x0, 2
    iss.write_mem(0x20000000, 4, encode_i(0x13, 0, 1, 0, 5));
    iss.write_mem(0x20000004, 4, encode_i(0x13, 0, 2, 0, 5));
    // beq x1, x2, +8: imm=8, funct3=0
    u32 beq = 0x63 | (0 << 12) | (1 << 15) | (2 << 20) | ((8 & 0x800) >> 4) | ((8 & 0x1E) << 7) | ((8 & 0x7E0) << 20);
    // Simpler: encode B-immediate manually.
    auto encode_b = [](u32 funct3, u32 rs1, u32 rs2, s32 offset) {
        u32 imm = static_cast<u32>(offset);
        return 0x63 | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) |
               (((imm >> 1) & 0xF)   << 8)  |
               (((imm >> 11) & 1)   << 7)  |
               (((imm >> 5) & 0x3F) << 25) |
               (((imm >> 12) & 1)  << 31);
    };
    beq = encode_b(0, 1, 2, 8);
    iss.write_mem(0x20000008, 4, beq);
    iss.write_mem(0x2000000C, 4, encode_i(0x13, 0, 3, 0, 1)); // skipped
    iss.write_mem(0x20000010, 4, encode_i(0x13, 0, 3, 0, 2)); // target
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.pc() == 0x20000010);
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(3) == 2);
}

int main() {
    test_decoder();
    test_addi();
    test_lui_auipc();
    test_store_load();
    test_csr();
    test_clint();
    test_branch();
    if (failures == 0) {
        std::printf("All tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
