// Single-cycle RV32E_Zicsr datapath.
//
// This module is the first implementation milestone. It uses a two-phase
// fetch/execute schedule so the single AXI4 read channel can serve both
// instruction fetches and data loads without combinational arbitration loops.

`include "npc_defines.vh"

module npc_single_cycle (
    input              clock,
    input              reset,

    // Instruction fetch interface (combinational read)
    output             req_fetch_valid,
    output     [31:0]  req_fetch_addr,
    input      [31:0]  resp_fetch_inst,
    input              resp_fetch_fault,
    input              resp_fetch_misaligned,

    // Data load interface (combinational read)
    output             req_load_valid,
    output     [31:0]  req_load_addr,
    output     [1:0]   req_load_size,
    input      [31:0]  resp_load_data,
    input              resp_load_fault,
    input              resp_load_misaligned,

    // Data store interface (registered write)
    output             req_store_valid,
    output     [31:0]  req_store_addr,
    output     [1:0]   req_store_size,
    output     [31:0]  req_store_data,
    input              resp_store_fault,
    input              resp_store_misaligned,

    // Commit interface
    output reg         commit_valid,
    output reg [31:0]  commit_pc,
    output reg [31:0]  commit_inst,
    output reg [4:0]   commit_rd,
    output reg [31:0]  commit_rd_value,
    output reg         commit_exception,
    output reg [31:0]  commit_cause,
    output reg [31:0]  commit_next_pc,

    // Debug ports
    output     [31:0]  debug_pc,
    output     [31:0]  debug_inst,
    input      [3:0]   debug_reg_addr,
    output     [31:0]  debug_reg_data,
    input      [11:0]  debug_csr_addr,
    output     [31:0]  debug_csr_data
);

    // Architectural state
    reg [31:0] pc;
    reg        halted;
    reg        execute_phase;
    reg [31:0] inst_reg;
    reg [31:0] fetch_pc_reg;
    reg        fetch_fault_reg;
    reg        fetch_misaligned_reg;

    // Commit constants: mcause values
    localparam [31:0] CAUSE_INST_MISALIGNED   = 32'd0;
    localparam [31:0] CAUSE_INST_ACCESS_FAULT = 32'd1;
    localparam [31:0] CAUSE_ILLEGAL_INST      = 32'd2;
    localparam [31:0] CAUSE_BREAKPOINT        = 32'd3;
    localparam [31:0] CAUSE_LOAD_MISALIGNED   = 32'd4;
    localparam [31:0] CAUSE_LOAD_ACCESS_FAULT = 32'd5;
    localparam [31:0] CAUSE_STORE_MISALIGNED  = 32'd6;
    localparam [31:0] CAUSE_STORE_ACCESS_FAULT = 32'd7;
    localparam [31:0] CAUSE_ECALL_M           = 32'd11;

    //==========================================================================
    // Decode
    //==========================================================================
    wire [31:0] inst = inst_reg;
    wire [4:0]  rd5;
    wire [4:0]  rs15;
    wire [4:0]  rs25;
    wire [3:0]  inst_rd;
    wire [3:0]  inst_rs1;
    wire [3:0]  inst_rs2;
    wire [11:0] inst_csr;

    wire        ctrl_illegal;
    wire        ctrl_ecall;
    wire        ctrl_ebreak;
    wire        ctrl_mret;
    wire        ctrl_wfi;
    wire        ctrl_fence_i;
    wire [3:0]  ctrl_alu_op;
    wire [1:0]  ctrl_alu_src_a;
    wire        ctrl_alu_src_b;
    wire [2:0]  ctrl_imm_sel;
    wire        ctrl_reg_write;
    wire        ctrl_mem_read;
    wire        ctrl_mem_write;
    wire [1:0]  ctrl_mem_size;
    wire        ctrl_mem_sext;
    wire [2:0]  ctrl_branch_type;
    wire        ctrl_jump;
    wire        ctrl_jump_reg;
    wire [2:0]  ctrl_csr_op;
    wire        ctrl_csr_src;
    wire        dec_has_rd;
    wire        dec_has_rs1;
    wire        dec_has_rs2;

    npc_decoder decoder (
        .inst             (inst),
        .rd5              (rd5),
        .rs15             (rs15),
        .rs25             (rs25),
        .rd               (inst_rd),
        .rs1              (inst_rs1),
        .rs2              (inst_rs2),
        .csr_addr         (inst_csr),
        .ctrl_illegal     (ctrl_illegal),
        .ctrl_ecall       (ctrl_ecall),
        .ctrl_ebreak      (ctrl_ebreak),
        .ctrl_mret        (ctrl_mret),
        .ctrl_wfi         (ctrl_wfi),
        .ctrl_fence_i     (ctrl_fence_i),
        .ctrl_alu_op      (ctrl_alu_op),
        .ctrl_alu_src_a   (ctrl_alu_src_a),
        .ctrl_alu_src_b   (ctrl_alu_src_b),
        .ctrl_imm_sel     (ctrl_imm_sel),
        .ctrl_reg_write   (ctrl_reg_write),
        .ctrl_mem_read    (ctrl_mem_read),
        .ctrl_mem_write   (ctrl_mem_write),
        .ctrl_mem_size    (ctrl_mem_size),
        .ctrl_mem_sext    (ctrl_mem_sext),
        .ctrl_branch_type (ctrl_branch_type),
        .ctrl_jump        (ctrl_jump),
        .ctrl_jump_reg    (ctrl_jump_reg),
        .ctrl_csr_op      (ctrl_csr_op),
        .ctrl_csr_src     (ctrl_csr_src),
        .dec_has_rd       (dec_has_rd),
        .dec_has_rs1      (dec_has_rs1),
        .dec_has_rs2      (dec_has_rs2)
    );

    //==========================================================================
    // Register file
    //==========================================================================
    wire [31:0] rs1_data;
    wire [31:0] rs2_data;
    reg         reg_wen;
    reg  [3:0]  reg_waddr;
    reg  [31:0] reg_wdata;

    npc_regfile regfile (
        .clock      (clock),
        .reset      (reset),
        .rs1_addr   (inst_rs1),
        .rs1_data   (rs1_data),
        .rs2_addr   (inst_rs2),
        .rs2_data   (rs2_data),
        .wen        (reg_wen),
        .rd_addr    (reg_waddr),
        .rd_data    (reg_wdata),
        .debug_addr (debug_reg_addr),
        .debug_data (debug_reg_data)
    );

    //==========================================================================
    // CSR file
    //==========================================================================
    wire [31:0] csr_read_data;
    wire        csr_read_fault;
    reg         csr_wen;
    reg  [31:0] csr_wdata;
    wire [31:0] mtvec_reg;
    wire [31:0] mepc_reg;

    npc_csr_file csr_file (
        .clock      (clock),
        .reset      (reset),
        .read_addr  (inst_csr),
        .read_data  (csr_read_data),
        .read_fault (csr_read_fault),
        .debug_addr (debug_csr_addr),
        .debug_data (debug_csr_data),
        .write_en   (csr_wen),
        .write_addr (inst_csr),
        .write_data (csr_wdata),
        .exc_en     (execute_phase && exc_take),
        .exc_mepc   (fetch_pc_reg),
        .exc_mcause (exc_cause),
        .mtvec      (mtvec_reg),
        .mepc       (mepc_reg)
    );

    //==========================================================================
    // Immediate generator
    //==========================================================================
    wire [31:0] imm;

    npc_imm imm_gen (
        .inst    (inst),
        .imm_sel (ctrl_imm_sel),
        .imm     (imm)
    );

    //==========================================================================
    // ALU
    //==========================================================================
    wire [31:0] alu_result;

    npc_alu alu (
        .rs1_data  (rs1_data),
        .rs2_data  (rs2_data),
        .pc        (pc),
        .imm       (imm),
        .alu_op    (ctrl_alu_op),
        .alu_src_a (ctrl_alu_src_a),
        .alu_src_b (ctrl_alu_src_b),
        .alu_result(alu_result)
    );

    //==========================================================================
    // Branch / jump target and condition
    //==========================================================================
    wire        branch_taken;
    wire [31:0] branch_target;
    wire [31:0] jump_target;
    wire        branch_target_misaligned;
    wire        jump_target_misaligned;

    npc_branch branch_unit (
        .pc                       (pc),
        .imm                      (imm),
        .rs1_data                 (rs1_data),
        .rs2_data                 (rs2_data),
        .branch_type              (ctrl_branch_type),
        .jump_reg                 (ctrl_jump_reg),
        .branch_taken             (branch_taken),
        .branch_target            (branch_target),
        .jump_target              (jump_target),
        .branch_target_misaligned (branch_target_misaligned),
        .jump_target_misaligned   (jump_target_misaligned)
    );

    //==========================================================================
    // Memory access
    //==========================================================================
    wire [31:0] mem_addr;
    wire [31:0] store_data;
    wire [31:0] load_final_data;
    wire        load_misaligned;
    wire        store_misaligned;
    wire        clint_req_valid;
    wire        clint_req_wen;
    wire        clint_is_access;
    wire [31:0] clint_rdata;
    wire        clint_fault = 1'b0;

    npc_clint clint (
        .clock      (clock),
        .reset      (reset),
        .tick_en    (execute_phase),
        .req_valid  (clint_req_valid),
        .req_wen    (clint_req_wen),
        .req_addr   (mem_addr),
        .req_wdata  (store_data),
        .resp_rdata (clint_rdata),
        .resp_fault (clint_fault)
    );

    npc_load_store_unit load_store_unit (
        .alu_result        (alu_result),
        .rs2_data          (rs2_data),
        .resp_load_data    (resp_load_data),
        .clint_rdata       (clint_rdata),
        .ctrl_mem_read     (execute_phase && ctrl_mem_read),
        .ctrl_mem_write    (execute_phase && ctrl_mem_write),
        .ctrl_mem_size     (ctrl_mem_size),
        .ctrl_mem_sext     (ctrl_mem_sext),
        .mem_addr          (mem_addr),
        .store_data        (store_data),
        .load_final_data   (load_final_data),
        .load_misaligned   (load_misaligned),
        .store_misaligned  (store_misaligned),
        .clint_is_access   (clint_is_access),
        .req_load_valid    (req_load_valid),
        .req_load_addr     (req_load_addr),
        .req_load_size     (req_load_size),
        .req_store_valid   (req_store_valid),
        .req_store_addr    (req_store_addr),
        .req_store_size    (req_store_size),
        .req_store_data    (req_store_data),
        .clint_req_valid   (clint_req_valid),
        .clint_req_wen     (clint_req_wen)
    );

    assign req_fetch_valid = !execute_phase && !halted;
    assign req_fetch_addr = pc;

    //==========================================================================
    // CSR operation
    //==========================================================================
    reg [31:0] csr_operand;
    reg [31:0] csr_new_value;
    reg        csr_do_write;
    reg        csr_illegal;

    always @(*) begin
        csr_operand = ctrl_csr_src ? imm : rs1_data;
        csr_do_write = 1'b1;
        case (ctrl_csr_op)
            `CSR_OP_RW, `CSR_OP_RWI: csr_new_value = csr_operand;
            `CSR_OP_RS, `CSR_OP_RSI: begin
                if ((ctrl_csr_src && imm[4:0] == 5'b0) ||
                    (!ctrl_csr_src && inst_rs1 == 4'd0)) begin
                    csr_do_write = 1'b0;
                end
                csr_new_value = csr_read_data | csr_operand;
            end
            `CSR_OP_RC, `CSR_OP_RCI: begin
                if ((ctrl_csr_src && imm[4:0] == 5'b0) ||
                    (!ctrl_csr_src && inst_rs1 == 4'd0)) begin
                    csr_do_write = 1'b0;
                end
                csr_new_value = csr_read_data & ~csr_operand;
            end
            default: begin csr_new_value = csr_read_data; csr_do_write = 1'b0; end
        endcase
        csr_illegal = (ctrl_csr_op != `CSR_OP_NONE) && csr_read_fault;
    end

    //==========================================================================
    // Exception detection and next PC
    //==========================================================================
    reg        exc_take;
    reg [31:0] exc_cause;
    reg [31:0] next_pc;

    wire reserved_rd  = dec_has_rd  && rd5[4];
    wire reserved_rs1 = dec_has_rs1 && rs15[4];
    wire reserved_rs2 = dec_has_rs2 && rs25[4];

    always @(*) begin
        exc_take = 1'b0;
        exc_cause = 32'h0;

        // 1. Instruction alignment / fetch fault
        if (fetch_pc_reg[1:0] != 2'b00) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_INST_MISALIGNED;
        end else if (fetch_fault_reg) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_INST_ACCESS_FAULT;
        end else if (fetch_misaligned_reg) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_INST_MISALIGNED;
        // 2. Decode-time exceptions
        end else if (ctrl_illegal || reserved_rd || reserved_rs1 || reserved_rs2 || csr_illegal) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_ILLEGAL_INST;
        // 3. Execute-time exceptions
        end else if (ctrl_jump && jump_target_misaligned) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_INST_MISALIGNED;
        end else if (branch_target_misaligned) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_INST_MISALIGNED;
        end else if (ctrl_mem_read && load_misaligned) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_LOAD_MISALIGNED;
        end else if (ctrl_mem_read && resp_load_fault) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_LOAD_ACCESS_FAULT;
        end else if (ctrl_mem_read && resp_load_misaligned) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_LOAD_MISALIGNED;
        end else if (ctrl_mem_write && store_misaligned) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_STORE_MISALIGNED;
        // 4. Environment/privileged exceptions
        end else if (ctrl_ecall) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_ECALL_M;
        end else if (ctrl_ebreak) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_BREAKPOINT;
        end

        // Next PC selection
        if (exc_take) begin
            next_pc = mtvec_reg;
        end else if (ctrl_mret) begin
            next_pc = mepc_reg;
        end else if (ctrl_jump) begin
            next_pc = jump_target;
        end else if (branch_taken) begin
            next_pc = branch_target;
        end else begin
            next_pc = pc + 32'd4;
        end
    end

    //==========================================================================
    // Writeback
    //==========================================================================
    reg [31:0] rd_value;
    always @(*) begin
        if (ctrl_mem_read) begin
            rd_value = load_final_data;
        end else if (ctrl_csr_op != `CSR_OP_NONE) begin
            rd_value = csr_read_data;
        end else if (ctrl_jump) begin
            rd_value = pc + 32'd4;
        end else begin
            rd_value = alu_result;
        end
    end

    //==========================================================================
    // Register and CSR write controls
    //==========================================================================
    always @(*) begin
        reg_wen   = execute_phase && ctrl_reg_write && !exc_take && (inst_rd != 4'd0);
        reg_waddr = inst_rd;
        reg_wdata = rd_value;

        csr_wen   = execute_phase && (ctrl_csr_op != `CSR_OP_NONE) && !exc_take && csr_do_write;
        csr_wdata = csr_new_value;
    end

    //==========================================================================
    // Sequential state update and commit capture
    //==========================================================================
    reg        commit_valid_r;
    reg [31:0] commit_pc_r;
    reg [31:0] commit_inst_r;
    reg [4:0]  commit_rd_r;
    reg [31:0] commit_rd_value_r;
    reg        commit_exception_r;
    reg [31:0] commit_cause_r;
    reg [31:0] commit_next_pc_r;

    always @(posedge clock or posedge reset) begin
        if (reset) begin
            pc                   <= `RESET_VECTOR;
            halted               <= 1'b0;
            execute_phase        <= 1'b0;
            inst_reg             <= 32'h00000013;
            fetch_pc_reg         <= `RESET_VECTOR;
            fetch_fault_reg      <= 1'b0;
            fetch_misaligned_reg <= 1'b0;
            commit_valid_r       <= 1'b0;
        end else if (!halted && !execute_phase) begin
            inst_reg             <= resp_fetch_inst;
            fetch_pc_reg         <= pc;
            fetch_fault_reg      <= resp_fetch_fault;
            fetch_misaligned_reg <= resp_fetch_misaligned;
            execute_phase        <= 1'b1;
            commit_valid_r       <= 1'b0;
        end else if (!halted) begin
            // Capture the instruction that retires this cycle. The PC register
            // will be updated to next_pc, so we must record the fetched PC here
            // for the commit interface.
            commit_valid_r     <= 1'b1;
            commit_pc_r        <= fetch_pc_reg;
            commit_inst_r      <= inst;
            commit_rd_r        <= reg_wen ? {1'b0, inst_rd} : 5'b0;
            commit_rd_value_r  <= reg_wen ? rd_value : 32'h0;
            commit_exception_r <= exc_take;
            commit_cause_r     <= exc_cause;
            commit_next_pc_r   <= next_pc;

            execute_phase <= 1'b0;
            if (exc_take) begin
                pc <= mtvec_reg;
                halted <= ctrl_ebreak;
            end else begin
                pc <= next_pc;
            end
        end else begin
            commit_valid_r <= 1'b0;
        end
    end

    //==========================================================================
    // Commit interface
    //==========================================================================
    assign commit_valid      = commit_valid_r;
    assign commit_pc         = commit_pc_r;
    assign commit_inst       = commit_inst_r;
    assign commit_rd         = commit_rd_r;
    assign commit_rd_value   = commit_rd_value_r;
    assign commit_exception  = commit_exception_r;
    assign commit_cause      = commit_cause_r;
    assign commit_next_pc    = commit_next_pc_r;

    //==========================================================================
    // Debug outputs
    //==========================================================================
    assign debug_pc   = execute_phase ? fetch_pc_reg : pc;
    assign debug_inst = inst;

endmodule
