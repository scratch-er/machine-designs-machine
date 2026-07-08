// AXI4 master bridge for the NPC core.
//
// The bridge accepts one transaction at a time. I-cache fills use a four-beat
// read burst on ID 0. Data loads/stores use single-beat transactions on ID 1.

`include "npc_defines.vh"

module npc_axi_master (
    input             clock,
    input             reset,

    input             req_fetch_valid,
    input      [31:0] req_fetch_addr,
    output            req_fetch_ready,
    output            resp_fetch_valid,
    output     [31:0] resp_fetch_data,
    output            resp_fetch_last,
    output            resp_fetch_fault,

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

    localparam STATE_IDLE       = 3'd0;
    localparam STATE_IF_AR      = 3'd1;
    localparam STATE_IF_R       = 3'd2;
    localparam STATE_DATA_AR    = 3'd3;
    localparam STATE_DATA_R     = 3'd4;
    localparam STATE_DATA_W     = 3'd5;
    localparam STATE_DATA_B     = 3'd6;

    reg [2:0] state;
    reg [31:0] addr_reg;
    reg [1:0]  size_reg;
    reg [31:0] store_data_reg;
    reg        data_read_fault_reg;
    reg        data_write_fault_reg;

    wire data_load_active = (state == STATE_IDLE) && req_load_valid;
    wire data_store_active = (state == STATE_IDLE) && !req_load_valid && req_store_valid;
    wire start_fetch = (state == STATE_IDLE) && !req_load_valid && !req_store_valid && req_fetch_valid;

    assign req_fetch_ready = (state == STATE_IF_AR) && io_master_arready;

    assign io_master_arvalid = (state == STATE_IF_AR) || data_load_active;
    assign io_master_araddr  = data_load_active ? req_load_addr : addr_reg;
    assign io_master_arid    = data_load_active ? AXI_ID_DATA : AXI_ID_FETCH;
    assign io_master_arlen   = 8'h00 | ((state == STATE_IF_AR) ? 8'h03 : 8'h00);
    assign io_master_arsize  = data_load_active ? {1'b0, req_load_size} : 3'b010;
    assign io_master_arburst = `AXBURST_INCR;
    assign io_master_rready  = (state == STATE_IF_R) || data_load_active;

    assign io_master_awvalid = data_store_active;
    assign io_master_awaddr  = req_store_addr;
    assign io_master_awid    = AXI_ID_DATA;
    assign io_master_awlen   = 8'h00;
    assign io_master_awsize  = {1'b0, req_store_size};
    assign io_master_awburst = `AXBURST_INCR;

    assign io_master_wvalid = data_store_active;
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

    wire r_fetch_beat = (state == STATE_IF_R) && io_master_rvalid && (io_master_rid == AXI_ID_FETCH);
    wire r_data_beat = data_load_active && io_master_rvalid && (io_master_rid == AXI_ID_DATA);
    wire b_data_beat = (state == STATE_DATA_B) && io_master_bvalid && (io_master_bid == AXI_ID_DATA);

    wire r_fault = (io_master_rresp == `AXRESP_SLVERR) || (io_master_rresp == `AXRESP_DECERR);
    wire b_fault = (io_master_bresp == `AXRESP_SLVERR) || (io_master_bresp == `AXRESP_DECERR);

    assign resp_fetch_valid = r_fetch_beat;
    assign resp_fetch_data  = io_master_rdata;
    assign resp_fetch_last  = io_master_rlast;
    assign resp_fetch_fault = r_fetch_beat && r_fault;

    assign resp_load_data = data_load_active ? io_master_rdata : 32'h0;
    assign resp_load_fault = data_load_active && (!r_data_beat || r_fault);
    assign resp_load_misaligned = 1'b0;

    assign resp_store_fault = data_store_active &&
                              (!(io_master_awready && io_master_wready && io_master_bvalid && io_master_bid == AXI_ID_DATA) ||
                               b_fault);
    assign resp_store_misaligned = 1'b0;

    always @(posedge clock or posedge reset) begin
        if (reset) begin
            state <= STATE_IDLE;
            addr_reg <= 32'h0;
            size_reg <= `MEM_SIZE_WORD;
            store_data_reg <= 32'h0;
            data_read_fault_reg <= 1'b0;
            data_write_fault_reg <= 1'b0;
        end else begin
            case (state)
                STATE_IDLE: begin
                    data_read_fault_reg <= 1'b0;
                    data_write_fault_reg <= 1'b0;
                    if (start_fetch) begin
                        addr_reg <= req_fetch_addr;
                        size_reg <= `MEM_SIZE_WORD;
                        state <= STATE_IF_AR;
                    end
                end

                STATE_IF_AR: begin
                    if (io_master_arready) begin
                        state <= STATE_IF_R;
                    end
                end

                STATE_IF_R: begin
                    if (r_fetch_beat && io_master_rlast) begin
                        state <= STATE_IDLE;
                    end
                end

                STATE_DATA_AR: begin
                    state <= STATE_IDLE;
                end

                STATE_DATA_R: begin
                    state <= STATE_IDLE;
                end

                STATE_DATA_W: begin
                    if (io_master_awready && io_master_wready) begin
                        state <= STATE_DATA_B;
                    end
                end

                STATE_DATA_B: begin
                    if (b_data_beat) begin
                        data_write_fault_reg <= b_fault;
                        state <= STATE_IDLE;
                    end
                end

                default: begin
                    state <= STATE_IDLE;
                end
            endcase
        end
    end

endmodule
