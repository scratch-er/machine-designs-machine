#include "emulator/common.h"
#include "emulator/decoder.h"
#include "emulator/emulator_iss.h"
#include "emulator/memory.h"
#include "emulator/difftest.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace emulator;

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL: %s at line %d\n", #cond, __LINE__); ++failures; } } while(0)

// --- Instruction encoders ---------------------------------------------------

static u32 encode_i(u32 opcode, u32 funct3, u32 rd, u32 rs1, u32 imm) {
    return opcode | (rd << 7) | (funct3 << 12) | (rs1 << 15) | ((imm & 0xFFF) << 20);
}

static u32 encode_r(u32 opcode, u32 funct3, u32 funct7, u32 rd, u32 rs1, u32 rs2) {
    return opcode | (rd << 7) | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) | (funct7 << 25);
}

static u32 encode_u(u32 opcode, u32 rd, u32 imm) {
    return opcode | (rd << 7) | (imm & 0xFFFFF000);
}

static u32 encode_s(u32 funct3, u32 rs1, u32 rs2, s32 offset) {
    u32 imm = static_cast<u32>(offset);
    return 0x23 | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) |
           ((imm & 0x1F) << 7) | (((imm >> 5) & 0x7F) << 25);
}

static u32 encode_b(u32 funct3, u32 rs1, u32 rs2, s32 offset) {
    u32 imm = static_cast<u32>(offset);
    return 0x63 | (funct3 << 12) | (rs1 << 15) | (rs2 << 20) |
           (((imm >> 11) & 1) << 7) |
           (((imm >> 1) & 0xF) << 8) |
           (((imm >> 5) & 0x3F) << 25) |
           (((imm >> 12) & 1) << 31);
}

static u32 encode_j(u32 rd, s32 offset) {
    u32 imm = static_cast<u32>(offset);
    u32 jimm = ((imm & 0x100000) << 11) |
               ((imm & 0x0FF000) << 0) |
               ((imm & 0x000800) << 9) |
               ((imm & 0x0007FE) << 20);
    return 0x6F | (rd << 7) | jimm;
}

static u32 encode_jalr(u32 rd, u32 rs1, s32 offset) {
    return 0x67 | (rd << 7) | (0 << 12) | (rs1 << 15) | ((static_cast<u32>(offset) & 0xFFF) << 20);
}

static u32 encode_csr(u32 funct3, u32 rd, u32 rs1_or_imm, u32 csr) {
    return 0x73 | (rd << 7) | (funct3 << 12) | (rs1_or_imm << 15) | (csr << 20);
}

// --- Decoder tests ----------------------------------------------------------

static void test_decoder() {
    DecodedInst d;

    d = decode(encode_i(OPCODE_OP_IMM, 0, 1, 2, 42)); // addi x1, x2, 42
    CHECK(d.type == InstType::ADDI);
    CHECK(d.rd == 1 && d.rs1 == 2 && d.imm == 42);

    d = decode(encode_r(OPCODE_OP, 0, 0, 3, 4, 5)); // add x3, x4, x5
    CHECK(d.type == InstType::ADD);
    CHECK(d.rd == 3 && d.rs1 == 4 && d.rs2 == 5);

    d = decode(encode_u(OPCODE_LUI, 6, 0x12345000)); // lui x6, 0x12345
    CHECK(d.type == InstType::LUI);
    CHECK(d.rd == 6 && d.imm == 0x12345000);

    d = decode(encode_u(OPCODE_AUIPC, 7, 0x00010000)); // auipc x7, 0x10
    CHECK(d.type == InstType::AUIPC);
    CHECK(d.rd == 7 && d.imm == 0x00010000);

    d = decode(encode_j(8, 1024)); // jal x8, +1024
    CHECK(d.type == InstType::JAL);
    CHECK(d.rd == 8 && d.imm == 1024);

    d = decode(encode_jalr(9, 10, -8)); // jalr x9, -8(x10)
    CHECK(d.type == InstType::JALR);
    CHECK(d.rd == 9 && d.rs1 == 10 && d.imm == static_cast<u32>(-8));

    d = decode(encode_b(0, 1, 2, 16)); // beq x1, x2, +16
    CHECK(d.type == InstType::BEQ);
    CHECK(d.rs1 == 1 && d.rs2 == 2 && d.imm == 16);

    d = decode(encode_s(2, 3, 4, -4)); // sw x4, -4(x3)
    CHECK(d.type == InstType::SW);
    CHECK(d.rs1 == 3 && d.rs2 == 4 && d.imm == static_cast<u32>(-4));

    d = decode(encode_i(OPCODE_LOAD, 2, 5, 6, 8)); // lw x5, 8(x6)
    CHECK(d.type == InstType::LW);
    CHECK(d.rd == 5 && d.rs1 == 6 && d.imm == 8);

    d = decode(0xFFFFFFFF);
    CHECK(d.type == InstType::UNKNOWN);

    d = decode(0x00000000); // c.unimp, treated as unknown
    CHECK(d.type == InstType::UNKNOWN);
}

