// NPC Core top module.
//
// Phase 1: single-cycle baseline with DPI-C memory backend. The AXI4 master is
// reserved/tied off; it will be wired up in the pipelined implementation.

`include "npc_defines.vh"

module npc_core (
    input              clock,
    input              reset,
    input              io_interrupt,

    // AXI4 master (reserved / tied off in single-cycle baseline)
    input              io_master_awready,
    output             io_master_awvalid,
    output     [31:0]  io_master_awaddr,
    output     [3:0]   io_master_awid,
    output     [7:0]   io_master_awlen,
    output     [2:0]   io_master_awsize,
    output     [1:0]   io_master_awburst,
    input              io_master_wready,
    output             io_master_wvalid,
    output     [31:0]  io_master_wdata,
    output     [3:0]   io_master_wstrb,
    output             io_master_wlast,
    output             io_master_bready,
    input              io_master_bvalid,
    input      [1:0]   io_master_bresp,
    input      [3:0]   io_master_bid,
    input              io_master_arready,
    output             io_master_arvalid,
    output     [31:0]  io_master_araddr,
    output     [3:0]   io_master_arid,
    output     [7:0]   io_master_arlen,
    output     [2:0]   io_master_arsize,
    output     [1:0]   io_master_arburst,
    output             io_master_rready,
    input              io_master_rvalid,
    input      [1:0]   io_master_rresp,
    input      [31:0]  io_master_rdata,
    input              io_master_rlast,
    input      [3:0]   io_master_rid,

    // AXI4 slave (reserved; outputs hardcoded to 0)
    output             io_slave_awready,
    input              io_slave_awvalid,
    input      [31:0]  io_slave_awaddr,
    input      [3:0]   io_slave_awid,
    input      [7:0]   io_slave_awlen,
    input      [2:0]   io_slave_awsize,
    input      [1:0]   io_slave_awburst,
    output             io_slave_wready,
    input              io_slave_wvalid,
    input      [31:0]  io_slave_wdata,
    input      [3:0]   io_slave_wstrb,
    input              io_slave_wlast,
    input              io_slave_bready,
    output             io_slave_bvalid,
    output     [1:0]   io_slave_bresp,
    output     [3:0]   io_slave_bid,
    output             io_slave_arready,
    input              io_slave_arvalid,
    input      [31:0]  io_slave_araddr,
    input      [3:0]   io_slave_arid,
    input      [7:0]   io_slave_arlen,
    input      [2:0]   io_slave_arsize,
    input      [1:0]   io_slave_arburst,
    output             io_slave_rready,
    input              io_slave_rvalid,
    output     [1:0]   io_slave_rresp,
    output     [31:0]  io_slave_rdata,
    output             io_slave_rlast,
    output     [3:0]   io_slave_rid,

    // Commit interface (required by difftest)
    output             commit_valid,
    output     [31:0]  commit_pc,
    output     [31:0]  commit_inst,
    output     [4:0]   commit_rd,
    output     [31:0]  commit_rd_value,
    output             commit_exception,
    output     [31:0]  commit_cause,
    output     [31:0]  commit_next_pc,

`ifdef VERILATOR
    // Debug ports (only visible under Verilator)
    output     [31:0]  debug_pc,
    output     [31:0]  debug_inst,
    input      [3:0]   debug_reg_addr,
    output     [31:0]  debug_reg_data,
    input      [11:0]  debug_csr_addr,
    output     [31:0]  debug_csr_data
