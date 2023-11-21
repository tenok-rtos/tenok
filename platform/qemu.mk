-include ../config.mk

LD_SCRIPT += platform/stm32f407.ld

CFLAGS += -D STM32F40_41xxx \
	  -D ENABLE_UART1_DMA=0 \
	  -D ENABLE_UART3_DMA=0 \
	  -D BUILD_QEMU \
          -D __ARCH__=\"armv7m\"

CFLAGS += -D__BOARD_NAME__=\"stm32f407\"

CFLAGS += -I./drivers/boards

SRC += ./drivers/boards/stm32f4disc.c

# Some useful qemu debug options.
# Type `$(QEMU) -d help` for more information.
# QEMU_DEBUG = -d in_asm
# QEMU_DEBUG = -d int
# QEMU_DEBUG = -d cpu
# QEMU_DEBUG = -d guest_errors

qemu: all
	$(QEMU) \
        $(QEMU_DEBUG) \
	-nographic \
	-cpu cortex-m4 \
	-M netduinoplus2 \
	-serial mon:stdio \
	-serial pty \
	-serial pty \
	-gdb tcp::3333 \
	-kernel ./$(ELF)

.PHONY: qemu