// --- RV32E register-index validation ----------------------------------------

static void test_rv32e_register_check() {
    EmulatorISS iss;
    // addi x16, x0, 0 should be illegal (rd out of range).
    iss.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 16, 0, 0));
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_ILLEGAL_INST);
    CHECK(iss.pc() == 0); // mtvec reset value
    CHECK(iss.csr(CSR_MEPC) == 0x20000000);
    CHECK(iss.csr(CSR_MCAUSE) == CAUSE_ILLEGAL_INST);
}

// --- ALU tests --------------------------------------------------------------

static void test_addi() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 5));
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(1) == 5);
}

static void test_alu_ops() {
    EmulatorISS iss;
    // x1 = 10, x2 = 3, x3 = 0xFFFFFFFF (-1)
    iss.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 10));  // addi x1, x0, 10
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 2, 0, 3));   // addi x2, x0, 3
    iss.write_mem(0x20000008, 4, encode_u(OPCODE_LUI, 3, 0xFFFFF000));    // lui x3, 0xFFFFF
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));

    struct Case { u32 funct3; u32 funct7; u32 rd; u32 rs1; u32 rs2; u32 expected; };
    Case cases[] = {
        {0, 0, 4, 1, 2, 13},     // add x4, x1, x2 = 13
        {0, 0x20, 5, 1, 2, 7},   // sub x5, x1, x2 = 7
        {1, 0, 6, 1, 2, 0x50},   // sll x6, x1, x2 = 10 << 3 = 0x50
        {2, 0, 7, 1, 2, 0},      // slt x7, x1, x2 = 0
        {3, 0, 8, 1, 2, 0},      // sltu x8, x1, x2 = 0
        {4, 0, 9, 1, 2, 9},      // xor x9, x1, x2 = 9
        {5, 0, 10, 1, 2, 1},     // srl x10, x1, x2 = 10 >> 3 = 1
        {5, 0x20, 11, 3, 2, 0xFFFFFE00}, // sra x11, x3, x2 = -4096 >> 3 = -512
        {6, 0, 12, 1, 2, 11},    // or x12, x1, x2 = 11
        {7, 0, 13, 1, 2, 2},     // and x13, x1, x2 = 2
    };
    u32 pc = 0x2000000C;
    for (const auto& c : cases) {
        u32 inst = encode_r(OPCODE_OP, c.funct3, c.funct7, c.rd, c.rs1, c.rs2);
        iss.write_mem(pc, 4, inst);
        CHECK(iss.step_inst(ev));
        if (iss.reg(c.rd) != c.expected) {
            DecodedInst d = decode(inst);
            std::printf("ALU case fail: rd=%u expected=%08x actual=%08x f3=%u f7=%u inst=%08x type=%s\n",
                        c.rd, c.expected, iss.reg(c.rd), c.funct3, c.funct7, inst,
                        inst_name(d.type).c_str());
        }
        CHECK(iss.reg(c.rd) == c.expected);
        pc += 4;
    }

    // OP-IMM variants
    iss.reset(0x20000000);
    iss.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 10));
    CHECK(iss.step_inst(ev));

    // slti x2, x1, 20 = 1; sltiu x3, x1, 20 = 1; xori x4, x1, 5 = 15; ori x5, x1, 5 = 15; andi x6, x1, 5 = 0
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 2, 2, 1, 20));
    iss.write_mem(0x20000008, 4, encode_i(OPCODE_OP_IMM, 3, 3, 1, 20));
    iss.write_mem(0x2000000C, 4, encode_i(OPCODE_OP_IMM, 4, 4, 1, 5));
    iss.write_mem(0x20000010, 4, encode_i(OPCODE_OP_IMM, 6, 5, 1, 5));
    iss.write_mem(0x20000014, 4, encode_i(OPCODE_OP_IMM, 7, 6, 1, 5));
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(2) == 1);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(3) == 1);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(4) == 15);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(5) == 15);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(6) == 0);
}

