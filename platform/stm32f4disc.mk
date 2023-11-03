#STM32F4DISCOVERY board (https://www.st.com/en/evaluation-tools/stm32f4discovery.html)

LD_SCRIPT += platform/stm32f407.ld

CFLAGS += -D STM32F40_41xxx \
	  -D ENABLE_UART1_DMA=1 \
	  -D ENABLE_UART3_DMA=1

CFLAGS += -Wl,-T,platform/stm32f407.ld

CFLAGS += -D__BOARD_NAME__=\"stm32f407\"

CFLAGS += -I./drivers/boards

SRC += ./drivers/boards/stm32f4disc.c

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
