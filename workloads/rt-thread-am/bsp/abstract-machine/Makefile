ARCH ?= riscv32e-npc
AM_APPS_MK = build/${ARCH}/am-apps.mk
FILE_MK = files.mk
FILE_TMP = .tmp.$(FILE_MK)
RTCONFIG_H = rtconfig.h

NAME = rtthread
SRCS = $(shell find src -name "*.c")
CFLAGS += -DHAVE_CCONFIG_H -D__RTTHREAD__ -DRT_USING_LIBC
# BSP include directory must precede AM/RT-Thread headers so we can supply
# missing declarations (string.h, unistd.h, fcntl.h, etc.).
CFLAGS += -Iinclude
-include $(FILE_MK)
ifneq ($(wildcard $(AM_APPS_MK)),)
  -include $(AM_APPS_MK)
endif
include $(AM_HOME)/Makefile
# extra.ld must be added after the AM Makefile, because the arch-specific
# config (scripts/riscv32e-npc.mk) resets LDFLAGS with :=.
LDSCRIPTS += extra.ld
LDFLAGS += -T extra.ld

# DFS needs the libc extension headers (e.g. sys/stat.h).
CFLAGS += -I../../components/libc/compilers/common/extension
$(RTCONFIG_H): .config
	touch $@
	scons --useconfig=.config

init: $(RTCONFIG_H)
	scons -c
	scons --verbose -n > $(FILE_TMP)
	cat $(FILE_TMP) | grep -o '[^ ]*.c$$' | awk '{print "SRCS += " $$1}' > $(FILE_MK)
	cat $(FILE_TMP) | grep 'gcc ' | head -n 1 | grep -o " -I[^ ]*" | awk '{print "CFLAGS += " $$1}' >> $(FILE_MK)
	rm $(FILE_TMP)

menuconfig:
	scons --menuconfig
	scons --genconfig
	$(MAKE) init

ifneq ($(MAKECMDGOALS),clean)
update: $(AM_APPS_MK)
$(AM_APPS_MK): integrate-am-apps.py
	mkdir -p $(@D)
	python3 $^ ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
endif

.PHONY: init menuconfig update
