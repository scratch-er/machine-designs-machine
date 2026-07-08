// Tiny direct-mapped instruction cache.
//
// Two flip-flop lines, 16 bytes per line. Hits respond combinationally so the
// existing two-phase fetch/execute core only stalls on misses.

`include "npc_defines.vh"

module npc_icache (
    input             clock,
    input             reset,
    input             flush,

    // CPU lookup side
    input             lookup_valid,
    input      [31:0] lookup_addr,
    output            resp_valid,
    output     [31:0] resp_inst,
    output            resp_fault,
    output            resp_misaligned,

    // Fill request/response side
    output            fill_req_valid,
    output     [31:0] fill_req_addr,
    input             fill_req_ready,
    input             fill_resp_valid,
    input      [31:0] fill_resp_data,
    input             fill_resp_last,
    input             fill_resp_fault,

    output reg [31:0] counter_access,
    output reg [31:0] counter_hit,
    output reg [31:0] counter_miss,
    output reg [31:0] counter_miss_cycles
);

    localparam STATE_IDLE = 2'd0;
    localparam STATE_REQ  = 2'd1;
    localparam STATE_DATA = 2'd2;

    reg [1:0] state;

    reg        valid0;
    reg        valid1;
    reg [26:0] tag0;
    reg [26:0] tag1;
    reg [31:0] data0_word0;
    reg [31:0] data0_word1;
    reg [31:0] data0_word2;
    reg [31:0] data0_word3;
    reg [31:0] data1_word0;
    reg [31:0] data1_word1;
    reg [31:0] data1_word2;
    reg [31:0] data1_word3;

    wire [26:0] lookup_tag = lookup_addr[31:5];
    wire        lookup_index = lookup_addr[4];
    wire [1:0]  lookup_word = lookup_addr[3:2];
    wire        lookup_aligned = (lookup_addr[1:0] == 2'b00);

    wire        line_valid = lookup_index ? valid1 : valid0;
    wire [26:0] line_tag = lookup_index ? tag1 : tag0;
    wire        hit = lookup_valid && lookup_aligned && (state == STATE_IDLE) &&
                      line_valid && (line_tag == lookup_tag);
    wire        miss = lookup_valid && lookup_aligned && (state == STATE_IDLE) && !hit;

    reg [31:0] hit_inst;
    always @(*) begin
        if (lookup_index) begin
            case (lookup_word)
                2'd0: hit_inst = data1_word0;
                2'd1: hit_inst = data1_word1;
                2'd2: hit_inst = data1_word2;
                default: hit_inst = data1_word3;
            endcase
        end else begin
            case (lookup_word)
                2'd0: hit_inst = data0_word0;
                2'd1: hit_inst = data0_word1;
                2'd2: hit_inst = data0_word2;
                default: hit_inst = data0_word3;
            endcase
        end
    end

    reg [31:0] miss_addr;
    reg [26:0] miss_tag;
    reg        miss_index;
    reg [1:0]  miss_word;
    reg [1:0]  fill_beat;
    reg        fill_fault_seen;
    reg [31:0] fill_word0;
    reg [31:0] fill_word1;
    reg [31:0] fill_word2;
    reg [31:0] fill_word3;

    wire [31:0] selected_fill_word =
        (miss_word == fill_beat && fill_resp_valid) ? fill_resp_data :
        (miss_word == 2'd0) ? fill_word0 :
        (miss_word == 2'd1) ? fill_word1 :
        (miss_word == 2'd2) ? fill_word2 : fill_word3;

    wire fill_done = (state == STATE_DATA) && fill_resp_valid && fill_resp_last;
    wire fill_done_fault = fill_fault_seen || fill_resp_fault;

    reg fault_resp_valid;

    assign fill_req_valid = (state == STATE_REQ);
    assign fill_req_addr  = {miss_addr[31:4], 4'b0};

    assign resp_valid = (lookup_valid && !lookup_aligned && state == STATE_IDLE) ||
                        hit || fault_resp_valid;
    assign resp_inst = hit ? hit_inst : selected_fill_word;
    assign resp_fault = fault_resp_valid;
    assign resp_misaligned = lookup_valid && !lookup_aligned && (state == STATE_IDLE);

    always @(posedge clock or posedge reset) begin
        if (reset) begin
            state <= STATE_IDLE;
            valid0 <= 1'b0;
            valid1 <= 1'b0;
            tag0 <= 27'h0;
            tag1 <= 27'h0;
            data0_word0 <= 32'h0;
            data0_word1 <= 32'h0;
            data0_word2 <= 32'h0;
            data0_word3 <= 32'h0;
            data1_word0 <= 32'h0;
            data1_word1 <= 32'h0;
            data1_word2 <= 32'h0;
            data1_word3 <= 32'h0;
            miss_addr <= 32'h0;
            miss_tag <= 27'h0;
            miss_index <= 1'b0;
            miss_word <= 2'h0;
            fill_beat <= 2'h0;
            fill_fault_seen <= 1'b0;
            fill_word0 <= 32'h0;
            fill_word1 <= 32'h0;
            fill_word2 <= 32'h0;
            fill_word3 <= 32'h0;
            fault_resp_valid <= 1'b0;
            counter_access <= 32'h0;
            counter_hit <= 32'h0;
            counter_miss <= 32'h0;
            counter_miss_cycles <= 32'h0;
        end else begin
            fault_resp_valid <= 1'b0;
            if (flush) begin
                valid0 <= 1'b0;
                valid1 <= 1'b0;
            end

            case (state)
                STATE_IDLE: begin
                    if (lookup_valid && !fault_resp_valid) begin
                        counter_access <= counter_access + 32'd1;
                        if (hit) begin
                            counter_hit <= counter_hit + 32'd1;
                        end else if (lookup_aligned) begin
                            counter_miss <= counter_miss + 32'd1;
                            miss_addr <= lookup_addr;
                            miss_tag <= lookup_tag;
                            miss_index <= lookup_index;
                            miss_word <= lookup_word;
                            fill_beat <= 2'h0;
                            fill_fault_seen <= 1'b0;
                            fill_word0 <= 32'h0;
                            fill_word1 <= 32'h0;
                            fill_word2 <= 32'h0;
                            fill_word3 <= 32'h0;
                            state <= STATE_REQ;
                        end
                    end
                end

                STATE_REQ: begin
                    counter_miss_cycles <= counter_miss_cycles + 32'd1;
                    if (fill_req_ready) begin
                        state <= STATE_DATA;
                    end
                end

                STATE_DATA: begin
                    counter_miss_cycles <= counter_miss_cycles + 32'd1;
                    if (fill_resp_valid) begin
                        case (fill_beat)
                            2'd0: fill_word0 <= fill_resp_data;
                            2'd1: fill_word1 <= fill_resp_data;
                            2'd2: fill_word2 <= fill_resp_data;
                            default: fill_word3 <= fill_resp_data;
                        endcase
                        fill_fault_seen <= fill_fault_seen || fill_resp_fault;

                        if (fill_resp_last) begin
                            if (fill_done_fault) begin
                                fault_resp_valid <= 1'b1;
                            end else begin
                                if (miss_index) begin
                                    valid1 <= 1'b1;
                                    tag1 <= miss_tag;
                                    data1_word0 <= (fill_beat == 2'd0) ? fill_resp_data : fill_word0;
                                    data1_word1 <= (fill_beat == 2'd1) ? fill_resp_data : fill_word1;
                                    data1_word2 <= (fill_beat == 2'd2) ? fill_resp_data : fill_word2;
                                    data1_word3 <= (fill_beat == 2'd3) ? fill_resp_data : fill_word3;
                                end else begin
                                    valid0 <= 1'b1;
                                    tag0 <= miss_tag;
                                    data0_word0 <= (fill_beat == 2'd0) ? fill_resp_data : fill_word0;
                                    data0_word1 <= (fill_beat == 2'd1) ? fill_resp_data : fill_word1;
                                    data0_word2 <= (fill_beat == 2'd2) ? fill_resp_data : fill_word2;
                                    data0_word3 <= (fill_beat == 2'd3) ? fill_resp_data : fill_word3;
                                end
                            end
                            state <= STATE_IDLE;
                        end else begin
                            fill_beat <= fill_beat + 2'd1;
                        end
                    end
                end

                default: begin
                    state <= STATE_IDLE;
                end
            endcase
        end
    end

endmodule
