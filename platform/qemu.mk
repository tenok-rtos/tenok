-include ../config.mk

CFLAGS+=-D STM32F40_41xxx \
	-D ENABLE_UART3_DMA=0
CFLAGS+=-Wl,-T,platform/stm32f407.ld

CFLAGS+=-D__BOARD_NAME__=\"stm32f407\"

CFLAGS+=-I./drivers/boards

SRC+=./drivers/boards/stm32f4disc.c

qemu: all
	$(QEMU) -cpu cortex-m4 \
	-M netduinoplus2 \
	-serial pty \
	-serial pty \
	-serial stdio \
	-gdb tcp::3333 \
	-kernel ./$(ELF)

.PHONY: qemu
