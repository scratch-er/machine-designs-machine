include $(AM_HOME)/scripts/isa/riscv.mk
include $(AM_HOME)/scripts/platform/npc.mk
COMMON_CFLAGS := --target=riscv32 -march=rv32e -mabi=ilp32e -mno-relax -static -fno-pic -mstrict-align -ffreestanding
LDFLAGS       := -melf32lriscv

# Re-apply COMMON_CFLAGS to CFLAGS/ASFLAGS since we just overwrote it.
CFLAGS  += $(COMMON_CFLAGS)
ASFLAGS += $(COMMON_CFLAGS)

AM_SRCS += riscv/npc/libgcc/div.S \
           riscv/npc/libgcc/muldi3.S \
           riscv/npc/libgcc/multi3.c \
           riscv/npc/libgcc/ashldi3.c \
           riscv/npc/libgcc/unused.c
