CROSS_COMPILE :=
COMMON_CFLAGS := --target=riscv32 -march=rv32e -mabi=ilp32e -mno-relax -static -fno-pic -mstrict-align -ffreestanding
CFLAGS        += $(COMMON_CFLAGS)
ASFLAGS       += $(COMMON_CFLAGS)
LDFLAGS       += -melf32lriscv

# overwrite ARCH_H defined in $(AM_HOME)/Makefile
ARCH_H := arch/riscv.h
