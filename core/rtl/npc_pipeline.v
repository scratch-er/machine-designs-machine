// Five-stage-ish RV32E_Zicsr in-order pipeline datapath.
//
// The external memory contract matches npc_single_cycle: instruction fetches are
// served by npc_icache and data accesses use the existing zero-latency AXI bridge.
// Pipeline stages are IF/ID, ID/EX, EX/MEM, and MEM/WB with static not-taken
// fetch, simple forwarding, and conservative stalls around load-use and CSR ops.

`include "npc_defines.vh"

module npc_pipeline (
    input              clock,
    input              reset,

    // Instruction fetch interface
    output             req_fetch_valid,
    output     [31:0]  req_fetch_addr,
    input              resp_fetch_valid,
    input      [31:0]  resp_fetch_inst,
    input              resp_fetch_fault,
    input              resp_fetch_misaligned,

    // Data load interface (combinational read)
    output             req_load_valid,
    output     [31:0]  req_load_addr,
    output     [1:0]   req_load_size,
    input              req_load_ready,
    input      [31:0]  resp_load_data,
    input              resp_load_fault,
    input              resp_load_misaligned,

    // Data store interface (registered write)
    output             req_store_valid,
    output     [31:0]  req_store_addr,
    output     [1:0]   req_store_size,
    output     [31:0]  req_store_data,
    input              req_store_ready,
    input              resp_store_fault,
    input              resp_store_misaligned,

    // Commit interface
    output             commit_valid,
    output     [31:0]  commit_pc,
    output     [31:0]  commit_inst,
    output     [4:0]   commit_rd,
    output     [31:0]  commit_rd_value,
    output             commit_exception,
    output     [31:0]  commit_cause,
    output     [31:0]  commit_next_pc,
    output             fence_i_commit,

    // Debug ports
    output     [31:0]  debug_pc,
    output     [31:0]  debug_inst,
    input      [3:0]   debug_reg_addr,
    output     [31:0]  debug_reg_data,
    input      [11:0]  debug_csr_addr,
    output     [31:0]  debug_csr_data
);

    //=========================================================================
    // IF stage
    //=========================================================================
    reg [31:0] pc;
    reg        halted;
    reg        fetch_pending;
    reg [31:0] fetch_pc;

    //=========================================================================
    // IF/ID register and decode
    //=========================================================================
    reg        if_id_valid;
    reg [31:0] if_id_pc;
    reg [31:0] if_id_inst;
    reg        if_id_fetch_fault;
    reg        if_id_fetch_misaligned;

    wire [4:0]  dec_rd5;
    wire [4:0]  dec_rs15;
    wire [4:0]  dec_rs25;
    wire [3:0]  dec_rd;
    wire [3:0]  dec_rs1;
    wire [3:0]  dec_rs2;
    wire [11:0] dec_csr;
    wire        dec_illegal;
    wire        dec_ecall;
    wire        dec_ebreak;
    wire        dec_mret;
    wire        dec_wfi;
    wire        dec_fence_i;
    wire [3:0]  dec_alu_op;
    wire [1:0]  dec_alu_src_a;
    wire        dec_alu_src_b;
    wire [2:0]  dec_imm_sel;
    wire        dec_reg_write;
    wire        dec_mem_read;
    wire        dec_mem_write;
    wire [1:0]  dec_mem_size;
    wire        dec_mem_sext;
    wire [2:0]  dec_branch_type;
    wire        dec_jump;
    wire        dec_jump_reg;
    wire [2:0]  dec_csr_op;
    wire        dec_csr_src;
    wire        dec_has_rd;
    wire        dec_has_rs1;
    wire        dec_has_rs2;

    npc_decoder decoder (
        .inst             (if_id_inst),
        .rd5              (dec_rd5),
        .rs15             (dec_rs15),
        .rs25             (dec_rs25),
        .rd               (dec_rd),
        .rs1              (dec_rs1),
        .rs2              (dec_rs2),
        .csr_addr         (dec_csr),
        .ctrl_illegal     (dec_illegal),
        .ctrl_ecall       (dec_ecall),
        .ctrl_ebreak      (dec_ebreak),
        .ctrl_mret        (dec_mret),
        .ctrl_wfi         (dec_wfi),
        .ctrl_fence_i     (dec_fence_i),
        .ctrl_alu_op      (dec_alu_op),
        .ctrl_alu_src_a   (dec_alu_src_a),
        .ctrl_alu_src_b   (dec_alu_src_b),
        .ctrl_imm_sel     (dec_imm_sel),
        .ctrl_reg_write   (dec_reg_write),
        .ctrl_mem_read    (dec_mem_read),
        .ctrl_mem_write   (dec_mem_write),
        .ctrl_mem_size    (dec_mem_size),
        .ctrl_mem_sext    (dec_mem_sext),
        .ctrl_branch_type (dec_branch_type),
        .ctrl_jump        (dec_jump),
        .ctrl_jump_reg    (dec_jump_reg),
        .ctrl_csr_op      (dec_csr_op),
        .ctrl_csr_src     (dec_csr_src),
        .dec_has_rd       (dec_has_rd),
        .dec_has_rs1      (dec_has_rs1),
        .dec_has_rs2      (dec_has_rs2)
    );

    wire [31:0] dec_imm;
    npc_imm imm_gen (
        .inst    (if_id_inst),
        .imm_sel (dec_imm_sel),
        .imm     (dec_imm)
    );

    //=========================================================================
    // Architectural state
    //=========================================================================
    wire [31:0] rs1_data;
    wire [31:0] rs2_data;
    reg         reg_wen;
    reg  [3:0]  reg_waddr;
    reg  [31:0] reg_wdata;

    npc_regfile regfile (
        .clock      (clock),
        .reset      (reset),
        .rs1_addr   (dec_rs1),
        .rs1_data   (rs1_data),
        .rs2_addr   (dec_rs2),
        .rs2_data   (rs2_data),
        .wen        (reg_wen),
        .rd_addr    (reg_waddr),
        .rd_data    (reg_wdata),
        .debug_addr (debug_reg_addr),
        .debug_data (debug_reg_data)
    );

    wire [31:0] csr_read_data;
    wire        csr_read_fault;
    reg         csr_wen;
    reg  [11:0] csr_waddr;
    reg  [31:0] csr_wdata;
    reg         csr_exc_en;
    reg  [31:0] csr_exc_mepc;
    reg  [31:0] csr_exc_mcause;
    wire [31:0] mtvec_reg;
    wire [31:0] mepc_reg;

    npc_csr_file csr_file (
        .clock      (clock),
        .reset      (reset),
        .read_addr  (dec_csr),
        .read_data  (csr_read_data),
        .read_fault (csr_read_fault),
        .debug_addr (debug_csr_addr),
        .debug_data (debug_csr_data),
        .write_en   (csr_wen),
        .write_addr (csr_waddr),
        .write_data (csr_wdata),
        .exc_en     (csr_exc_en),
        .exc_mepc   (csr_exc_mepc),
        .exc_mcause (csr_exc_mcause),
        .mtvec      (mtvec_reg),
        .mepc       (mepc_reg)
    );

    //=========================================================================
    // ID/EX register
    //=========================================================================
    reg        id_ex_valid;
    reg [31:0] id_ex_pc;
    reg [31:0] id_ex_inst;
    reg [3:0]  id_ex_rd;
    reg [3:0]  id_ex_rs1;
    reg [3:0]  id_ex_rs2;
    reg [4:0]  id_ex_rd5;
    reg [4:0]  id_ex_rs15;
    reg [4:0]  id_ex_rs25;
    reg [11:0] id_ex_csr;
    reg [31:0] id_ex_rs1_data;
    reg [31:0] id_ex_rs2_data;
    reg [31:0] id_ex_imm;
    reg [31:0] id_ex_csr_data;
    reg        id_ex_csr_fault;
    reg        id_ex_fetch_fault;
    reg        id_ex_fetch_misaligned;
    reg        id_ex_illegal;
    reg        id_ex_ecall;
    reg        id_ex_ebreak;
    reg        id_ex_mret;
    reg        id_ex_fence_i;
    reg [3:0]  id_ex_alu_op;
    reg [1:0]  id_ex_alu_src_a;
    reg        id_ex_alu_src_b;
    reg        id_ex_reg_write;
    reg        id_ex_mem_read;
    reg        id_ex_mem_write;
    reg [1:0]  id_ex_mem_size;
    reg        id_ex_mem_sext;
    reg [2:0]  id_ex_branch_type;
    reg        id_ex_jump;
    reg        id_ex_jump_reg;
    reg [2:0]  id_ex_csr_op;
    reg        id_ex_csr_src;
    reg        id_ex_has_rd;
    reg        id_ex_has_rs1;
    reg        id_ex_has_rs2;

    //=========================================================================
    // EX stage
    //=========================================================================
    reg [31:0] ex_rs1_value;
    reg [31:0] ex_rs2_value;
    reg [31:0] ex_forward_value;

    wire [31:0] mem_stage_rd_value;
    wire        ex_mem_can_forward;
    wire        mem_wb_can_forward;

    always @(*) begin
        ex_rs1_value = id_ex_rs1_data;
        ex_rs2_value = id_ex_rs2_data;
        if (id_ex_rs1 != 4'd0 && ex_mem_can_forward && ex_mem_rd == id_ex_rs1) begin
            ex_rs1_value = ex_forward_value;
        end else if (id_ex_rs1 != 4'd0 && mem_wb_can_forward && mem_wb_rd == id_ex_rs1) begin
            ex_rs1_value = mem_wb_rd_value;
        end
        if (id_ex_rs2 != 4'd0 && ex_mem_can_forward && ex_mem_rd == id_ex_rs2) begin
            ex_rs2_value = ex_forward_value;
        end else if (id_ex_rs2 != 4'd0 && mem_wb_can_forward && mem_wb_rd == id_ex_rs2) begin
            ex_rs2_value = mem_wb_rd_value;
        end
    end

    wire [31:0] ex_alu_result;
    npc_alu alu (
        .rs1_data   (ex_rs1_value),
        .rs2_data   (ex_rs2_value),
        .pc         (id_ex_pc),
        .imm        (id_ex_imm),
        .alu_op     (id_ex_alu_op),
        .alu_src_a  (id_ex_alu_src_a),
        .alu_src_b  (id_ex_alu_src_b),
        .alu_result (ex_alu_result)
    );

    wire        ex_branch_taken;
    wire [31:0] ex_branch_target;
    wire [31:0] ex_jump_target;
    wire        ex_branch_target_misaligned;
    wire        ex_jump_target_misaligned;

    npc_branch branch_unit (
        .pc                       (id_ex_pc),
        .imm                      (id_ex_imm),
        .rs1_data                 (ex_rs1_value),
        .rs2_data                 (ex_rs2_value),
        .branch_type              (id_ex_branch_type),
        .jump_reg                 (id_ex_jump_reg),
        .branch_taken             (ex_branch_taken),
        .branch_target            (ex_branch_target),
        .jump_target              (ex_jump_target),
        .branch_target_misaligned (ex_branch_target_misaligned),
        .jump_target_misaligned   (ex_jump_target_misaligned)
    );

    reg [31:0] ex_csr_operand;
    reg [31:0] ex_csr_new_value;
    reg        ex_csr_do_write;
    always @(*) begin
        ex_csr_operand = id_ex_csr_src ? id_ex_imm : ex_rs1_value;
        ex_csr_do_write = 1'b1;
        case (id_ex_csr_op)
            `CSR_OP_RW, `CSR_OP_RWI: ex_csr_new_value = ex_csr_operand;
            `CSR_OP_RS, `CSR_OP_RSI: begin
                if ((id_ex_csr_src && id_ex_imm[4:0] == 5'b0) ||
                    (!id_ex_csr_src && id_ex_rs1 == 4'd0)) begin
                    ex_csr_do_write = 1'b0;
                end
                ex_csr_new_value = id_ex_csr_data | ex_csr_operand;
            end
            `CSR_OP_RC, `CSR_OP_RCI: begin
                if ((id_ex_csr_src && id_ex_imm[4:0] == 5'b0) ||
                    (!id_ex_csr_src && id_ex_rs1 == 4'd0)) begin
                    ex_csr_do_write = 1'b0;
                end
                ex_csr_new_value = id_ex_csr_data & ~ex_csr_operand;
            end
            default: begin
                ex_csr_new_value = id_ex_csr_data;
                ex_csr_do_write = 1'b0;
            end
        endcase
    end

    wire id_ex_reserved_rd  = id_ex_has_rd  && id_ex_rd5[4];
    wire id_ex_reserved_rs1 = id_ex_has_rs1 && id_ex_rs15[4];
    wire id_ex_reserved_rs2 = id_ex_has_rs2 && id_ex_rs25[4];
    wire id_ex_csr_illegal = (id_ex_csr_op != `CSR_OP_NONE) && id_ex_csr_fault;

    reg        ex_exception;
    reg [31:0] ex_cause;
    reg [31:0] ex_next_pc;
    always @(*) begin
        ex_exception = 1'b0;
        ex_cause = 32'h0;

        if (id_ex_pc[1:0] != 2'b00) begin
            ex_exception = 1'b1;
            ex_cause = `CAUSE_INST_MISALIGNED;
        end else if (id_ex_fetch_fault) begin
            ex_exception = 1'b1;
            ex_cause = `CAUSE_INST_ACCESS_FAULT;
        end else if (id_ex_fetch_misaligned) begin
            ex_exception = 1'b1;
            ex_cause = `CAUSE_INST_MISALIGNED;
        end else if (id_ex_illegal || id_ex_reserved_rd || id_ex_reserved_rs1 || id_ex_reserved_rs2 || id_ex_csr_illegal) begin
            ex_exception = 1'b1;
            ex_cause = `CAUSE_ILLEGAL_INST;
        end else if (id_ex_jump && ex_jump_target_misaligned) begin
            ex_exception = 1'b1;
            ex_cause = `CAUSE_INST_MISALIGNED;
        end else if (ex_branch_target_misaligned) begin
            ex_exception = 1'b1;
            ex_cause = `CAUSE_INST_MISALIGNED;
        end else if (id_ex_ecall) begin
            ex_exception = 1'b1;
            ex_cause = `CAUSE_ECALL_M;
        end else if (id_ex_ebreak) begin
            ex_exception = 1'b1;
            ex_cause = `CAUSE_BREAKPOINT;
        end

        if (ex_exception) begin
            ex_next_pc = mtvec_reg;
        end else if (id_ex_mret) begin
            ex_next_pc = mepc_reg;
        end else if (id_ex_jump) begin
            ex_next_pc = ex_jump_target;
        end else if (ex_branch_taken) begin
            ex_next_pc = ex_branch_target;
        end else begin
            ex_next_pc = id_ex_pc + 32'd4;
        end
    end

    wire ex_redirect = id_ex_valid && (ex_exception || id_ex_mret || id_ex_jump || ex_branch_taken);

    //=========================================================================
    // EX/MEM register and MEM stage
    //=========================================================================
    reg        ex_mem_valid;
    reg [31:0] ex_mem_pc;
    reg [31:0] ex_mem_inst;
    reg [3:0]  ex_mem_rd;
    reg [11:0] ex_mem_csr;
    reg [31:0] ex_mem_alu_result;
    reg [31:0] ex_mem_rs2_data;
    reg [31:0] ex_mem_csr_data;
    reg [31:0] ex_mem_csr_new_value;
    reg        ex_mem_csr_do_write;
    reg        ex_mem_reg_write;
    reg        ex_mem_mem_read;
    reg        ex_mem_mem_write;
    reg [1:0]  ex_mem_mem_size;
    reg        ex_mem_mem_sext;
    reg [2:0]  ex_mem_csr_op;
    reg        ex_mem_jump;
    reg        ex_mem_fence_i;
    reg        ex_mem_exception;
    reg [31:0] ex_mem_cause;
    reg [31:0] ex_mem_next_pc;
    reg        ex_mem_ebreak;

    wire [31:0] mem_addr;
    wire [31:0] store_data;
    wire [31:0] load_final_data;
    wire        load_misaligned;
    wire        store_misaligned;
    wire        clint_req_valid;
    wire        clint_req_wen;
    wire        clint_is_access;
    wire [31:0] clint_rdata;
    wire        clint_fault;

    npc_clint clint (
        .clock      (clock),
        .reset      (reset),
        .tick_en    (commit_valid),
        .req_valid  (clint_req_valid),
        .req_wen    (clint_req_wen),
        .req_addr   (mem_addr),
        .req_wdata  (store_data),
        .resp_rdata (clint_rdata),
        .resp_fault (clint_fault)
    );

    npc_load_store_unit load_store_unit (
        .alu_result        (ex_mem_alu_result),
        .rs2_data          (ex_mem_rs2_data),
        .resp_load_data    (resp_load_data),
        .clint_rdata       (clint_rdata),
        .ctrl_mem_read     (ex_mem_valid && ex_mem_mem_read && !ex_mem_exception),
        .ctrl_mem_write    (ex_mem_valid && ex_mem_mem_write && !ex_mem_exception),
        .ctrl_mem_size     (ex_mem_mem_size),
        .ctrl_mem_sext     (ex_mem_mem_sext),
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

    reg        mem_exception;
    reg [31:0] mem_cause;
    always @(*) begin
        mem_exception = ex_mem_exception;
        mem_cause = ex_mem_cause;
        if (!ex_mem_exception && ex_mem_mem_read && load_misaligned) begin
            mem_exception = 1'b1;
            mem_cause = `CAUSE_LOAD_MISALIGNED;
        end else if (!ex_mem_exception && ex_mem_mem_read && resp_load_fault) begin
            mem_exception = 1'b1;
            mem_cause = `CAUSE_LOAD_ACCESS_FAULT;
        end else if (!ex_mem_exception && ex_mem_mem_read && resp_load_misaligned) begin
            mem_exception = 1'b1;
            mem_cause = `CAUSE_LOAD_MISALIGNED;
        end else if (!ex_mem_exception && ex_mem_mem_write && store_misaligned) begin
            mem_exception = 1'b1;
            mem_cause = `CAUSE_STORE_MISALIGNED;
        end else if (!ex_mem_exception && ex_mem_mem_write && resp_store_fault) begin
            mem_exception = 1'b1;
            mem_cause = `CAUSE_STORE_ACCESS_FAULT;
        end else if (!ex_mem_exception && ex_mem_mem_write && resp_store_misaligned) begin
            mem_exception = 1'b1;
            mem_cause = `CAUSE_STORE_MISALIGNED;
        end
    end

    assign mem_stage_rd_value = ex_mem_mem_read ? load_final_data :
                                (ex_mem_csr_op != `CSR_OP_NONE) ? ex_mem_csr_data :
                                ex_mem_jump ? (ex_mem_pc + 32'd4) : ex_mem_alu_result;

    always @(*) begin
        ex_forward_value = (ex_mem_csr_op != `CSR_OP_NONE) ? ex_mem_csr_data :
                           ex_mem_jump ? (ex_mem_pc + 32'd4) : ex_mem_alu_result;
    end

    assign ex_mem_can_forward = ex_mem_valid && ex_mem_reg_write && !ex_mem_mem_read &&
                                !ex_mem_exception && (ex_mem_rd != 4'd0);

    //=========================================================================
    // MEM/WB register and WB stage
    //=========================================================================
    reg        mem_wb_valid;
    reg [31:0] mem_wb_pc;
    reg [31:0] mem_wb_inst;
    reg [3:0]  mem_wb_rd;
    reg [11:0] mem_wb_csr;
    reg [31:0] mem_wb_rd_value;
    reg [31:0] mem_wb_csr_new_value;
    reg        mem_wb_csr_do_write;
    reg        mem_wb_reg_write;
    reg [2:0]  mem_wb_csr_op;
    reg        mem_wb_fence_i;
    reg        mem_wb_exception;
    reg [31:0] mem_wb_cause;
    reg [31:0] mem_wb_next_pc;
    reg        mem_wb_ebreak;

    assign mem_wb_can_forward = mem_wb_valid && mem_wb_reg_write && !mem_wb_exception &&
                                (mem_wb_rd != 4'd0);

    //=========================================================================
    // Hazard and fetch control
    //=========================================================================
    wire mem_access_wait = ex_mem_valid && !ex_mem_exception &&
                           ((ex_mem_mem_read && !load_misaligned && !clint_is_access && !req_load_ready) ||
                            (ex_mem_mem_write && !store_misaligned && !clint_is_access && !req_store_ready));
    wire mem_new_exception = ex_mem_valid && !ex_mem_exception && mem_exception;
    wire mem_redirect = mem_new_exception;
    wire dec_uses_rs1 = if_id_valid && dec_has_rs1;
    wire dec_uses_rs2 = if_id_valid && dec_has_rs2;
    wire load_use_stall = if_id_valid && id_ex_valid && id_ex_mem_read && id_ex_reg_write &&
                          (id_ex_rd != 4'd0) &&
                          ((dec_uses_rs1 && dec_rs1 == id_ex_rd) ||
                           (dec_uses_rs2 && dec_rs2 == id_ex_rd));
    wire wb_will_halt = mem_wb_valid && mem_wb_exception && mem_wb_ebreak;
    wire halt_pending = (id_ex_valid && id_ex_ebreak) ||
                        (ex_mem_valid && ex_mem_ebreak) ||
                        (mem_wb_valid && mem_wb_ebreak);
    wire store_data_stall = if_id_valid && dec_mem_write &&
                            (((dec_has_rs1 && dec_rs1 != 4'd0) &&
                              ((id_ex_valid && id_ex_reg_write && id_ex_rd == dec_rs1) ||
                               (ex_mem_valid && ex_mem_reg_write && ex_mem_rd == dec_rs1))) ||
                             ((dec_has_rs2 && dec_rs2 != 4'd0) &&
                              ((id_ex_valid && id_ex_reg_write && id_ex_rd == dec_rs2) ||
                               (ex_mem_valid && ex_mem_reg_write && ex_mem_rd == dec_rs2))));
    wire store_busy = (id_ex_valid && id_ex_mem_write) || (ex_mem_valid && ex_mem_mem_write) ||
                      (mem_wb_valid && mem_wb_inst[6:0] == 7'b0100011);
    wire store_load_stall = if_id_valid && dec_mem_read && store_busy;
    wire csr_busy = (id_ex_valid && id_ex_csr_op != `CSR_OP_NONE) ||
                    (ex_mem_valid && ex_mem_csr_op != `CSR_OP_NONE) ||
                    (mem_wb_valid && mem_wb_csr_op != `CSR_OP_NONE);
    wire csr_stall = if_id_valid && csr_busy;
    wire id_stall = mem_access_wait || load_use_stall || store_data_stall || store_load_stall || csr_stall;

    wire [31:0] dec_rs1_data = (dec_rs1 != 4'd0 && reg_wen && reg_waddr == dec_rs1) ? reg_wdata : rs1_data;
    wire [31:0] dec_rs2_data = (dec_rs2 != 4'd0 && reg_wen && reg_waddr == dec_rs2) ? reg_wdata : rs2_data;

    wire if_id_accept = !if_id_valid || (!id_stall && !ex_redirect && !mem_redirect);
    wire fetch_can_start = !halted && !halt_pending && !wb_will_halt && !mem_access_wait && !fetch_pending && if_id_accept && !ex_redirect && !mem_redirect;
    assign req_fetch_valid = (fetch_pending && !halt_pending && !wb_will_halt) || fetch_can_start;
    assign req_fetch_addr = fetch_pending ? fetch_pc : pc;

    wire fetch_capture = resp_fetch_valid && if_id_accept && !ex_redirect && !wb_will_halt;

    //=========================================================================
    // Sequential pipeline control
    //=========================================================================
    always @(posedge clock or posedge reset) begin
        if (reset) begin
            pc <= `RESET_VECTOR;
            halted <= 1'b0;
            fetch_pending <= 1'b0;
            fetch_pc <= `RESET_VECTOR;
            if_id_valid <= 1'b0;
            id_ex_valid <= 1'b0;
            ex_mem_valid <= 1'b0;
            mem_wb_valid <= 1'b0;
        end else begin
            if (wb_will_halt) begin
                halted <= 1'b1;
                fetch_pending <= 1'b0;
                if_id_valid <= 1'b0;
                id_ex_valid <= 1'b0;
                ex_mem_valid <= 1'b0;
            end else if (mem_redirect) begin
                pc <= mtvec_reg;
                fetch_pending <= 1'b0;
                if_id_valid <= 1'b0;
                id_ex_valid <= 1'b0;
            end else if (ex_redirect) begin
                pc <= ex_next_pc;
                fetch_pending <= 1'b0;
                if_id_valid <= 1'b0;
                id_ex_valid <= 1'b0;
            end else begin
                if (fetch_can_start) begin
                    fetch_pending <= 1'b1;
                    fetch_pc <= pc;
                end
                if (fetch_capture) begin
                    if_id_valid <= 1'b1;
                    if_id_pc <= fetch_pending ? fetch_pc : pc;
                    if_id_inst <= resp_fetch_inst;
                    if_id_fetch_fault <= resp_fetch_fault;
                    if_id_fetch_misaligned <= resp_fetch_misaligned;
                    pc <= (fetch_pending ? fetch_pc : pc) + 32'd4;
                    fetch_pending <= 1'b0;
                end else if (!id_stall && if_id_valid) begin
                    if_id_valid <= 1'b0;
                end

                if (mem_access_wait) begin
                    id_ex_valid <= id_ex_valid;
                end else if (!id_stall) begin
                    id_ex_valid <= if_id_valid;
                    id_ex_pc <= if_id_pc;
                    id_ex_inst <= if_id_inst;
                    id_ex_rd <= dec_rd;
                    id_ex_rs1 <= dec_rs1;
                    id_ex_rs2 <= dec_rs2;
                    id_ex_rd5 <= dec_rd5;
                    id_ex_rs15 <= dec_rs15;
                    id_ex_rs25 <= dec_rs25;
                    id_ex_csr <= dec_csr;
                    id_ex_rs1_data <= dec_rs1_data;
                    id_ex_rs2_data <= dec_rs2_data;
                    id_ex_imm <= dec_imm;
                    id_ex_csr_data <= csr_read_data;
                    id_ex_csr_fault <= csr_read_fault;
                    id_ex_fetch_fault <= if_id_fetch_fault;
                    id_ex_fetch_misaligned <= if_id_fetch_misaligned;
                    id_ex_illegal <= dec_illegal;
                    id_ex_ecall <= dec_ecall;
                    id_ex_ebreak <= dec_ebreak;
                    id_ex_mret <= dec_mret;
                    id_ex_fence_i <= dec_fence_i;
                    id_ex_alu_op <= dec_alu_op;
                    id_ex_alu_src_a <= dec_alu_src_a;
                    id_ex_alu_src_b <= dec_alu_src_b;
                    id_ex_reg_write <= dec_reg_write;
                    id_ex_mem_read <= dec_mem_read;
                    id_ex_mem_write <= dec_mem_write;
                    id_ex_mem_size <= dec_mem_size;
                    id_ex_mem_sext <= dec_mem_sext;
                    id_ex_branch_type <= dec_branch_type;
                    id_ex_jump <= dec_jump;
                    id_ex_jump_reg <= dec_jump_reg;
                    id_ex_csr_op <= dec_csr_op;
                    id_ex_csr_src <= dec_csr_src;
                    id_ex_has_rd <= dec_has_rd;
                    id_ex_has_rs1 <= dec_has_rs1;
                    id_ex_has_rs2 <= dec_has_rs2;
                end else begin
                    id_ex_valid <= 1'b0;
                end
            end

            if (wb_will_halt) begin
                ex_mem_valid <= 1'b0;
                mem_wb_valid <= 1'b0;
            end else if (mem_redirect) begin
                ex_mem_valid <= 1'b0;
                mem_wb_valid <= ex_mem_valid;
                mem_wb_pc <= ex_mem_pc;
                mem_wb_inst <= ex_mem_inst;
                mem_wb_rd <= ex_mem_rd;
                mem_wb_csr <= ex_mem_csr;
                mem_wb_rd_value <= mem_stage_rd_value;
                mem_wb_csr_new_value <= ex_mem_csr_new_value;
                mem_wb_csr_do_write <= ex_mem_csr_do_write;
                mem_wb_reg_write <= ex_mem_reg_write;
                mem_wb_csr_op <= ex_mem_csr_op;
                mem_wb_fence_i <= ex_mem_fence_i;
                mem_wb_exception <= mem_exception;
                mem_wb_cause <= mem_cause;
                mem_wb_next_pc <= mtvec_reg;
                mem_wb_ebreak <= ex_mem_ebreak;
            end else if (mem_access_wait) begin
                ex_mem_valid <= ex_mem_valid;
                mem_wb_valid <= 1'b0;
            end else begin
                ex_mem_valid <= id_ex_valid;
                ex_mem_pc <= id_ex_pc;
                ex_mem_inst <= id_ex_inst;
                ex_mem_rd <= id_ex_rd;
                ex_mem_csr <= id_ex_csr;
                ex_mem_alu_result <= ex_alu_result;
                ex_mem_rs2_data <= ex_rs2_value;
                ex_mem_csr_data <= id_ex_csr_data;
                ex_mem_csr_new_value <= ex_csr_new_value;
                ex_mem_csr_do_write <= ex_csr_do_write;
                ex_mem_reg_write <= id_ex_reg_write;
                ex_mem_mem_read <= id_ex_mem_read;
                ex_mem_mem_write <= id_ex_mem_write;
                ex_mem_mem_size <= id_ex_mem_size;
                ex_mem_mem_sext <= id_ex_mem_sext;
                ex_mem_csr_op <= id_ex_csr_op;
                ex_mem_jump <= id_ex_jump;
                ex_mem_fence_i <= id_ex_fence_i;
                ex_mem_exception <= ex_exception;
                ex_mem_cause <= ex_cause;
                ex_mem_next_pc <= ex_next_pc;
                ex_mem_ebreak <= id_ex_ebreak;

                mem_wb_valid <= ex_mem_valid;
                mem_wb_pc <= ex_mem_pc;
                mem_wb_inst <= ex_mem_inst;
                mem_wb_rd <= ex_mem_rd;
                mem_wb_csr <= ex_mem_csr;
                mem_wb_rd_value <= mem_stage_rd_value;
                mem_wb_csr_new_value <= ex_mem_csr_new_value;
                mem_wb_csr_do_write <= ex_mem_csr_do_write;
                mem_wb_reg_write <= ex_mem_reg_write;
                mem_wb_csr_op <= ex_mem_csr_op;
                mem_wb_fence_i <= ex_mem_fence_i;
                mem_wb_exception <= mem_exception;
                mem_wb_cause <= mem_cause;
                mem_wb_next_pc <= mem_new_exception ? mtvec_reg : ex_mem_next_pc;
                mem_wb_ebreak <= ex_mem_ebreak;
            end
        end
    end

    //=========================================================================
    // Writeback and commit interface
    //=========================================================================
    always @(*) begin
        reg_wen = mem_wb_valid && mem_wb_reg_write && !mem_wb_exception && (mem_wb_rd != 4'd0);
        reg_waddr = mem_wb_rd;
        reg_wdata = mem_wb_rd_value;

        csr_wen = mem_wb_valid && (mem_wb_csr_op != `CSR_OP_NONE) && !mem_wb_exception && mem_wb_csr_do_write;
        csr_waddr = mem_wb_csr;
        csr_wdata = mem_wb_csr_new_value;

        csr_exc_en = mem_wb_valid && mem_wb_exception;
        csr_exc_mepc = mem_wb_pc;
        csr_exc_mcause = mem_wb_cause;
    end

    assign commit_valid = mem_wb_valid;
    assign commit_pc = mem_wb_pc;
    assign commit_inst = mem_wb_inst;
    assign commit_rd = reg_wen ? {1'b0, mem_wb_rd} : 5'b0;
    assign commit_rd_value = reg_wen ? mem_wb_rd_value : 32'h0;
    assign commit_exception = mem_wb_exception;
    assign commit_cause = mem_wb_cause;
    assign commit_next_pc = mem_wb_next_pc;
    assign fence_i_commit = mem_wb_valid && mem_wb_fence_i && !mem_wb_exception;

    assign debug_pc = if_id_valid ? if_id_pc : (fetch_pending ? fetch_pc : pc);
    assign debug_inst = if_id_valid ? if_id_inst : 32'h00000013;

endmodule
