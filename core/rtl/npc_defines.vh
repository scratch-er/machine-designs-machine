// NPC Core — global definitions and parameter defaults.
// This header is included by all Verilog modules in the core.

`ifndef NPC_DEFINES_VH
`define NPC_DEFINES_VH

//------------------------------------------------------------------------------
// Core configuration
//------------------------------------------------------------------------------
`ifndef RESET_VECTOR
`define RESET_VECTOR 32'h20000000
`endif

`ifndef CLINT_BASE
`define CLINT_BASE 32'h02000000
`endif

`ifndef CLINT_SIZE
`define CLINT_SIZE 32'h00010000
`endif

`ifndef GPR_COUNT
`define GPR_COUNT 16
`endif

`ifndef XLEN
`define XLEN 32
`endif

//------------------------------------------------------------------------------
// ALU operations
//------------------------------------------------------------------------------
`define ALU_ADD   4'b0000
`define ALU_SUB   4'b0001
`define ALU_SLT   4'b0010
`define ALU_SLTU  4'b0011
`define ALU_XOR   4'b0100
`define ALU_OR    4'b0101
`define ALU_AND   4'b0110
`define ALU_SLL   4'b0111
`define ALU_SRL   4'b1000
`define ALU_SRA   4'b1001

//------------------------------------------------------------------------------
// ALU source A select
//------------------------------------------------------------------------------
`define ALU_A_RS1  2'b00
`define ALU_A_PC   2'b01
`define ALU_A_ZERO 2'b10

//------------------------------------------------------------------------------
// ALU source B select
//------------------------------------------------------------------------------
`define ALU_B_RS2 1'b0
`define ALU_B_IMM 1'b1

//------------------------------------------------------------------------------
// Immediate type select
//------------------------------------------------------------------------------
`define IMM_I 3'b000
`define IMM_S 3'b001
`define IMM_B 3'b010
`define IMM_U 3'b011
`define IMM_J 3'b100
`define IMM_Z 3'b101   // CSR uimm[4:0], zero-extended

//------------------------------------------------------------------------------
// Branch condition type
//------------------------------------------------------------------------------
`define BR_NONE 3'b000
`define BR_BEQ  3'b001
`define BR_BNE  3'b010
`define BR_BLT  3'b011
`define BR_BGE  3'b100
`define BR_BLTU 3'b101
`define BR_BGEU 3'b110

//------------------------------------------------------------------------------
// CSR operation type
//------------------------------------------------------------------------------
`define CSR_OP_NONE 3'b000
`define CSR_OP_RW   3'b001
`define CSR_OP_RS   3'b010
`define CSR_OP_RC   3'b011
`define CSR_OP_RWI  3'b101
`define CSR_OP_RSI  3'b110
`define CSR_OP_RCI  3'b111

//------------------------------------------------------------------------------
// CSR source select
//------------------------------------------------------------------------------
`define CSR_SRC_RS1 1'b0
`define CSR_SRC_IMM 1'b1

//------------------------------------------------------------------------------
// Memory access size
//------------------------------------------------------------------------------
`define MEM_SIZE_BYTE 2'b00
`define MEM_SIZE_HALF 2'b01
`define MEM_SIZE_WORD 2'b10

//------------------------------------------------------------------------------
// RISC-V exception cause codes (mcause[30:0])
//------------------------------------------------------------------------------
`define CAUSE_INST_MISALIGNED  32'd0
`define CAUSE_INST_ACCESS_FAULT 32'd1
`define CAUSE_ILLEGAL_INST     32'd2
`define CAUSE_BREAKPOINT       32'd3
`define CAUSE_LOAD_MISALIGNED  32'd4
`define CAUSE_LOAD_ACCESS_FAULT 32'd5
`define CAUSE_STORE_MISALIGNED 32'd6
`define CAUSE_STORE_ACCESS_FAULT 32'd7
`define CAUSE_ECALL_M          32'd11

//------------------------------------------------------------------------------
// CSR addresses
//------------------------------------------------------------------------------
`define CSR_MVENDORID 12'hF11
`define CSR_MARCHID   12'hF12
`define CSR_MSTATUS   12'h300
`define CSR_MTVEC     12'h305
`define CSR_MEPC      12'h341
`define CSR_MCAUSE    12'h342

//------------------------------------------------------------------------------
// mstatus reset value: MPP = M-mode (bits [12:11] = 2'b11)
//------------------------------------------------------------------------------
`define MSTATUS_RESET 32'h00001800

//------------------------------------------------------------------------------
// CLINT register offsets within the CLINT region
//------------------------------------------------------------------------------
`define CLINT_MTIME_OFFSET  32'h0000_BFF8
`define CLINT_MTIMEH_OFFSET 32'h0000_BFFC

//------------------------------------------------------------------------------
// AXI4 burst types and response codes
//------------------------------------------------------------------------------
`define AXBURST_FIXED 2'b00
`define AXBURST_INCR  2'b01
`define AXBURST_WRAP  2'b10

`define AXRESP_OKAY   2'b00
`define AXRESP_EXOKAY 2'b01
`define AXRESP_SLVERR 2'b10
`define AXRESP_DECERR 2'b11

`endif // NPC_DEFINES_VH
