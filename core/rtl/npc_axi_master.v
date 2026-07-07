// Single-cycle AXI4 master bridge for the baseline core.
//
// The current datapath still retires one instruction per cycle, so this bridge
// uses the zero-latency AXI timing planned for the baseline: the testbench keeps
// address/data ready high and returns R/B responses combinationally in the same
// cycle as the request. The signal set is real AXI4 and is reusable by the later
// pipelined implementation, which will replace this bridge with a stateful
// transaction sequencer.

`include "npc_defines.vh"

module npc_axi_master (
    input             req_fetch_valid,
    input      [31:0] req_fetch_addr,
    output     [31:0] resp_fetch_inst,
    output            resp_fetch_fault,
    output            resp_fetch_misaligned,

    input             req_load_valid,
    input      [31:0] req_load_addr,
    input      [1:0]  req_load_size,
    output     [31:0] resp_load_data,
    output            resp_load_fault,
    output            resp_load_misaligned,

    input             req_store_valid,
    input      [31:0] req_store_addr,
    input      [1:0]  req_store_size,
    input      [31:0] req_store_data,
    output            resp_store_fault,
    output            resp_store_misaligned,

    input             io_master_awready,
    output            io_master_awvalid,
    output     [31:0] io_master_awaddr,
    output     [3:0]  io_master_awid,
    output     [7:0]  io_master_awlen,
    output     [2:0]  io_master_awsize,
    output     [1:0]  io_master_awburst,
    input             io_master_wready,
    output            io_master_wvalid,
    output     [31:0] io_master_wdata,
    output     [3:0]  io_master_wstrb,
    output            io_master_wlast,
    output            io_master_bready,
    input             io_master_bvalid,
    input      [1:0]  io_master_bresp,
    input      [3:0]  io_master_bid,
    input             io_master_arready,
    output            io_master_arvalid,
    output     [31:0] io_master_araddr,
    output     [3:0]  io_master_arid,
    output     [7:0]  io_master_arlen,
    output     [2:0]  io_master_arsize,
    output     [1:0]  io_master_arburst,
    output            io_master_rready,
    input             io_master_rvalid,
    input      [1:0]  io_master_rresp,
    input      [31:0] io_master_rdata,
    input             io_master_rlast,
    input      [3:0]  io_master_rid
);

    localparam [3:0] AXI_ID_FETCH = 4'h0;
    localparam [3:0] AXI_ID_DATA  = 4'h1;

    wire        data_read_active = req_load_valid;
    wire        fetch_active = req_fetch_valid && !data_read_active;
    wire [31:0] read_addr = data_read_active ? req_load_addr : req_fetch_addr;
    wire [1:0]  read_size = data_read_active ? req_load_size : `MEM_SIZE_WORD;
    wire [3:0]  read_id   = data_read_active ? AXI_ID_DATA : AXI_ID_FETCH;

    assign io_master_arvalid = data_read_active || fetch_active;
    assign io_master_araddr  = read_addr;
    assign io_master_arid    = read_id;
    assign io_master_arlen   = 8'h00;
    assign io_master_arsize  = {1'b0, read_size};
    assign io_master_arburst = `AXBURST_INCR;
    assign io_master_rready  = 1'b1;

    assign io_master_awvalid = req_store_valid;
    assign io_master_awaddr  = req_store_addr;
    assign io_master_awid    = AXI_ID_DATA;
    assign io_master_awlen   = 8'h00;
    assign io_master_awsize  = {1'b0, req_store_size};
    assign io_master_awburst = `AXBURST_INCR;

    assign io_master_wvalid = req_store_valid;
    assign io_master_wlast  = 1'b1;
    assign io_master_bready = 1'b1;

    reg [3:0]  store_wstrb;
    reg [31:0] store_wdata_shifted;
    always @(*) begin
        case (req_store_size)
            `MEM_SIZE_BYTE: begin
                store_wstrb = 4'b0001 << req_store_addr[1:0];
                store_wdata_shifted = {4{req_store_data[7:0]}} << (8 * req_store_addr[1:0]);
            end
            `MEM_SIZE_HALF: begin
                store_wstrb = req_store_addr[1] ? 4'b1100 : 4'b0011;
                store_wdata_shifted = {2{req_store_data[15:0]}} << (8 * req_store_addr[1:0]);
            end
            default: begin
                store_wstrb = 4'b1111;
                store_wdata_shifted = req_store_data;
            end
        endcase
    end

    assign io_master_wstrb = store_wstrb;
    assign io_master_wdata = store_wdata_shifted;

    wire read_accepted = io_master_arvalid && io_master_arready &&
                         io_master_rvalid && io_master_rlast &&
                         (io_master_rid == read_id);
    wire read_fault = !read_accepted ||
                      (io_master_rresp == `AXRESP_SLVERR) ||
                      (io_master_rresp == `AXRESP_DECERR);

    assign resp_fetch_inst       = fetch_active ? io_master_rdata : 32'h0;
    assign resp_fetch_fault      = fetch_active && read_fault;
    assign resp_fetch_misaligned = req_fetch_valid && (req_fetch_addr[1:0] != 2'b00);

    assign resp_load_data       = data_read_active ? io_master_rdata : 32'h0;
    assign resp_load_fault      = data_read_active && read_fault;
    assign resp_load_misaligned = 1'b0;

    wire write_accepted = req_store_valid && io_master_awready && io_master_wready &&
                          io_master_bvalid && (io_master_bid == AXI_ID_DATA);
    assign resp_store_fault = req_store_valid &&
                              (!write_accepted ||
                               io_master_bresp == `AXRESP_SLVERR ||
                               io_master_bresp == `AXRESP_DECERR);
    assign resp_store_misaligned = 1'b0;

endmodule
