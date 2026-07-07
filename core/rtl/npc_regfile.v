// 16-entry register file for RV32E.
// Asynchronous read, synchronous write. x0 is hardwired to zero.

`include "npc_defines.vh"

module npc_regfile (
    input              clock,
    input              reset,

    input      [3:0]   rs1_addr,
    output     [31:0]  rs1_data,

    input      [3:0]   rs2_addr,
    output     [31:0]  rs2_data,

    input              wen,
    input      [3:0]   rd_addr,
    input      [31:0]  rd_data,

    // Debug read port
    input      [3:0]   debug_addr,
    output     [31:0]  debug_data
);

    reg [31:0] regs [0:`GPR_COUNT-1];

    assign rs1_data = (rs1_addr == 4'd0) ? 32'h0 : regs[rs1_addr];
    assign rs2_data = (rs2_addr == 4'd0) ? 32'h0 : regs[rs2_addr];
    assign debug_data = (debug_addr == 4'd0) ? 32'h0 : regs[debug_addr];

    integer i;
    always @(posedge clock or posedge reset) begin
        if (reset) begin
            for (i = 0; i < `GPR_COUNT; i = i + 1) begin
                regs[i] <= 32'h0;
            end
        end else if (wen && rd_addr != 4'd0) begin
            regs[rd_addr] <= rd_data;
        end
    end

endmodule
