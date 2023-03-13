#STM32F4DISCOVERY board (https://www.st.com/en/evaluation-tools/stm32f4discovery.html)

CFLAGS+=-D STM32F40_41xxx \
	-D ENABLE_UART3_DMA=1

CFLAGS+=-Wl,-T,platform/stm32f407.ld

CFLAGS+=-D__BOARD_NAME__=\"stm32f407\"

CFLAGS+=-I./drivers/stm32f407

SRC+=./drivers/stm32f407/gpio.c \
	./drivers/stm32f407/uart.c \
	./drivers/stm32f407/bsp_drv.c

flash:
	openocd -f interface/stlink.cfg \
	-f target/stm32f4x.cfg \
	-c "init" \
	-c "reset init" \
	-c "halt" \
	-c "flash write_image erase $(ELF)" \
	-c "verify_image $(ELF)" \
	-c "reset run" -c shutdown

openocd:
	openocd -s /opt/openocd/share/openocd/scripts/ -f ./gdb/openocd.cfg

.PHONY: flash openocd
