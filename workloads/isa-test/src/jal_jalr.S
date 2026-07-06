.section .text
.globl _start
.include "workloads/isa-test/common/test_macros.S"

_start:
    SETUP_UART

    # jal forward: link value must be PC+4 of the jal instruction.
    jal x4, jal_forward
jal_forward_ret:
    j fail                  # should be skipped
jal_forward:
    la x14, jal_forward_ret
    bne x4, x14, fail

    # jal backward
    j jal_backward_entry
jal_backward_entry:
    jal x4, jal_backward
jal_backward_ret:
    j after_backward
jal_backward:
    la x14, jal_backward_ret
    bne x4, x14, fail
    j after_backward
after_backward:

    # jalr: target = (rs1 + imm) & ~1.  rs1 is deliberately odd.
    la x5, jalr_target
    ori x5, x5, 1           # make it odd; bit 0 should be cleared by jalr
    jalr x6, 0(x5)
jalr_ret:
    j fail
jalr_target:
    la x14, jalr_ret
    bne x6, x14, fail

    # jalr with non-zero immediate
    la x5, jalr_imm_target
    addi x5, x5, -4
    jalr x6, 4(x5)
jalr_imm_ret:
    j fail
jalr_imm_target:
    la x14, jalr_imm_ret
    bne x6, x14, fail

    # jalr link to x0 (no link write)
    la x5, jalr_x0_target
    jalr x0, 0(x5)
    j fail
jalr_x0_target:
    CHECK_EQ x0, 0

    PASS

fail:
    FAIL
