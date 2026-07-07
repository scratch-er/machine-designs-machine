// Combinational ALU for RV32E integer operations.

`include "npc_defines.vh"

module npc_alu (
    input      [31:0] rs1_data,
    input      [31:0] rs2_data,
    input      [31:0] pc,
    input      [31:0] imm,
    input      [3:0]  alu_op,
    input      [1:0]  alu_src_a,
    input             alu_src_b,
    output reg [31:0] alu_result
);

    reg [31:0] alu_op1;
    reg [31:0] alu_op2;

    always @(*) begin
        case (alu_src_a)
            `ALU_A_RS1:  alu_op1 = rs1_data;
            `ALU_A_PC:   alu_op1 = pc;
            `ALU_A_ZERO: alu_op1 = 32'h0;
            default:     alu_op1 = 32'h0;
        endcase

        case (alu_src_b)
            `ALU_B_RS2: alu_op2 = rs2_data;
            `ALU_B_IMM: alu_op2 = imm;
            default:    alu_op2 = imm;
        endcase

        case (alu_op)
            `ALU_ADD:  alu_result = alu_op1 + alu_op2;
            `ALU_SUB:  alu_result = alu_op1 - alu_op2;
            `ALU_SLT:  alu_result = ($signed(alu_op1) < $signed(alu_op2)) ? 32'h1 : 32'h0;
            `ALU_SLTU: alu_result = (alu_op1 < alu_op2) ? 32'h1 : 32'h0;
            `ALU_XOR:  alu_result = alu_op1 ^ alu_op2;
            `ALU_OR:   alu_result = alu_op1 | alu_op2;
            `ALU_AND:  alu_result = alu_op1 & alu_op2;
            `ALU_SLL:  alu_result = alu_op1 << alu_op2[4:0];
            `ALU_SRL:  alu_result = alu_op1 >> alu_op2[4:0];
            `ALU_SRA:  alu_result = $signed(alu_op1) >>> alu_op2[4:0];
            default:   alu_result = 32'h0;
        endcase
    end

endmodule