static u32 encode_shift(u32 funct3, u32 funct7, u32 rd, u32 rs1, u32 shamt) {
    return OPCODE_OP_IMM | (rd << 7) | (funct3 << 12) | (rs1 << 15) |
           ((shamt & 0x1F) << 20) | (funct7 << 25);
}

static void test_shift_amount_masking() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 1)); // x1=1
    // slli x2, x1, 31 (valid max shift)
    iss.write_mem(0x20000004, 4, encode_shift(1, 0, 2, 1, 31));
    // srli x3, x1, 32 -> amount masked to 0
    iss.write_mem(0x20000008, 4, encode_shift(5, 0, 3, 1, 32));
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(2) == 0x80000000);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(3) == 1);
}

static void test_sra_sign() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_u(OPCODE_LUI, 1, 0x80000000)); // x1 = 0x80000000
    // srai x2, x1, 1
    iss.write_mem(0x20000004, 4, encode_shift(5, 0x20, 2, 1, 1));
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(2) == 0xC0000000);
}

// --- Upper immediate tests --------------------------------------------------

static void test_lui_auipc() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_u(OPCODE_LUI, 1, 0x12345000));
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(1) == 0x12345000);
    iss.write_mem(0x20000004, 4, encode_u(OPCODE_AUIPC, 2, 0x00001000));
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(2) == (0x20000004 + 0x00001000));
}

// --- Branch tests -----------------------------------------------------------

static void test_branch() {
    EmulatorISS iss;
    // addi x1, x0, 5; addi x2, x0, 5; beq x1, x2, +8; addi x3, x0, 1; addi x3, x0, 2
    iss.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 5));
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 2, 0, 5));
    iss.write_mem(0x20000008, 4, encode_b(0, 1, 2, 8)); // beq x1, x2, +8
    iss.write_mem(0x2000000C, 4, encode_i(OPCODE_OP_IMM, 0, 3, 0, 1)); // skipped
    iss.write_mem(0x20000010, 4, encode_i(OPCODE_OP_IMM, 0, 3, 0, 2)); // target
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.pc() == 0x20000010);
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(3) == 2);
}

static void test_branch_misaligned_target() {
    EmulatorISS iss;
    // addi x1, x0, 1; addi x2, x0, 1; beq x1, x2, +2 (misaligned)
    iss.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 1));
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 2, 0, 1));
    iss.write_mem(0x20000008, 4, encode_b(0, 1, 2, 2)); // target 0x2000000A is misaligned
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_INST_MISALIGNED);
    CHECK(iss.csr(CSR_MCAUSE) == CAUSE_INST_MISALIGNED);
}

// --- Jump tests -------------------------------------------------------------

