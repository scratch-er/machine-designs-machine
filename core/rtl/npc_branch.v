// Branch and jump target logic.

`include "npc_defines.vh"

module npc_branch (
    input      [31:0] pc,
    input      [31:0] imm,
    input      [31:0] rs1_data,
    input      [31:0] rs2_data,
    input      [2:0]  branch_type,
    input             jump_reg,

    output reg        branch_taken,
    output reg [31:0] branch_target,
    output reg [31:0] jump_target,
    output reg        branch_target_misaligned,
    output reg        jump_target_misaligned
);

    always @(*) begin
        branch_target = pc + imm;
        jump_target = jump_reg ? ((rs1_data + imm) & 32'hFFFF_FFFE) : (pc + imm);
        jump_target_misaligned = (jump_target[1:0] != 2'b00);

        case (branch_type)
            `BR_BEQ:  branch_taken = (rs1_data == rs2_data);
            `BR_BNE:  branch_taken = (rs1_data != rs2_data);
            `BR_BLT:  branch_taken = ($signed(rs1_data) < $signed(rs2_data));
            `BR_BGE:  branch_taken = ($signed(rs1_data) >= $signed(rs2_data));
            `BR_BLTU: branch_taken = (rs1_data < rs2_data);
            `BR_BGEU: branch_taken = (rs1_data >= rs2_data);
            default:  branch_taken = 1'b0;
        endcase

        branch_target_misaligned = branch_taken && (branch_target[1:0] != 2'b00);
    end

endmodule
