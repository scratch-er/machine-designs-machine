// NPC DPI-C memory interface.
//
// This module wraps the DPI-C functions that let the RTL read and write the
// emulator's memory model. Reads are pure and may be called from combinational
// logic. Writes must only be called from clocked logic.
//
// The read functions return a 64-bit packed value:
//   bits [31:0]  : data (little-endian)
//   bit  [32]    : access fault
//   bit  [33]    : misaligned fault

`include "npc_defines.vh"

module npc_memory_dpi (
    input              clock,
    input              reset,

    // Fetch port (combinational)
    input              req_fetch_valid,
    input      [31:0]  req_fetch_addr,
    output reg [31:0]  resp_fetch_inst,
    output reg         resp_fetch_fault,
    output reg         resp_fetch_misaligned,

    // Load port (combinational)
    input              req_load_valid,
    input      [31:0]  req_load_addr,
    input      [1:0]   req_load_size,
    output reg [31:0]  resp_load_data,
    output reg         resp_load_fault,
    output reg         resp_load_misaligned,

    // Store port (registered on posedge clock)
    input              req_store_valid,
    input      [31:0]  req_store_addr,
    input      [1:0]   req_store_size,
    input      [31:0]  req_store_data,
    output reg         resp_store_fault,
    output reg         resp_store_misaligned
);

    import "DPI-C" pure function longint unsigned npc_dpi_mem_fetch(input int unsigned addr);
    import "DPI-C" pure function longint unsigned npc_dpi_mem_load(input int unsigned addr, input int unsigned size);
    import "DPI-C" function void npc_dpi_mem_store(input int unsigned addr, input int unsigned size, input int unsigned data);

    always @(*) begin
        resp_fetch_inst      = 32'h0;
        resp_fetch_fault     = 1'b0;
        resp_fetch_misaligned= 1'b0;
        if (req_fetch_valid && !reset) begin
            automatic longint unsigned r = npc_dpi_mem_fetch(req_fetch_addr);
            resp_fetch_inst       = r[31:0];
            resp_fetch_fault      = r[32];
            resp_fetch_misaligned = r[33];
        end
    end

    always @(*) begin
        resp_load_data       = 32'h0;
        resp_load_fault      = 1'b0;
        resp_load_misaligned = 1'b0;
        if (req_load_valid && !reset) begin
            automatic longint unsigned r = npc_dpi_mem_load(req_load_addr, {30'b0, req_load_size});
            resp_load_data       = r[31:0];
            resp_load_fault      = r[32];
            resp_load_misaligned = r[33];
        end
    end

    always @(posedge clock or posedge reset) begin
        if (reset) begin
            resp_store_fault      <= 1'b0;
            resp_store_misaligned <= 1'b0;
        end else begin
            resp_store_fault      <= 1'b0;
            resp_store_misaligned <= 1'b0;
            if (req_store_valid) begin
                npc_dpi_mem_store(req_store_addr, {30'b0, req_store_size}, req_store_data);
            end
        end
    end

endmodule