`endif
);

    // Tie off reserved AXI master outputs.
    assign io_master_awvalid = 1'b0;
    assign io_master_awaddr  = 32'h0;
    assign io_master_awid    = 4'h0;
    assign io_master_awlen   = 8'h0;
    assign io_master_awsize  = 3'h0;
    assign io_master_awburst = 2'h0;
    assign io_master_wvalid  = 1'b0;
    assign io_master_wdata   = 32'h0;
    assign io_master_wstrb   = 4'h0;
    assign io_master_wlast   = 1'b0;
    assign io_master_bready  = 1'b0;
    assign io_master_arvalid = 1'b0;
    assign io_master_araddr  = 32'h0;
    assign io_master_arid    = 4'h0;
    assign io_master_arlen   = 8'h0;
    assign io_master_arsize  = 3'h0;
    assign io_master_arburst = 2'h0;
    assign io_master_rready  = 1'b0;

    // Tie off reserved AXI slave outputs.
    assign io_slave_awready = 1'b0;
    assign io_slave_wready  = 1'b0;
    assign io_slave_bvalid  = 1'b0;
    assign io_slave_bresp   = 2'h0;
    assign io_slave_bid     = 4'h0;
    assign io_slave_arready = 1'b0;
    assign io_slave_rready  = 1'b0;
    assign io_slave_rresp   = 2'h0;
    assign io_slave_rdata   = 32'h0;
    assign io_slave_rlast   = 1'b0;
    assign io_slave_rid     = 4'h0;

    // Unused reserved input.
    wire _unused_io_interrupt = io_interrupt;

    // Wires between the datapath and the DPI-C memory.
    wire        req_fetch_valid;
    wire [31:0] req_fetch_addr;
    wire [31:0] resp_fetch_inst;
    wire        resp_fetch_fault;
    wire        resp_fetch_misaligned;

    wire        req_load_valid;
    wire [31:0] req_load_addr;
    wire [1:0]  req_load_size;
    wire [31:0] resp_load_data;
    wire        resp_load_fault;
    wire        resp_load_misaligned;

    wire        req_store_valid;
    wire [31:0] req_store_addr;
    wire [1:0]  req_store_size;
    wire [31:0] req_store_data;
    wire        resp_store_fault;
    wire        resp_store_misaligned;

`ifndef VERILATOR
    // When not under Verilator, debug inputs are unused and outputs are zero.
    wire [3:0]  debug_reg_addr = 4'h0;
    wire [11:0] debug_csr_addr = 12'h0;
    wire [31:0] debug_pc_unused;
    wire [31:0] debug_inst_unused;
    wire [31:0] debug_reg_data_unused;
    wire [31:0] debug_csr_data_unused;
`endif

    npc_single_cycle core (
        .clock                  (clock),
        .reset                  (reset),
        .req_fetch_valid        (req_fetch_valid),
        .req_fetch_addr         (req_fetch_addr),
        .resp_fetch_inst        (resp_fetch_inst),
        .resp_fetch_fault       (resp_fetch_fault),
        .resp_fetch_misaligned  (resp_fetch_misaligned),
        .req_load_valid         (req_load_valid),
        .req_load_addr          (req_load_addr),
        .req_load_size          (req_load_size),
        .resp_load_data         (resp_load_data),
        .resp_load_fault        (resp_load_fault),
        .resp_load_misaligned   (resp_load_misaligned),
        .req_store_valid        (req_store_valid),
        .req_store_addr         (req_store_addr),
        .req_store_size         (req_store_size),
        .req_store_data         (req_store_data),
        .resp_store_fault       (resp_store_fault),
        .resp_store_misaligned  (resp_store_misaligned),
        .commit_valid           (commit_valid),
        .commit_pc              (commit_pc),
        .commit_inst            (commit_inst),
        .commit_rd              (commit_rd),
        .commit_rd_value        (commit_rd_value),
        .commit_exception       (commit_exception),
        .commit_cause           (commit_cause),
        .commit_next_pc         (commit_next_pc),
`ifdef VERILATOR
        .debug_pc               (debug_pc),
        .debug_inst             (debug_inst),
        .debug_reg_addr         (debug_reg_addr),
        .debug_reg_data         (debug_reg_data),
        .debug_csr_addr         (debug_csr_addr),
        .debug_csr_data         (debug_csr_data)
`else
        .debug_pc               (debug_pc_unused),
        .debug_inst             (debug_inst_unused),
        .debug_reg_addr         (debug_reg_addr),
        .debug_reg_data         (debug_reg_data_unused),
        .debug_csr_addr         (debug_csr_addr),
        .debug_csr_data         (debug_csr_data_unused)
`endif
    );

    npc_memory_dpi memory_dpi (
        .clock                  (clock),
        .reset                  (reset),
        .req_fetch_valid        (req_fetch_valid),
        .req_fetch_addr         (req_fetch_addr),
        .resp_fetch_inst        (resp_fetch_inst),
        .resp_fetch_fault       (resp_fetch_fault),
        .resp_fetch_misaligned  (resp_fetch_misaligned),
        .req_load_valid         (req_load_valid),
        .req_load_addr          (req_load_addr),
        .req_load_size          (req_load_size),
        .resp_load_data         (resp_load_data),
        .resp_load_fault        (resp_load_fault),
        .resp_load_misaligned   (resp_load_misaligned),
        .req_store_valid        (req_store_valid),
        .req_store_addr         (req_store_addr),
        .req_store_size         (req_store_size),
        .req_store_data         (req_store_data),
        .resp_store_fault       (resp_store_fault),
        .resp_store_misaligned  (resp_store_misaligned)
    );

endmodule
