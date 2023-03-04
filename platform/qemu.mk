-include ../config.mk

CFLAGS+=-D STM32F40_41xxx
CFLAGS+=-Wl,-T,platform/stm32f407.ld

all:

qemu: all
	$(QEMU) -cpu cortex-m4 \
	-M netduinoplus2 \
	-serial /dev/null \
	-serial /dev/null \
	-serial stdio \
	-gdb tcp::3333 \
	-kernel ./$(ELF)

.PHONY: all qemu
