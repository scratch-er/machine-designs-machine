// RV32E_Zicsr instruction decoder.

`include "npc_defines.vh"

module npc_decoder (
    input      [31:0] inst,

    output     [4:0] rd5,
    output     [4:0] rs15,
    output     [4:0] rs25,
    output     [3:0] rd,
    output     [3:0] rs1,
    output     [3:0] rs2,
    output     [11:0] csr_addr,

    output reg        ctrl_illegal,
    output reg        ctrl_ecall,
    output reg        ctrl_ebreak,
    output reg        ctrl_mret,
    output reg        ctrl_wfi,
    output reg        ctrl_fence_i,
    output reg [3:0]  ctrl_alu_op,
    output reg [1:0]  ctrl_alu_src_a,
    output reg        ctrl_alu_src_b,
    output reg [2:0]  ctrl_imm_sel,
    output reg        ctrl_reg_write,
    output reg        ctrl_mem_read,
    output reg        ctrl_mem_write,
    output reg [1:0]  ctrl_mem_size,
    output reg        ctrl_mem_sext,
    output reg [2:0]  ctrl_branch_type,
    output reg        ctrl_jump,
    output reg        ctrl_jump_reg,
    output reg [2:0]  ctrl_csr_op,
    output reg        ctrl_csr_src,

    output reg        dec_has_rd,
    output reg        dec_has_rs1,
    output reg        dec_has_rs2
);

    wire [6:0]  opcode = inst[6:0];
    wire [2:0]  funct3 = inst[14:12];
    wire [6:0]  funct7 = inst[31:25];
    wire [11:0] funct12 = inst[31:20];

    assign rd5 = inst[11:7];
    assign rs15 = inst[19:15];
    assign rs25 = inst[24:20];
    assign rd = rd5[3:0];
    assign rs1 = rs15[3:0];
    assign rs2 = rs25[3:0];
    assign csr_addr = inst[31:20];

    always @(*) begin
        ctrl_illegal     = 1'b0;
        ctrl_ecall       = 1'b0;
        ctrl_ebreak      = 1'b0;
        ctrl_mret        = 1'b0;
        ctrl_wfi         = 1'b0;
        ctrl_fence_i     = 1'b0;
        ctrl_alu_op      = `ALU_ADD;
        ctrl_alu_src_a   = `ALU_A_RS1;
        ctrl_alu_src_b   = `ALU_B_IMM;
        ctrl_imm_sel     = `IMM_I;
        ctrl_reg_write   = 1'b0;
        ctrl_mem_read    = 1'b0;
        ctrl_mem_write   = 1'b0;
        ctrl_mem_size    = `MEM_SIZE_BYTE;
        ctrl_mem_sext    = 1'b0;
        ctrl_branch_type = `BR_NONE;
        ctrl_jump        = 1'b0;
        ctrl_jump_reg    = 1'b0;
        ctrl_csr_op      = `CSR_OP_NONE;
        ctrl_csr_src     = `CSR_SRC_RS1;
        dec_has_rd       = 1'b0;
        dec_has_rs1      = 1'b0;
        dec_has_rs2      = 1'b0;

        case (opcode)
            7'b0010011: begin // OP-IMM
                dec_has_rd = 1'b1;
                dec_has_rs1 = 1'b1;
                ctrl_reg_write = 1'b1;
                case (funct3)
                    3'b000: ctrl_alu_op = `ALU_ADD;
                    3'b010: ctrl_alu_op = `ALU_SLT;
                    3'b011: ctrl_alu_op = `ALU_SLTU;
                    3'b100: ctrl_alu_op = `ALU_XOR;
                    3'b110: ctrl_alu_op = `ALU_OR;
                    3'b111: ctrl_alu_op = `ALU_AND;
                    3'b001: ctrl_alu_op = (funct7 == 7'b0000000) ? `ALU_SLL : `ALU_ADD;
                    3'b101: begin
                        if (funct7 == 7'b0000000) ctrl_alu_op = `ALU_SRL;
                        else if (funct7 == 7'b0100000) ctrl_alu_op = `ALU_SRA;
                        else ctrl_illegal = 1'b1;
                    end
                    default: ctrl_illegal = 1'b1;
                endcase
                if (funct3 == 3'b001 && funct7 != 7'b0000000) ctrl_illegal = 1'b1;
            end

            7'b0110011: begin // OP
                dec_has_rd = 1'b1;
                dec_has_rs1 = 1'b1;
                dec_has_rs2 = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_alu_src_b = `ALU_B_RS2;
                case (funct3)
                    3'b000: begin
                        if (funct7 == 7'b0000000) ctrl_alu_op = `ALU_ADD;
                        else if (funct7 == 7'b0100000) ctrl_alu_op = `ALU_SUB;
                        else ctrl_illegal = 1'b1;
                    end
                    3'b001: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b010: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b011: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b100: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b101: begin
                        if (funct7 == 7'b0000000) ctrl_alu_op = `ALU_SRL;
                        else if (funct7 == 7'b0100000) ctrl_alu_op = `ALU_SRA;
                        else ctrl_illegal = 1'b1;
                    end
                    3'b110: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b111: ctrl_illegal = (funct7 != 7'b0000000);
                    default: ctrl_illegal = 1'b1;
                endcase
                if (!ctrl_illegal) begin
                    case (funct3)
                        3'b001: ctrl_alu_op = `ALU_SLL;
                        3'b010: ctrl_alu_op = `ALU_SLT;
                        3'b011: ctrl_alu_op = `ALU_SLTU;
                        3'b100: ctrl_alu_op = `ALU_XOR;
                        3'b110: ctrl_alu_op = `ALU_OR;
                        3'b111: ctrl_alu_op = `ALU_AND;
                        default: ;
                    endcase
                end
            end

            7'b0110111: begin // LUI
                dec_has_rd = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_alu_src_a = `ALU_A_ZERO;
                ctrl_imm_sel = `IMM_U;
            end

            7'b0010111: begin // AUIPC
                dec_has_rd = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_alu_src_a = `ALU_A_PC;
                ctrl_imm_sel = `IMM_U;
            end

            7'b1101111: begin // JAL
                dec_has_rd = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_alu_src_a = `ALU_A_PC;
                ctrl_imm_sel = `IMM_J;
                ctrl_jump = 1'b1;
            end

            7'b1100111: begin // JALR
                if (funct3 == 3'b000) begin
                    dec_has_rd = 1'b1;
                    dec_has_rs1 = 1'b1;
                    ctrl_reg_write = 1'b1;
                    ctrl_alu_src_a = `ALU_A_RS1;
                    ctrl_imm_sel = `IMM_I;
                    ctrl_jump = 1'b1;
                    ctrl_jump_reg = 1'b1;
                end else begin
                    ctrl_illegal = 1'b1;
                end
            end

            7'b1100011: begin // BRANCH
                dec_has_rs1 = 1'b1;
                dec_has_rs2 = 1'b1;
                ctrl_alu_src_b = `ALU_B_RS2;
                ctrl_imm_sel = `IMM_B;
                case (funct3)
                    3'b000: ctrl_branch_type = `BR_BEQ;
                    3'b001: ctrl_branch_type = `BR_BNE;
                    3'b100: ctrl_branch_type = `BR_BLT;
                    3'b101: ctrl_branch_type = `BR_BGE;
                    3'b110: ctrl_branch_type = `BR_BLTU;
                    3'b111: ctrl_branch_type = `BR_BGEU;
                    default: ctrl_illegal = 1'b1;
                endcase
            end

            7'b0000011: begin // LOAD
                dec_has_rd = 1'b1;
                dec_has_rs1 = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_mem_read = 1'b1;
                ctrl_mem_size = funct3[1:0];
                ctrl_mem_sext = ~funct3[2];
                if (funct3[1:0] == 2'b11 || funct3 == 3'b010 && funct3[2]) ctrl_illegal = 1'b1;
                if (funct3 == 3'b110 || funct3 == 3'b111) ctrl_illegal = 1'b1;
            end

            7'b0100011: begin // STORE
                dec_has_rs1 = 1'b1;
                dec_has_rs2 = 1'b1;
                ctrl_mem_write = 1'b1;
                ctrl_mem_size = funct3[1:0];
                ctrl_imm_sel = `IMM_S;
                if (funct3[2] == 1'b1 || funct3[1:0] == 2'b11) ctrl_illegal = 1'b1;
            end

            7'b0001111: begin // MISC-MEM
                if (funct3 == 3'b000) begin
                    // fence: nop
                end else if (funct3 == 3'b001 && inst[31:15] == 17'h00000) begin
                    ctrl_fence_i = 1'b1;
                end else begin
                    ctrl_illegal = 1'b1;
                end
            end

            7'b1110011: begin // SYSTEM
                if (funct3 == 3'b000) begin
                    case (funct12)
                        12'h000: ctrl_ecall = 1'b1;
                        12'h001: ctrl_ebreak = 1'b1;
                        12'h302: ctrl_mret = 1'b1;
                        12'h105: ctrl_wfi = 1'b1;
                        default: ctrl_illegal = 1'b1;
                    endcase
                end else begin
                    dec_has_rd = 1'b1;
                    ctrl_reg_write = 1'b1;
                    ctrl_csr_src = funct3[2];
                    case (funct3)
                        3'b001: begin ctrl_csr_op = `CSR_OP_RW;  dec_has_rs1 = 1'b1; end
                        3'b010: begin ctrl_csr_op = `CSR_OP_RS;  dec_has_rs1 = 1'b1; end
                        3'b011: begin ctrl_csr_op = `CSR_OP_RC;  dec_has_rs1 = 1'b1; end
                        3'b101: begin ctrl_csr_op = `CSR_OP_RWI; ctrl_imm_sel = `IMM_Z; end
                        3'b110: begin ctrl_csr_op = `CSR_OP_RSI; ctrl_imm_sel = `IMM_Z; end
                        3'b111: begin ctrl_csr_op = `CSR_OP_RCI; ctrl_imm_sel = `IMM_Z; end
                        default: ctrl_illegal = 1'b1;
                    endcase
                end
            end

            default: ctrl_illegal = 1'b1;
        endcase
    end

endmodule
