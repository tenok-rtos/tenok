-include ../config.mk

CFLAGS+=-D STM32F40_41xxx \
	-D ENABLE_UART3_DMA=0
CFLAGS+=-Wl,-T,platform/stm32f407.ld

CFLAGS+=-D__BOARD_NAME__=\"stm32f407\"

CFLAGS+=-I./drivers/stm32f407

SRC+=./drivers/stm32f407/gpio.c \
	./drivers/stm32f407/uart.c \
        ./drivers/stm32f407/bsp_drv.c

qemu: all
	$(QEMU) -cpu cortex-m4 \
	-M netduinoplus2 \
	-serial /dev/null \
	-serial /dev/null \
	-serial stdio \
	-gdb tcp::3333 \
	-kernel ./$(ELF)

.PHONY: qemu
