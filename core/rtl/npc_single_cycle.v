// Single-cycle RV32E_Zicsr datapath.
//
// This module is the first implementation milestone. It retires one
// instruction per cycle using a DPI-C memory backend. The AXI4 master is not
// used in this baseline; it will be wired up in the pipelined design.

`include "npc_defines.vh"

module npc_single_cycle (
    input              clock,
    input              reset,

    // Instruction fetch interface (combinational read)
    output reg         req_fetch_valid,
    output reg [31:0]  req_fetch_addr,
    input      [31:0]  resp_fetch_inst,
    input              resp_fetch_fault,
    input              resp_fetch_misaligned,

    // Data load interface (combinational read)
    output reg         req_load_valid,
    output reg [31:0]  req_load_addr,
    output reg [1:0]   req_load_size,
    input      [31:0]  resp_load_data,
    input              resp_load_fault,
    input              resp_load_misaligned,

    // Data store interface (registered write)
    output reg         req_store_valid,
    output reg [31:0]  req_store_addr,
    output reg [1:0]   req_store_size,
    output reg [31:0]  req_store_data,
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
    // Instruction fields
    //==========================================================================
    wire [31:0] inst = resp_fetch_inst;
    wire [6:0]  opcode = inst[6:0];
    wire [2:0]  funct3 = inst[14:12];
    wire [6:0]  funct7 = inst[31:25];
    wire [11:0] funct12 = inst[31:20];
    wire [4:0]  rd5  = inst[11:7];
    wire [4:0]  rs15 = inst[19:15];
    wire [4:0]  rs25 = inst[24:20];
    wire [3:0]  inst_rd  = rd5[3:0];
    wire [3:0]  inst_rs1 = rs15[3:0];
    wire [3:0]  inst_rs2 = rs25[3:0];
    wire [11:0] inst_csr = inst[31:20];

    //==========================================================================
    // Decoder
    //==========================================================================
    reg         ctrl_illegal;
    reg         ctrl_ecall;
    reg         ctrl_ebreak;
    reg         ctrl_mret;
    reg         ctrl_wfi;
    reg         ctrl_fence_i;
    reg  [3:0]  ctrl_alu_op;
    reg  [1:0]  ctrl_alu_src_a;
    reg         ctrl_alu_src_b;
    reg  [2:0]  ctrl_imm_sel;
    reg         ctrl_reg_write;
    reg         ctrl_mem_read;
    reg         ctrl_mem_write;
    reg  [1:0]  ctrl_mem_size;
    reg         ctrl_mem_sext;
    reg  [2:0]  ctrl_branch_type;
    reg         ctrl_jump;
    reg         ctrl_jump_reg;
    reg  [2:0]  ctrl_csr_op;
    reg         ctrl_csr_src;

    reg dec_has_rd;
    reg dec_has_rs1;
    reg dec_has_rs2;

    always @(*) begin
        // Defaults
        ctrl_illegal    = 1'b0;
        ctrl_ecall      = 1'b0;
        ctrl_ebreak     = 1'b0;
        ctrl_mret       = 1'b0;
        ctrl_wfi        = 1'b0;
        ctrl_fence_i    = 1'b0;
        ctrl_alu_op     = `ALU_ADD;
        ctrl_alu_src_a  = `ALU_A_RS1;
        ctrl_alu_src_b  = `ALU_B_IMM;
        ctrl_imm_sel    = `IMM_I;
        ctrl_reg_write  = 1'b0;
        ctrl_mem_read   = 1'b0;
        ctrl_mem_write  = 1'b0;
        ctrl_mem_size   = `MEM_SIZE_BYTE;
        ctrl_mem_sext   = 1'b0;
        ctrl_branch_type= `BR_NONE;
        ctrl_jump       = 1'b0;
        ctrl_jump_reg   = 1'b0;
        ctrl_csr_op     = `CSR_OP_NONE;
        ctrl_csr_src    = `CSR_SRC_RS1;
        dec_has_rd      = 1'b0;
        dec_has_rs1     = 1'b0;
        dec_has_rs2     = 1'b0;

        case (opcode)
            7'b0010011: begin // OP-IMM
                dec_has_rd  = 1'b1;
                dec_has_rs1 = 1'b1;
                ctrl_reg_write = 1'b1;
                case (funct3)
                    3'b000: ctrl_alu_op = `ALU_ADD;
                    3'b010: ctrl_alu_op = `ALU_SLT;
                    3'b011: ctrl_alu_op = `ALU_SLTU;
                    3'b100: ctrl_alu_op = `ALU_XOR;
                    3'b110: ctrl_alu_op = `ALU_OR;
                    3'b111: ctrl_alu_op = `ALU_AND;
                    3'b001: ctrl_alu_op = (funct7 == 7'b0000000) ? `ALU_SLL : `ALU_ADD;
                    3'b101: begin
                        if (funct7 == 7'b0000000) ctrl_alu_op = `ALU_SRL;
                        else if (funct7 == 7'b0100000) ctrl_alu_op = `ALU_SRA;
                        else ctrl_illegal = 1'b1;
                    end
                    default: ctrl_illegal = 1'b1;
                endcase
                if (funct3 == 3'b001 && funct7 != 7'b0000000) ctrl_illegal = 1'b1;
            end

            7'b0110011: begin // OP
                dec_has_rd  = 1'b1;
                dec_has_rs1 = 1'b1;
                dec_has_rs2 = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_alu_src_b = `ALU_B_RS2;
                case (funct3)
                    3'b000: begin
                        if (funct7 == 7'b0000000) ctrl_alu_op = `ALU_ADD;
                        else if (funct7 == 7'b0100000) ctrl_alu_op = `ALU_SUB;
                        else ctrl_illegal = 1'b1;
                    end
                    3'b001: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b010: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b011: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b100: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b101: begin
                        if (funct7 == 7'b0000000) ctrl_alu_op = `ALU_SRL;
                        else if (funct7 == 7'b0100000) ctrl_alu_op = `ALU_SRA;
                        else ctrl_illegal = 1'b1;
                    end
                    3'b110: ctrl_illegal = (funct7 != 7'b0000000);
                    3'b111: ctrl_illegal = (funct7 != 7'b0000000);
                    default: ctrl_illegal = 1'b1;
                endcase
                if (!ctrl_illegal) begin
                    case (funct3)
                        3'b001: ctrl_alu_op = `ALU_SLL;
                        3'b010: ctrl_alu_op = `ALU_SLT;
                        3'b011: ctrl_alu_op = `ALU_SLTU;
                        3'b100: ctrl_alu_op = `ALU_XOR;
                        3'b110: ctrl_alu_op = `ALU_OR;
                        3'b111: ctrl_alu_op = `ALU_AND;
                        default: ;
                    endcase
                end
            end

            7'b0110111: begin // LUI
                dec_has_rd = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_alu_src_a = `ALU_A_ZERO;
                ctrl_imm_sel = `IMM_U;
            end

            7'b0010111: begin // AUIPC
                dec_has_rd = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_alu_src_a = `ALU_A_PC;
                ctrl_imm_sel = `IMM_U;
            end

            7'b1101111: begin // JAL
                dec_has_rd = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_alu_src_a = `ALU_A_PC;
                ctrl_imm_sel = `IMM_J;
                ctrl_jump = 1'b1;
            end

            7'b1100111: begin // JALR
                if (funct3 == 3'b000) begin
                    dec_has_rd = 1'b1;
                    dec_has_rs1 = 1'b1;
                    ctrl_reg_write = 1'b1;
                    ctrl_alu_src_a = `ALU_A_RS1;
                    ctrl_imm_sel = `IMM_I;
                    ctrl_jump = 1'b1;
                    ctrl_jump_reg = 1'b1;
                end else begin
                    ctrl_illegal = 1'b1;
                end
            end

            7'b1100011: begin // BRANCH
                dec_has_rs1 = 1'b1;
                dec_has_rs2 = 1'b1;
                ctrl_alu_src_b = `ALU_B_RS2;
                ctrl_imm_sel = `IMM_B;
                case (funct3)
                    3'b000: ctrl_branch_type = `BR_BEQ;
                    3'b001: ctrl_branch_type = `BR_BNE;
                    3'b100: ctrl_branch_type = `BR_BLT;
                    3'b101: ctrl_branch_type = `BR_BGE;
                    3'b110: ctrl_branch_type = `BR_BLTU;
                    3'b111: ctrl_branch_type = `BR_BGEU;
                    default: ctrl_illegal = 1'b1;
                endcase
            end

            7'b0000011: begin // LOAD
                dec_has_rd = 1'b1;
                dec_has_rs1 = 1'b1;
                ctrl_reg_write = 1'b1;
                ctrl_mem_read = 1'b1;
                ctrl_mem_size = funct3[1:0];
                ctrl_mem_sext = ~funct3[2];
                if (funct3[1:0] == 2'b11 || funct3 == 3'b010 && funct3[2]) ctrl_illegal = 1'b1;
                if (funct3 == 3'b110 || funct3 == 3'b111) ctrl_illegal = 1'b1;
            end

            7'b0100011: begin // STORE
                dec_has_rs1 = 1'b1;
                dec_has_rs2 = 1'b1;
                ctrl_mem_write = 1'b1;
                ctrl_mem_size = funct3[1:0];
                ctrl_imm_sel = `IMM_S;
                if (funct3[2] == 1'b1 || funct3[1:0] == 2'b11) ctrl_illegal = 1'b1;
            end

            7'b0001111: begin // MISC-MEM
                if (funct3 == 3'b000) begin
                    // fence: nop
                end else if (funct3 == 3'b001 && inst[31:15] == 17'h00000) begin
                    ctrl_fence_i = 1'b1;
                end else begin
                    ctrl_illegal = 1'b1;
                end
            end

            7'b1110011: begin // SYSTEM
                if (funct3 == 3'b000) begin
                    case (funct12)
                        12'h000: ctrl_ecall = 1'b1;
                        12'h001: ctrl_ebreak = 1'b1;
                        12'h302: ctrl_mret = 1'b1;
                        12'h105: ctrl_wfi = 1'b1;
                        default: ctrl_illegal = 1'b1;
                    endcase
                end else begin
                    dec_has_rd = 1'b1;
                    ctrl_reg_write = 1'b1;
                    ctrl_csr_src = funct3[2];
                    case (funct3)
                        3'b001: begin ctrl_csr_op = `CSR_OP_RW;  dec_has_rs1 = 1'b1; end
                        3'b010: begin ctrl_csr_op = `CSR_OP_RS;  dec_has_rs1 = 1'b1; end
                        3'b011: begin ctrl_csr_op = `CSR_OP_RC;  dec_has_rs1 = 1'b1; end
                        3'b101: begin ctrl_csr_op = `CSR_OP_RWI; ctrl_imm_sel = `IMM_Z; end
                        3'b110: begin ctrl_csr_op = `CSR_OP_RSI; ctrl_imm_sel = `IMM_Z; end
                        3'b111: begin ctrl_csr_op = `CSR_OP_RCI; ctrl_imm_sel = `IMM_Z; end
                        default: ctrl_illegal = 1'b1;
                    endcase
                end
            end

            default: ctrl_illegal = 1'b1;
        endcase
    end

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
        .exc_en     (exc_take),
        .exc_mepc   (pc),
        .exc_mcause (exc_cause),
        .mtvec      (mtvec_reg),
        .mepc       (mepc_reg)
    );

    //==========================================================================
    // Immediate generator
    //==========================================================================
    reg [31:0] imm;
    always @(*) begin
        case (ctrl_imm_sel)
            `IMM_I: imm = {{20{inst[31]}}, inst[31:20]};
            `IMM_S: imm = {{20{inst[31]}}, inst[31:25], inst[11:7]};
            `IMM_B: imm = {{19{inst[31]}}, inst[31], inst[7], inst[30:25], inst[11:8], 1'b0};
            `IMM_U: imm = {inst[31:12], 12'b0};
            `IMM_J: imm = {{11{inst[31]}}, inst[31], inst[19:12], inst[20], inst[30:21], 1'b0};
            `IMM_Z: imm = {27'b0, inst[19:15]};
            default: imm = 32'h0;
        endcase
    end

    //==========================================================================
    // ALU
    //==========================================================================
    reg [31:0] alu_op1;
    reg [31:0] alu_op2;
    reg [31:0] alu_result;

    always @(*) begin
        case (ctrl_alu_src_a)
            `ALU_A_RS1:  alu_op1 = rs1_data;
            `ALU_A_PC:   alu_op1 = pc;
            `ALU_A_ZERO: alu_op1 = 32'h0;
            default:     alu_op1 = 32'h0;
        endcase

        case (ctrl_alu_src_b)
            `ALU_B_RS2: alu_op2 = rs2_data;
            `ALU_B_IMM: alu_op2 = imm;
            default:    alu_op2 = imm;
        endcase

        case (ctrl_alu_op)
            `ALU_ADD:  alu_result = alu_op1 + alu_op2;
            `ALU_SUB:  alu_result = alu_op1 - alu_op2;
            `ALU_SLT:  alu_result = ($signed(alu_op1) < $signed(alu_op2)) ? 32'h1 : 32'h0;
            `ALU_SLTU: alu_result = (alu_op1 < alu_op2) ? 32'h1 : 32'h0;
            `ALU_XOR:  alu_result = alu_op1 ^ alu_op2;
            `ALU_OR:   alu_result = alu_op1 | alu_op2;
            `ALU_AND:  alu_result = alu_op1 & alu_op2;
            `ALU_SLL:  alu_result = alu_op1 << alu_op2[4:0];
            `ALU_SRL:  alu_result = alu_op1 >> alu_op2[4:0];
            `ALU_SRA:  alu_result = $signed(alu_op1) >>> alu_op2[4:0];
            default:   alu_result = 32'h0;
        endcase
    end

    //==========================================================================
    // Branch / jump target and condition
    //==========================================================================
    reg        branch_taken;
    reg [31:0] branch_target;
    reg [31:0] jump_target;
    reg        jump_target_misaligned;

    always @(*) begin
        branch_target = pc + imm;
        jump_target = ctrl_jump_reg ? ((rs1_data + imm) & 32'hFFFF_FFFE) : (pc + imm);
        jump_target_misaligned = (jump_target[1:0] != 2'b00);

        case (ctrl_branch_type)
            `BR_BEQ:  branch_taken = (rs1_data == rs2_data);
            `BR_BNE:  branch_taken = (rs1_data != rs2_data);
            `BR_BLT:  branch_taken = ($signed(rs1_data) < $signed(rs2_data));
            `BR_BGE:  branch_taken = ($signed(rs1_data) >= $signed(rs2_data));
            `BR_BLTU: branch_taken = (rs1_data < rs2_data);
            `BR_BGEU: branch_taken = (rs1_data >= rs2_data);
            default:  branch_taken = 1'b0;
        endcase
    end

    wire [31:0] branch_final_target = branch_taken ? branch_target : pc + 32'd4;
    wire        branch_target_misaligned = branch_taken && (branch_target[1:0] != 2'b00);

    //==========================================================================
    // Memory access
    //==========================================================================
    reg [31:0] mem_addr;
    reg [31:0] store_data;
    reg [31:0] load_aligned_data;
    reg [31:0] load_final_data;
    reg        load_misaligned;
    reg        store_misaligned;
    reg        clint_req_valid;
    reg        clint_req_wen;

    wire        clint_is_access = (mem_addr >= `CLINT_BASE) &&
                                  (mem_addr < `CLINT_BASE + `CLINT_SIZE);
    wire [31:0] clint_rdata;
    wire        clint_fault = 1'b0;

    npc_clint clint (
        .clock      (clock),
        .reset      (reset),
        .req_valid  (clint_req_valid),
        .req_wen    (clint_req_wen),
        .req_addr   (mem_addr),
        .req_wdata  (store_data),
        .resp_rdata (clint_rdata),
        .resp_fault (clint_fault)
    );

    always @(*) begin
        mem_addr = alu_result;
        store_data = rs2_data;

        // Alignment checks
        case (ctrl_mem_size)
            `MEM_SIZE_BYTE: begin load_misaligned = 1'b0; store_misaligned = 1'b0; end
            `MEM_SIZE_HALF: begin
                load_misaligned = ctrl_mem_read && (mem_addr[0] != 1'b0);
                store_misaligned = ctrl_mem_write && (mem_addr[0] != 1'b0);
            end
            `MEM_SIZE_WORD: begin
                load_misaligned = ctrl_mem_read && (mem_addr[1:0] != 2'b00);
                store_misaligned = ctrl_mem_write && (mem_addr[1:0] != 2'b00);
            end
            default: begin load_misaligned = 1'b0; store_misaligned = 1'b0; end
        endcase

        // Load alignment / sign extension
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

        // Choose CLINT or memory for load data
        load_final_data = clint_is_access ? clint_rdata : load_aligned_data;

        // Memory request signals
        req_fetch_valid = 1'b1;
        req_fetch_addr  = pc;
        req_load_valid  = ctrl_mem_read && !clint_is_access;
        req_load_addr   = mem_addr;
        req_load_size   = ctrl_mem_size;
        req_store_valid = ctrl_mem_write && !clint_is_access;
        req_store_addr  = mem_addr;
        req_store_size  = ctrl_mem_size;
        req_store_data  = store_data;

        // CLINT request
        clint_req_valid = (ctrl_mem_read || ctrl_mem_write) && clint_is_access;
        clint_req_wen   = ctrl_mem_write;
    end

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
        if (pc[1:0] != 2'b00) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_INST_MISALIGNED;
        end else if (resp_fetch_fault) begin
            exc_take = 1'b1;
            exc_cause = CAUSE_INST_ACCESS_FAULT;
        end else if (resp_fetch_misaligned) begin
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
        reg_wen   = ctrl_reg_write && !exc_take && (inst_rd != 4'd0);
        reg_waddr = inst_rd;
        reg_wdata = rd_value;

        csr_wen   = (ctrl_csr_op != `CSR_OP_NONE) && !exc_take && csr_do_write;
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
            pc              <= `RESET_VECTOR;
            halted          <= 1'b0;
            commit_valid_r  <= 1'b0;
        end else if (!halted) begin
            // Capture the instruction that retires this cycle. The PC register
            // will be updated to next_pc, so we must record the current value
            // here for the commit interface.
            commit_valid_r     <= 1'b1;
            commit_pc_r        <= pc;
            commit_inst_r      <= inst;
            commit_rd_r        <= reg_wen ? {1'b0, inst_rd} : 5'b0;
            commit_rd_value_r  <= reg_wen ? rd_value : 32'h0;
            commit_exception_r <= exc_take;
            commit_cause_r     <= exc_cause;
            commit_next_pc_r   <= next_pc;

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
    assign debug_pc   = pc;
    assign debug_inst = inst;

endmodule