static void test_jal_jalr() {
    EmulatorISS iss;
    // jal x1, +8 -> target 0x20000008
    iss.write_mem(0x20000000, 4, encode_j(1, 8));
    // addi x2, x0, 0x20000010
    iss.write_mem(0x20000008, 4, encode_u(OPCODE_LUI, 2, 0x20000000));
    iss.write_mem(0x2000000C, 4, encode_i(OPCODE_OP_IMM, 0, 2, 2, 0x10));
    // jalr x3, 4(x2) -> 0x20000014
    iss.write_mem(0x20000010, 4, encode_jalr(3, 2, 4));
    // target
    iss.write_mem(0x20000014, 4, encode_i(OPCODE_OP_IMM, 0, 4, 0, 7));

    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.pc() == 0x20000008);
    CHECK(iss.reg(1) == 0x20000004);
    CHECK(iss.step_inst(ev)); // lui
    CHECK(iss.step_inst(ev)); // addi
    CHECK(iss.step_inst(ev)); // jalr
    CHECK(iss.pc() == 0x20000014);
    CHECK(iss.reg(3) == 0x20000014);
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(4) == 7);
}

static void test_jal_misaligned_target() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_j(0, 2)); // target 0x20000002 misaligned
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_INST_MISALIGNED);
}

// --- Load/store tests -------------------------------------------------------

static void test_store_load() {
    EmulatorISS iss;
    // lui x3, 0x20000 -> x3 = 0x20000000 (RAM base)
    // addi x1, x0, 0xAB -> x1 = 0xAB
    // sw x1, 0(x3) -> mem[0x20000000] = 0xAB
    // lw x2, 0(x3) -> x2 = 0xAB
    iss.write_mem(0x20000000, 4, encode_u(OPCODE_LUI, 3, 0x20000000));
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 0xAB));
    iss.write_mem(0x20000008, 4, encode_s(2, 3, 1, 0));
    iss.write_mem(0x2000000C, 4, encode_i(OPCODE_LOAD, 2, 2, 3, 0));
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(2) == 0xAB);
}

static void test_load_store_sizes() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_u(OPCODE_LUI, 3, 0x20000000));
    // x1 = 0x1234A580 (negative byte and negative halfword)
    iss.write_mem(0x20000004, 4, encode_u(OPCODE_LUI, 1, 0x1234A000));
    iss.write_mem(0x20000008, 4, encode_i(OPCODE_OP_IMM, 0, 1, 1, 0x580));
    iss.write_mem(0x2000000C, 4, encode_s(2, 3, 1, 0)); // sw
    iss.write_mem(0x20000010, 4, encode_i(OPCODE_LOAD, 0, 4, 3, 0)); // lb sign-ext
    iss.write_mem(0x20000014, 4, encode_i(OPCODE_LOAD, 4, 5, 3, 0)); // lbu
    iss.write_mem(0x20000018, 4, encode_i(OPCODE_LOAD, 1, 6, 3, 0)); // lh sign-ext
    iss.write_mem(0x2000001C, 4, encode_i(OPCODE_LOAD, 5, 7, 3, 0)); // lhu
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(4) == 0xFFFFFF80);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(5) == 0x80);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(6) == 0xFFFFA580);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(7) == 0x0000A580);
}

static void test_load_misaligned() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_u(OPCODE_LUI, 3, 0x20000000));
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_LOAD, 2, 1, 3, 1)); // lw at +1
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_LOAD_MISALIGNED);
}

static void test_store_misaligned() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_u(OPCODE_LUI, 3, 0x20000000));
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 0xAB));
    iss.write_mem(0x20000008, 4, encode_s(2, 3, 1, 1)); // sw at +1
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_STORE_MISALIGNED);
}

static void test_strict_mem_fault() {
    Config cfg;
    cfg.strict_mem = true;
    EmulatorISS iss(cfg);
    iss.write_mem(0x20000000, 4, encode_u(OPCODE_LUI, 3, 0x30000000)); // outside RAM
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_LOAD, 2, 1, 3, 0)); // lw unmapped
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_LOAD_ACCESS_FAULT);
}

// --- CSR tests --------------------------------------------------------------

