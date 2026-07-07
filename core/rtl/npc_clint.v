// Built-in CLINT. Implements mtime/mtimeh only; other registers are ignored.
// mtime increments by 1 every clock cycle.

`include "npc_defines.vh"

module npc_clint (
    input              clock,
    input              reset,

    // Memory-mapped access (full 32-bit address)
    input              req_valid,
    input              req_wen,
    input      [31:0]  req_addr,
    input      [31:0]  req_wdata,
    output reg [31:0]  resp_rdata,
    output reg         resp_fault
);

    reg [63:0] mtime;

    wire [31:0] offset = req_addr - `CLINT_BASE;
    wire is_mtime  = (offset == `CLINT_MTIME_OFFSET);
    wire is_mtimeh = (offset == `CLINT_MTIMEH_OFFSET);

    always @(*) begin
        resp_fault = 1'b0;
        if (req_valid && !req_wen) begin
            if (is_mtime)      resp_rdata = mtime[31:0];
            else if (is_mtimeh) resp_rdata = mtime[63:32];
            else                resp_rdata = 32'h0;  // msip, mtimecmp: read undefined
        end else begin
            resp_rdata = 32'h0;
        end
    end

    always @(posedge clock or posedge reset) begin
        if (reset) begin
            mtime <= 64'h0;
        end else begin
            if (req_valid && req_wen) begin
                if (is_mtime)       mtime <= {mtime[63:32], req_wdata} + 64'h1;
                else if (is_mtimeh) mtime <= {req_wdata, mtime[31:0]} + 64'h1;
                else                mtime <= mtime + 64'h1;
            end else begin
                mtime <= mtime + 64'h1;
            end
        end
    end

endmodule
