// CSR file implementing the six CSRs required by RV32E_Zicsr M-mode.
// Adds a dedicated exception port so mepc and mcause can be updated atomically,
// and a separate debug read port.

`include "npc_defines.vh"

module npc_csr_file (
    input              clock,
    input              reset,

    // Architectural read port (used by CSR instructions)
    input      [11:0]  read_addr,
    output reg [31:0]  read_data,
    output reg         read_fault,

    // Debug read port
    input      [11:0]  debug_addr,
    output reg [31:0]  debug_data,

    // Normal write port (synchronous)
    input              write_en,
    input      [11:0]  write_addr,
    input      [31:0]  write_data,

    // Exception update port (synchronous, priority over normal write)
    input              exc_en,
    input      [31:0]  exc_mepc,
    input      [31:0]  exc_mcause,

    // Architectural values needed by the datapath
    output     [31:0]  mtvec,
    output     [31:0]  mepc
);

    reg [31:0] mstatus_reg;
    reg [31:0] mtvec_reg;
    reg [31:0] mepc_reg;
    reg [31:0] mcause_reg;

    assign mtvec = mtvec_reg;
    assign mepc  = mepc_reg;

    // Architectural read port
    always @(*) begin
        read_fault = 1'b0;
        case (read_addr)
            `CSR_MVENDORID: read_data = 32'h0;
            `CSR_MARCHID:   read_data = 32'h0;
            `CSR_MSTATUS:   read_data = mstatus_reg;
            `CSR_MTVEC:     read_data = mtvec_reg;
            `CSR_MEPC:      read_data = mepc_reg;
            `CSR_MCAUSE:    read_data = mcause_reg;
            default: begin
                read_data = 32'h0;
                read_fault = 1'b1;
            end
        endcase
    end

    // Debug read port
    always @(*) begin
        case (debug_addr)
            `CSR_MVENDORID: debug_data = 32'h0;
            `CSR_MARCHID:   debug_data = 32'h0;
            `CSR_MSTATUS:   debug_data = mstatus_reg;
            `CSR_MTVEC:     debug_data = mtvec_reg;
            `CSR_MEPC:      debug_data = mepc_reg;
            `CSR_MCAUSE:    debug_data = mcause_reg;
            default:        debug_data = 32'h0;
        endcase
    end

    // Synchronous write. Exception port takes priority.
    always @(posedge clock or posedge reset) begin
        if (reset) begin
            mstatus_reg <= `MSTATUS_RESET;
            mtvec_reg   <= 32'h0;
            mepc_reg    <= 32'h0;
            mcause_reg  <= 32'h0;
        end else if (exc_en) begin
            mepc_reg   <= exc_mepc;
            mcause_reg <= exc_mcause;
        end else if (write_en) begin
            case (write_addr)
                `CSR_MSTATUS: mstatus_reg <= `MSTATUS_RESET;
                `CSR_MTVEC:   mtvec_reg   <= write_data & 32'hFFFF_FFFC;
                `CSR_MEPC:    mepc_reg    <= write_data & 32'hFFFF_FFFC;
                `CSR_MCAUSE:  mcause_reg  <= write_data;
                // mvendorid and marchid are read-only; writes are ignored.
                default: ;
            endcase
        end
    end

endmodule