static void test_csr() {
    EmulatorISS iss;
    // csrrw x1, mstatus, x0 -> read mstatus into x1, write 0
    u32 csrw = encode_csr(0x001, 1, 0, CSR_MSTATUS); // funct3=001 (CSRRW)
    iss.write_mem(0x20000000, 4, csrw);
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.reg(1) == 0x00001800); // MPP=M
    CHECK(iss.csr(CSR_MSTATUS) == 0x00001800);
}

static void test_csr_all_ops() {
    EmulatorISS iss;
    // Use mtvec to test read-modify-write semantics (mtvec only masks low 2 bits).
    // csrrwi mtvec, 8: funct3=5.
    iss.write_mem(0x20000000, 4, encode_csr(0x5, 0, 0x8, CSR_MTVEC)); // imm=8 -> mtvec=8
    // csrrs x1, mtvec, x0: read, funct3=2.
    iss.write_mem(0x20000004, 4, encode_csr(0x2, 1, 0, CSR_MTVEC));
    // csrrc x2, mtvec, x0: read, funct3=3.
    iss.write_mem(0x20000008, 4, encode_csr(0x3, 2, 0, CSR_MTVEC));
    // Set x3 = 0x10.
    iss.write_mem(0x2000000C, 4, encode_i(OPCODE_OP_IMM, 0, 3, 0, 0x10));
    // csrrs x4, mtvec, x3: set bit 4, funct3=2.
    iss.write_mem(0x20000010, 4, encode_csr(0x2, 4, 3, CSR_MTVEC));
    // csrrc x5, mtvec, x3: clear bit 4, funct3=3.
    iss.write_mem(0x20000014, 4, encode_csr(0x3, 5, 3, CSR_MTVEC));
    // csrrsi mtvec, 0x4: set bit 2, funct3=6.
    iss.write_mem(0x20000018, 4, encode_csr(0x6, 0, 0x4, CSR_MTVEC));
    // csrrci mtvec, 0x4: clear bit 2, funct3=7.
    iss.write_mem(0x2000001C, 4, encode_csr(0x7, 6, 0x4, CSR_MTVEC));
    CommitEvent ev;
    CHECK(iss.step_inst(ev)); CHECK(iss.csr(CSR_MTVEC) == 8);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(1) == 8);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(2) == 8);
    CHECK(iss.step_inst(ev)); // x3 = 0x10
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(4) == 8); CHECK(iss.csr(CSR_MTVEC) == 0x18);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(5) == 0x18); CHECK(iss.csr(CSR_MTVEC) == 8);
    CHECK(iss.step_inst(ev)); CHECK(iss.csr(CSR_MTVEC) == 0xC);
    CHECK(iss.step_inst(ev)); CHECK(iss.reg(6) == 0xC); CHECK(iss.csr(CSR_MTVEC) == 8);
}

static void test_csr_illegal() {
    EmulatorISS iss;
    // Access unimplemented CSR 0xFC0.
    iss.write_mem(0x20000000, 4, encode_csr(0x002, 1, 0, 0xFC0)); // csrrs x1, 0xFC0, x0
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_ILLEGAL_INST);
}

// --- Exception and privileged tests -----------------------------------------

static void test_ecall() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, 0x00000073); // ecall
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_ECALL_M);
    CHECK(iss.csr(CSR_MEPC) == 0x20000000);
    CHECK(iss.csr(CSR_MCAUSE) == CAUSE_ECALL_M);
}

static void test_ebreak() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, 0x00100073); // ebreak
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_BREAKPOINT);
    CHECK(iss.hart().halted);
}

static void test_mret() {
    EmulatorISS iss;
    iss.reset(0x20000000);
    iss.write_mem(0x20000000, 4, encode_u(OPCODE_LUI, 1, 0x20000000)); // x1 = 0x20000000
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 1, 1, 0x100)); // x1 = 0x20000100
    iss.write_mem(0x20000008, 4, encode_csr(0x1, 0, 1, CSR_MEPC));        // csrrw mepc, x1
    iss.write_mem(0x2000000C, 4, 0x30200073); // mret
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    CHECK(iss.pc() == 0x20000100);
}

