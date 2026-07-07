// Load/store address, alignment, and data formatting helper.

`include "npc_defines.vh"

module npc_load_store_unit (
    input      [31:0] alu_result,
    input      [31:0] rs2_data,
    input      [31:0] resp_load_data,
    input      [31:0] clint_rdata,
    input             ctrl_mem_read,
    input             ctrl_mem_write,
    input      [1:0]  ctrl_mem_size,
    input             ctrl_mem_sext,

    output     [31:0] mem_addr,
    output     [31:0] store_data,
    output reg [31:0] load_final_data,
    output reg        load_misaligned,
    output reg        store_misaligned,
    output            clint_is_access,
    output            req_load_valid,
    output     [31:0] req_load_addr,
    output     [1:0]  req_load_size,
    output            req_store_valid,
    output     [31:0] req_store_addr,
    output     [1:0]  req_store_size,
    output     [31:0] req_store_data,
    output            clint_req_valid,
    output            clint_req_wen
);

    reg [31:0] load_aligned_data;

    assign mem_addr = alu_result;
    assign store_data = rs2_data;

    assign clint_is_access = (mem_addr >= `CLINT_BASE) &&
                             (mem_addr < `CLINT_BASE + `CLINT_SIZE);

    assign req_load_valid = ctrl_mem_read && !clint_is_access;
    assign req_load_addr = mem_addr;
    assign req_load_size = ctrl_mem_size;
    assign req_store_valid = ctrl_mem_write && !clint_is_access;
    assign req_store_addr = mem_addr;
    assign req_store_size = ctrl_mem_size;
    assign req_store_data = store_data;

    assign clint_req_valid = (ctrl_mem_read || ctrl_mem_write) && clint_is_access;
    assign clint_req_wen = ctrl_mem_write;

    always @(*) begin
        case (ctrl_mem_size)
            `MEM_SIZE_BYTE: begin
                load_misaligned = 1'b0;
                store_misaligned = 1'b0;
            end
            `MEM_SIZE_HALF: begin
                load_misaligned = ctrl_mem_read && (mem_addr[0] != 1'b0);
                store_misaligned = ctrl_mem_write && (mem_addr[0] != 1'b0);
            end
            `MEM_SIZE_WORD: begin
                load_misaligned = ctrl_mem_read && (mem_addr[1:0] != 2'b00);
                store_misaligned = ctrl_mem_write && (mem_addr[1:0] != 2'b00);
            end
            default: begin
                load_misaligned = 1'b0;
                store_misaligned = 1'b0;
            end
        endcase

        case (ctrl_mem_size)
            `MEM_SIZE_BYTE: begin
                load_aligned_data = {{24{resp_load_data[7]}}, resp_load_data[7:0]};
                if (!ctrl_mem_sext) load_aligned_data = load_aligned_data & 32'h000000FF;
            end
            `MEM_SIZE_HALF: begin
                load_aligned_data = {{16{resp_load_data[15]}}, resp_load_data[15:0]};
                if (!ctrl_mem_sext) load_aligned_data = load_aligned_data & 32'h0000FFFF;
            end
            `MEM_SIZE_WORD: begin
                load_aligned_data = resp_load_data;
            end
            default: load_aligned_data = resp_load_data;
        endcase

        load_final_data = clint_is_access ? clint_rdata : load_aligned_data;
    end

endmodule