static void test_exception_priority_fetch_misaligned() {
    EmulatorISS iss;
    // Set PC to misaligned address. Fetch should fault before decode.
    iss.reset(0x20000001);
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_INST_MISALIGNED);
}

static void test_exception_priority_illegal_after_fetch() {
    EmulatorISS iss;
    // Write an illegal instruction at aligned address; should raise illegal, not access fault.
    iss.write_mem(0x20000000, 4, 0xFFFFFFFF);
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(ev.exception && ev.cause == CAUSE_ILLEGAL_INST);
}

// --- CLINT test --------------------------------------------------------------

static void test_clint() {
    EmulatorISS iss;
    // lui x2, 0x0200C gives x2 = 0x0200C000; lw x1, -8(x2) reads 0x0200BFF8.
    u32 lui_addr = encode_u(OPCODE_LUI, 2, 0x0200C000);
    u32 lw_mtime = encode_i(OPCODE_LOAD, 2, 1, 2, static_cast<u32>(-8));
    iss.write_mem(0x20000000, 4, lui_addr);
    iss.write_mem(0x20000004, 4, lw_mtime);
    CommitEvent ev;
    CHECK(iss.step_inst(ev)); // lui
    CHECK(iss.step_inst(ev)); // lw; reads mtime after first tick = 1
    CHECK(iss.reg(1) == 1);
}

// --- Difftest self-test ------------------------------------------------------

static void test_difftest_match() {
    EmulatorISS ref;
    EmulatorISS dut;
    // Program: addi x1, x0, 7; addi x2, x1, 3; ebreak
    ref.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 7));
    ref.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 2, 1, 3));
    ref.write_mem(0x20000008, 4, 0x00100073); // ebreak
    dut.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 7));
    dut.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 2, 1, 3));
    dut.write_mem(0x20000008, 4, 0x00100073); // ebreak

    Difftest diff(&ref, &dut);
    CHECK(diff.run(10));
    CHECK(diff.retire_index() == 3);
}

static void test_difftest_mismatch_detected() {
    EmulatorISS ref;
    EmulatorISS dut;
    // ref writes 7 to x1; dut writes 8 to x1.
    ref.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 7));
    dut.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 8));

    Difftest diff(&ref, &dut);
    CHECK(!diff.run(10));
    CHECK(!diff.last_mismatch().empty());
}

// --- Ring buffer test --------------------------------------------------------

static void test_ring_buffer() {
    EmulatorISS iss;
    iss.write_mem(0x20000000, 4, encode_i(OPCODE_OP_IMM, 0, 1, 0, 1));
    iss.write_mem(0x20000004, 4, encode_i(OPCODE_OP_IMM, 0, 2, 0, 2));
    CommitEvent ev;
    CHECK(iss.step_inst(ev));
    CHECK(iss.step_inst(ev));
    auto last = iss.last_events(5);
    CHECK(last.size() == 2);
    CHECK(last[0].rd == 2);
    CHECK(last[1].rd == 1);
}

// --- Main -------------------------------------------------------------------

int main() {
    test_decoder();
    test_rv32e_register_check();
    test_addi();
    test_alu_ops();
    test_shift_amount_masking();
    test_sra_sign();
    test_lui_auipc();
    test_branch();
    test_branch_misaligned_target();
    test_jal_jalr();
    test_jal_misaligned_target();
    test_store_load();
    test_load_store_sizes();
    test_load_misaligned();
    test_store_misaligned();
    test_strict_mem_fault();
    test_csr();
    test_csr_all_ops();
    test_csr_illegal();
    test_ecall();
    test_ebreak();
    test_mret();
    test_exception_priority_fetch_misaligned();
    test_exception_priority_illegal_after_fetch();
    test_clint();
    test_difftest_match();
    test_difftest_mismatch_detected();
    test_ring_buffer();

    if (failures == 0) {
        std::printf("All tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
